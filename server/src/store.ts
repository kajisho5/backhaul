// The database (Cloudflare D1). All SQL of the service is in this file.
import type { Licence } from "./licence";

export interface Device {
  row: number;
  licenceId: number;
  deviceId: string;
  name: string;
  lastSeenAt: number;
}

export interface Login {
  state: string;
  deviceRow: number;
  challenge: string;
  code: string | null;
  denied: boolean;
  createdAt: number;
}

interface LicenceRow {
  id: number;
  key_tail: string;
  note: string;
  seats: number;
  expires_at: number | null;
  revoked: number;
  created_at: number;
}

function toLicence(r: LicenceRow): Licence {
  return {
    id: r.id,
    keyTail: r.key_tail,
    note: r.note,
    seats: r.seats,
    expiresAt: r.expires_at,
    revoked: r.revoked !== 0,
    createdAt: r.created_at,
  };
}

interface DeviceRow {
  id: number;
  licence_id: number;
  device_id: string;
  name: string;
  last_seen_at: number;
}

function toDevice(r: DeviceRow): Device {
  return { row: r.id, licenceId: r.licence_id, deviceId: r.device_id, name: r.name, lastSeenAt: r.last_seen_at };
}

export interface Signin {
  refreshCipher: string;
  accessCipher: string | null;
  accessExpiresAt: number;
  zakCipher: string | null;
  zakAt: number;
}

/** A Stripe subscription and its licence. */
export interface Sale {
  stripeId: string;
  licenceId: number;
  status: string;
  keyCipher: string | null;
  mailedAt: number | null;
  shownAt: number | null;
}

interface SaleRow {
  stripe_id: string;
  licence_id: number;
  status: string;
  key_cipher: string | null;
  mailed_at: number | null;
  shown_at: number | null;
}

function toSale(r: SaleRow): Sale {
  return { stripeId: r.stripe_id, licenceId: r.licence_id, status: r.status, keyCipher: r.key_cipher, mailedAt: r.mailed_at, shownAt: r.shown_at };
}

export class Store {
  constructor(private readonly db: D1Database) {}

  async createLicence(keyHash: string, keyTail: string, note: string, seats: number, expiresAt: number | null, now: number) {
    const row = await this.db
      .prepare("INSERT INTO licences (key_hash, key_tail, note, seats, expires_at, created_at) VALUES (?, ?, ?, ?, ?, ?) RETURNING id")
      .bind(keyHash, keyTail, note, seats, expiresAt, now)
      .first<{ id: number }>();
    if (row === null) throw new Error("the licence was not stored");
    return row.id;
  }

  async licenceByKeyHash(keyHash: string): Promise<Licence | null> {
    const row = await this.db.prepare("SELECT * FROM licences WHERE key_hash = ?").bind(keyHash).first<LicenceRow>();
    return row === null ? null : toLicence(row);
  }

  async licenceById(id: number): Promise<Licence | null> {
    const row = await this.db.prepare("SELECT * FROM licences WHERE id = ?").bind(id).first<LicenceRow>();
    return row === null ? null : toLicence(row);
  }

  async listLicences(): Promise<Licence[]> {
    const rows = await this.db.prepare("SELECT * FROM licences ORDER BY id").all<LicenceRow>();
    return rows.results.map(toLicence);
  }

  /** False when there is no such licence. */
  async setRevoked(id: number, revoked: boolean): Promise<boolean> {
    const done = await this.db.prepare("UPDATE licences SET revoked = ? WHERE id = ?").bind(revoked ? 1 : 0, id).run();
    return done.meta.changes > 0;
  }

  async devicesOf(licenceId: number): Promise<Device[]> {
    const rows = await this.db.prepare("SELECT * FROM devices WHERE licence_id = ? ORDER BY id").bind(licenceId).all<DeviceRow>();
    return rows.results.map(toDevice);
  }

