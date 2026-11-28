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
}