  /**
   * Registers a PC, or gives a PC that is already registered a new token (its old one
   * stops working). Returns false when the licence has no free place for a new PC; the
   * count and the insert are one statement, so two PCs cannot take the last place together.
   */
  async registerDevice(licenceId: number, seats: number, deviceId: string, name: string, tokenHash: string, now: number) {
    const again = await this.db
      .prepare("UPDATE devices SET name = ?, token_hash = ?, last_seen_at = ? WHERE licence_id = ? AND device_id = ?")
      .bind(name, tokenHash, now, licenceId, deviceId)
      .run();
    if (again.meta.changes > 0) return true;
    const added = await this.db
      .prepare(
        `INSERT INTO devices (licence_id, device_id, name, token_hash, created_at, last_seen_at)
         SELECT ?, ?, ?, ?, ?, ? WHERE (SELECT COUNT(*) FROM devices WHERE licence_id = ?) < ?`,
      )
      .bind(licenceId, deviceId, name, tokenHash, now, now, licenceId, seats)
      .run();
    return added.meta.changes > 0;
  }

  async deviceByTokenHash(tokenHash: string): Promise<Device | null> {
    const row = await this.db.prepare("SELECT * FROM devices WHERE token_hash = ?").bind(tokenHash).first<DeviceRow>();
    return row === null ? null : toDevice(row);
  }

  async touchDevice(row: number, now: number) {
    await this.db.prepare("UPDATE devices SET last_seen_at = ? WHERE id = ?").bind(now, row).run();
  }

  async removeDevice(row: number) {
    await this.db.prepare("DELETE FROM logins WHERE device_row = ?").bind(row).run();
    await this.db.prepare("DELETE FROM devices WHERE id = ?").bind(row).run();
  }

  async startLogin(state: string, deviceRow: number, challenge: string, now: number) {
    await this.db.prepare("DELETE FROM logins WHERE device_row = ? OR created_at < ?").bind(deviceRow, now - 600).run();
    await this.db
      .prepare("INSERT INTO logins (state, device_row, challenge, created_at) VALUES (?, ?, ?, ?)")
      .bind(state, deviceRow, challenge, now)
      .run();
  }

  async loginByState(state: string, now: number): Promise<Login | null> {
    const row = await this.db
      .prepare("SELECT * FROM logins WHERE state = ? AND created_at >= ?")
      .bind(state, now - 600)
      .first<{ state: string; device_row: number; challenge: string; code: string | null; denied: number; created_at: number }>();
    if (row === null) return null;
    return {
      state: row.state,
      deviceRow: row.device_row,
      challenge: row.challenge,
      code: row.code,
      denied: row.denied !== 0,
      createdAt: row.created_at,
    };
  }

  async answerLogin(state: string, code: string | null) {
    await this.db
      .prepare("UPDATE logins SET code = ?, denied = ? WHERE state = ? AND code IS NULL AND denied = 0")
      .bind(code, code === null ? 1 : 0, state)
      .run();
  }

  async endLogin(state: string) {
    await this.db.prepare("DELETE FROM logins WHERE state = ?").bind(state).run();
  }

  /** Counts one request in `bucket`. False when that is more than `limit` within the window. */
  async allow(bucket: string, limit: number, windowSeconds: number, now: number): Promise<boolean> {
    const window = Math.floor(now / windowSeconds);
    const key = `${bucket}:${window}`;
    const row = await this.db
      .prepare(
        `INSERT INTO attempts (bucket, count, expires_at) VALUES (?, 1, ?)
         ON CONFLICT (bucket) DO UPDATE SET count = count + 1 RETURNING count`,
      )
      .bind(key, (window + 2) * windowSeconds)
      .first<{ count: number }>();
    if (window % 16 === 0) await this.db.prepare("DELETE FROM attempts WHERE expires_at < ?").bind(now).run();
    return row !== null && row.count <= limit;
  }

  /** True when `bucket` already holds `limit` counted requests in this window. Counts nothing itself. */
  async over(bucket: string, limit: number, windowSeconds: number, now: number): Promise<boolean> {
    const key = `${bucket}:${Math.floor(now / windowSeconds)}`;
    const row = await this.db.prepare("SELECT count FROM attempts WHERE bucket = ?").bind(key).first<{ count: number }>();
    return row !== null && row.count >= limit;
  }

  // ---- the licence's Zoom sign-in ----

  async signinOf(licenceId: number): Promise<Signin | null> {
    const row = await this.db
      .prepare("SELECT * FROM zoom_signins WHERE licence_id = ?")
      .bind(licenceId)
      .first<{ refresh_cipher: string; access_cipher: string | null; access_expires_at: number; zak_cipher: string | null; zak_at: number }>();
    if (row === null) return null;
    return {
      refreshCipher: row.refresh_cipher,
      accessCipher: row.access_cipher,
      accessExpiresAt: row.access_expires_at,
      zakCipher: row.zak_cipher,
      zakAt: row.zak_at,
    };
  }

  /** A new sign-in replaces the old one, and whatever was remembered from it. */
  async saveSignin(licenceId: number, refreshCipher: string, accessCipher: string | null, accessExpiresAt: number, now: number) {
    await this.db
      .prepare(
        `INSERT INTO zoom_signins (licence_id, refresh_cipher, access_cipher, access_expires_at, updated_at) VALUES (?, ?, ?, ?, ?)
         ON CONFLICT (licence_id) DO UPDATE SET refresh_cipher = excluded.refresh_cipher, access_cipher = excluded.access_cipher,
           access_expires_at = excluded.access_expires_at, zak_cipher = NULL, zak_at = 0, busy_until = 0, updated_at = excluded.updated_at`,
      )
      .bind(licenceId, refreshCipher, accessCipher, accessExpiresAt, now)
      .run();
  }

  /** After Zoom renewed the tokens (the old refresh token is dead from then on). The lock is kept. */
  async renewSignin(licenceId: number, refreshCipher: string, accessCipher: string, accessExpiresAt: number, now: number) {
    await this.db
      .prepare("UPDATE zoom_signins SET refresh_cipher = ?, access_cipher = ?, access_expires_at = ?, updated_at = ? WHERE licence_id = ?")
      .bind(refreshCipher, accessCipher, accessExpiresAt, now, licenceId)
      .run();
  }

  async saveZak(licenceId: number, zakCipher: string, now: number) {
    await this.db.prepare("UPDATE zoom_signins SET zak_cipher = ?, zak_at = ? WHERE licence_id = ?").bind(zakCipher, now, licenceId).run();
  }

  async removeSignin(licenceId: number) {
    await this.db.prepare("DELETE FROM zoom_signins WHERE licence_id = ?").bind(licenceId).run();
  }

  /** True for exactly one caller at a time per licence; the others wait and look again. */
  async lockSignin(licenceId: number, now: number, seconds: number): Promise<boolean> {
    const done = await this.db
      .prepare("UPDATE zoom_signins SET busy_until = ? WHERE licence_id = ? AND busy_until <= ?")
      .bind(now + seconds, licenceId, now)
      .run();
    return done.meta.changes > 0;
  }

  async unlockSignin(licenceId: number) {
    await this.db.prepare("UPDATE zoom_signins SET busy_until = 0 WHERE licence_id = ?").bind(licenceId).run();
  }

  // ---- bought licences (Stripe subscriptions) and trials ----

  async subscriptionOf(stripeId: string): Promise<Sale | null> {
    const row = await this.db.prepare("SELECT * FROM subscriptions WHERE stripe_id = ?").bind(stripeId).first<SaleRow>();
    return row === null ? null : toSale(row);
  }

  /**
   * Ties a new licence to a subscription. False when the subscription already has one
   * (another request was faster): the caller then drops the licence it just made.
   */
  async addSubscription(stripeId: string, licenceId: number, status: string, keyCipher: string, now: number): Promise<boolean> {
    const done = await this.db
      .prepare(
        `INSERT INTO subscriptions (stripe_id, licence_id, status, key_cipher, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?)
         ON CONFLICT (stripe_id) DO NOTHING`,
      )
      .bind(stripeId, licenceId, status, keyCipher, now, now)
      .run();
    return done.meta.changes > 0;
  }

  /** A licence nobody has seen the key of (made a moment ago, and not needed after all). */
  async dropLicence(id: number) {
    await this.db.prepare("DELETE FROM licences WHERE id = ?").bind(id).run();
  }

  /**
   * True when this licence is paid by a subscription that renews by itself: its end date
   * then moves on with every payment, and nobody needs to be told that it is near.
   */
  async renews(licenceId: number): Promise<boolean> {
    const row = await this.db
      .prepare("SELECT status, ending FROM subscriptions WHERE licence_id = ?")
      .bind(licenceId)
      .first<{ status: string; ending: number }>();
    return row !== null && (row.status === "active" || row.status === "trialing") && row.ending === 0;
  }

  /** What the subscription says now. PCs beyond the new number lose their place, the longest unused first. */
  async followSubscription(stripeId: string, licenceId: number, status: string, ending: boolean, seats: number, expiresAt: number, now: number) {
    await this.db
      .prepare("UPDATE subscriptions SET status = ?, ending = ?, updated_at = ? WHERE stripe_id = ?")
      .bind(status, ending ? 1 : 0, now, stripeId)
      .run();
    await this.db.prepare("UPDATE licences SET seats = ?, expires_at = ? WHERE id = ?").bind(seats, expiresAt, licenceId).run();
    const keep = "SELECT id FROM devices WHERE licence_id = ? ORDER BY last_seen_at DESC, id DESC LIMIT ?";
    await this.db
      .prepare(`DELETE FROM logins WHERE device_row IN (SELECT id FROM devices WHERE licence_id = ? AND id NOT IN (${keep}))`)
      .bind(licenceId, licenceId, seats)
      .run();
    await this.db.prepare(`DELETE FROM devices WHERE licence_id = ? AND id NOT IN (${keep})`).bind(licenceId, licenceId, seats).run();
  }

  /** True for exactly one caller: that one sends the key by e-mail. */
  async claimKeyMail(stripeId: string, now: number): Promise<boolean> {
    const done = await this.db
      .prepare("UPDATE subscriptions SET mailed_at = ? WHERE stripe_id = ? AND mailed_at IS NULL AND key_cipher IS NOT NULL")
      .bind(now, stripeId)
      .run();
    return done.meta.changes > 0;
  }

  /** The e-mail did not go out: the next request tries again. */
  async releaseKeyMail(stripeId: string) {
    await this.db.prepare("UPDATE subscriptions SET mailed_at = NULL WHERE stripe_id = ?").bind(stripeId).run();
  }

  /** True for exactly one caller: that one shows the key on the page. */
  async claimKeyShown(stripeId: string, now: number): Promise<boolean> {
    const done = await this.db
      .prepare("UPDATE subscriptions SET shown_at = ? WHERE stripe_id = ? AND shown_at IS NULL AND key_cipher IS NOT NULL")
      .bind(now, stripeId)
      .run();
    return done.meta.changes > 0;
  }

  /** Removes sealed keys that reached their buyer: shown and mailed, or mailed a week ago. */
  async forgetDeliveredKeys(now: number) {
    await this.db
      .prepare("UPDATE subscriptions SET key_cipher = NULL WHERE key_cipher IS NOT NULL AND mailed_at IS NOT NULL AND (shown_at IS NOT NULL OR mailed_at < ?)")
      .bind(now - 7 * 86400)
      .run();
  }

  async trialExists(emailHash: string): Promise<boolean> {
    return (await this.db.prepare("SELECT 1 AS one FROM trials WHERE email_hash = ?").bind(emailHash).first<{ one: number }>()) !== null;
  }

  /** False when this address already had a trial. */
  async addTrial(emailHash: string, licenceId: number, now: number): Promise<boolean> {
    const done = await this.db
      .prepare("INSERT INTO trials (email_hash, licence_id, created_at) VALUES (?, ?, ?) ON CONFLICT (email_hash) DO NOTHING")
      .bind(emailHash, licenceId, now)
      .run();
    return done.meta.changes > 0;
  }
}
