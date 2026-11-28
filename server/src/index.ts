// backhaul gate. A PC with a licence key gets, from here, what it needs to join a Zoom
// meeting: the Meeting SDK token (signed with keys that only this service holds) and,
// after the operator signed in to Zoom, that user's ZAK.
import { Hono } from "hono";
import type { Context } from "hono";
import { newLicenceKey, normaliseKey, problemOf, randomToken, sameText, sha256Hex } from "./licence";
import type { Licence } from "./licence";
import { adminPage } from "./admin_page";
import { publicPage, publicPaths } from "./public_pages";
import { openShop, shopRoutes } from "./shop";
import { Store } from "./store";
import { Vault } from "./vault";
import type { Device } from "./store";
import { ZoomError, authoriseUrl, exchangeCode, fetchZak, makeSdkToken, refresh, revoke } from "./zoom";
import type { Fetcher, ZoomKeys } from "./zoom";

export interface Env {
  DB: D1Database;
  PUBLIC_URL: string;
  ZOOM_CLIENT_ID?: string;
  ZOOM_CLIENT_SECRET?: string;
  ADMIN_TOKEN?: string;
  SIGNIN_KEY?: string; // random text, 32 characters or more: seals the licences' Zoom sign-ins and undelivered keys
  // Selling (see shop.ts). The first three are plain settings, the rest are secrets.
  SITE_URL?: string; // the sales site, the only origin that may call /v1/shop from a browser
  MAIL_FROM?: string; // "backhaul <address>", an address of a domain verified at Resend
  BILLING_URL?: string; // Stripe's sign-in page for subscribers (plan, card, cancel)
  SELLER_TAX_ID?: string; // the seller's registration number for Japanese invoices, shown on mails of yen purchases
  STRIPE_SECRET_KEY?: string;
  STRIPE_WEBHOOK_SECRET?: string;
  RESEND_API_KEY?: string;
}

/** The outside world, so that tests can set the time and stand in for Zoom. */
export interface Outside {
  now: () => number; // unix seconds
  fetcher: Fetcher;
  sleep: (ms: number) => Promise<void>;
}

type App = { Bindings: Env; Variables: { device: Device; licence: Licence } };

const MAX_SEATS = 100;
const ZAK_KEEP_SECONDS = 600; // PCs joining within ten minutes share one ZAK (it lasts two hours at Zoom)
const LOCK_SECONDS = 20;

function zoomKeys(env: Env): ZoomKeys | null {
  if (!env.ZOOM_CLIENT_ID || !env.ZOOM_CLIENT_SECRET) return null;
  return { clientId: env.ZOOM_CLIENT_ID, clientSecret: env.ZOOM_CLIENT_SECRET };
}

function licenceView(licence: Licence, devices: Device[], own: Device | null, zoomSignedIn: boolean) {
  return {
    zoomSignedIn,
    keyTail: licence.keyTail,
    seats: licence.seats,
    used: devices.length,
    expiresAt: licence.expiresAt,
    deviceName: own?.name ?? null,
    devices: devices.map((d) => d.name),
  };
}

async function readJson(c: Context): Promise<Record<string, unknown> | null> {
  const body: unknown = await c.req.json().catch(() => null);
  return body !== null && typeof body === "object" && !Array.isArray(body) ? (body as Record<string, unknown>) : null;
}

function text(value: unknown, min: number, max: number): string | null {
  return typeof value === "string" && value.length >= min && value.length <= max ? value : null;
}

function page(title: string, message: string): string {
  const safe = (s: string) => s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
  return `<!doctype html><html lang="ja"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>${safe(title)}</title><style>body{margin:0;background:#0a0b0c;color:#f2f2f0;font:16px/1.7 system-ui,"Noto Sans JP",sans-serif;display:grid;place-items:center;min-height:100vh}
main{max-width:32rem;padding:2rem}h1{font-size:1.25rem;margin:0 0 .75rem}p{margin:0;color:#9a9fa6}small{display:block;margin-top:2rem;font:13px ui-monospace,monospace;letter-spacing:.08em;color:#9a9fa6}</style></head>
<body><main><h1>${safe(title)}</h1><p>${safe(message)}</p><small>backhaul</small></main></body></html>`;
}

export function createApp(outside: Outside) {
  const app = new Hono<App>();

  app.onError((error, c) => {
    console.error(error);
    return c.json({ error: "server_error" }, 500);
  });

  app.get("/", async (c) =>
    c.json({
      service: "backhaul-gate",
      zoomConfigured: zoomKeys(c.env) !== null,
      signInConfigured: (await Vault.open(c.env.SIGNIN_KEY)) !== null,
      shopConfigured: (await openShop(c.env, outside)) !== null,
    }),
  );

  // Before "/v1" below: these need no PC token.
  app.route("/v1/shop", shopRoutes(outside));

  // ---- registering a PC ----

  app.post("/v1/activate", async (c) => {
    const store = new Store(c.env.DB);
    const now = outside.now();
    const caller = await sha256Hex(c.req.header("cf-connecting-ip") ?? "unknown");
    // Only keys that do not exist count: that is what guessing looks like. A venue that
    // registers twenty PCs through one connection within minutes is not slowed down.
    const guesses = `activate:${caller}`;
    if (await store.over(guesses, 10, 600, now)) return c.json({ error: "slow_down" }, 429);
    const wrongKey = async () => {
      await store.allow(guesses, 10, 600, now);
      return c.json({ error: "unknown_key" }, 404);
    };

    const body = await readJson(c);
    const key = normaliseKey(text(body?.key, 1, 80) ?? "");
    const deviceId = text(body?.deviceId, 16, 64);
    const name = text(body?.deviceName, 1, 60);
    if (key === null) return wrongKey();
    if (deviceId === null || !/^[A-Za-z0-9-]+$/.test(deviceId) || name === null) return c.json({ error: "bad_request" }, 400);

    const licence = await store.licenceByKeyHash(await sha256Hex(key));
    if (licence === null) return wrongKey();
    const problem = problemOf(licence, now);
    if (problem !== null) return c.json({ error: problem, expiresAt: licence.expiresAt }, 403);

    const token = randomToken();
    if (!(await store.registerDevice(licence.id, licence.seats, deviceId, name.trim() || "PC", await sha256Hex(token), now))) {
      const devices = await store.devicesOf(licence.id);
      return c.json({ error: "seats_full", seats: licence.seats, devices: devices.map((d) => d.name) }, 409);
    }
    const devices = await store.devicesOf(licence.id);
    const own = devices.find((d) => d.deviceId === deviceId) ?? null;
    const signedIn = (await store.signinOf(licence.id)) !== null;
    return c.json({ deviceToken: token, licence: licenceView(licence, devices, own, signedIn) });
  });

  // ---- everything below /v1 needs the PC's token ----

  const pc = new Hono<App>();
  pc.use("*", async (c, next) => {
    const header = c.req.header("authorization") ?? "";
    const token = header.startsWith("Bearer ") ? header.slice(7) : "";
    if (token.length < 20 || token.length > 200) return c.json({ error: "unknown_device" }, 401);
    const store = new Store(c.env.DB);
    const device = await store.deviceByTokenHash(await sha256Hex(token));
    if (device === null) return c.json({ error: "unknown_device" }, 401);
    const licence = await store.licenceById(device.licenceId);
    if (licence === null) return c.json({ error: "unknown_device" }, 401);
    c.set("device", device);
    c.set("licence", licence);
    // A PC may always look at its licence and give its place back; the rest needs a good licence.
    const path = new URL(c.req.url).pathname;
    if (path !== "/v1/status" && path !== "/v1/deactivate") {
      const problem = problemOf(licence, outside.now());
      if (problem !== null) return c.json({ error: problem, expiresAt: licence.expiresAt }, 403);
    }
    await next();
  });

  pc.post("/status", async (c) => {
    const store = new Store(c.env.DB);
    const licence = c.get("licence");
    const devices = await store.devicesOf(licence.id);
    const signedIn = (await store.signinOf(licence.id)) !== null;
    return c.json({ problem: problemOf(licence, outside.now()), licence: licenceView(licence, devices, c.get("device"), signedIn) });
  });

  pc.post("/deactivate", async (c) => {
    await new Store(c.env.DB).removeDevice(c.get("device").row);
    return c.json({ ok: true });
  });

  pc.post("/sdk-token", async (c) => {
    const keys = zoomKeys(c.env);
    if (keys === null) return c.json({ error: "not_configured" }, 503);
    const store = new Store(c.env.DB);
    const now = outside.now();
    const device = c.get("device");
    if (!(await store.allow(`sdk:${device.row}`, 60, 3600, now))) return c.json({ error: "slow_down" }, 429);
    await store.touchDevice(device.row, now);
    return c.json(await makeSdkToken(keys, now));
  });

  // ---- signing in to Zoom ----
  // One sign-in serves every PC of the licence. The PC that starts it makes a PKCE verifier
  // and keeps it: what Zoom sends back to this service is useless without that verifier.
  // What comes out of the sign-in is kept here, sealed, and never handed to a PC; PCs get the ZAK.

  pc.post("/zoom/begin", async (c) => {
    const keys = zoomKeys(c.env);
    if (keys === null || (await Vault.open(c.env.SIGNIN_KEY)) === null) return c.json({ error: "not_configured" }, 503);
    const body = await readJson(c);
    const challenge = text(body?.challenge, 43, 128);
    if (challenge === null || !/^[A-Za-z0-9_-]+$/.test(challenge)) return c.json({ error: "bad_request" }, 400);
    const state = randomToken(24);
    await new Store(c.env.DB).startLogin(state, c.get("device").row, challenge, outside.now());
    return c.json({ state, url: authoriseUrl(keys, `${c.env.PUBLIC_URL}/v1/zoom/callback`, state, challenge) });
  });

  pc.post("/zoom/collect", async (c) => {
    const keys = zoomKeys(c.env);
    const vault = await Vault.open(c.env.SIGNIN_KEY);
    if (keys === null || vault === null) return c.json({ error: "not_configured" }, 503);
    const body = await readJson(c);
    const state = text(body?.state, 10, 100);
    const verifier = text(body?.verifier, 43, 128);
    if (state === null || verifier === null) return c.json({ error: "bad_request" }, 400);
    const store = new Store(c.env.DB);
    const now = outside.now();
    const login = await store.loginByState(state, now);
    if (login === null || login.deviceRow !== c.get("device").row) return c.json({ error: "unknown_login" }, 404);
    if (login.denied) {
      await store.endLogin(state);
      return c.json({ error: "denied" }, 410);
    }
    if (login.code === null) return c.json({ pending: true }, 202);
    await store.endLogin(state); // a code works once
    try {
      const tokens = await exchangeCode(keys, outside.fetcher, login.code, verifier, `${c.env.PUBLIC_URL}/v1/zoom/callback`);
      await store.saveSignin(
        c.get("licence").id,
        await vault.seal(tokens.refreshToken),
        tokens.expiresIn > 0 ? await vault.seal(tokens.accessToken) : null,
        now + tokens.expiresIn,
        now,
      );
      return c.json({ signedIn: true });
    } catch (error) {
      if (!(error instanceof ZoomError)) throw error;
      return c.json({ error: "zoom_failed", detail: error.message }, 502);
    }
  });

  pc.post("/zoom/zak", async (c) => {
    const keys = zoomKeys(c.env);
    const vault = await Vault.open(c.env.SIGNIN_KEY);
    if (keys === null || vault === null) return c.json({ error: "not_configured" }, 503);
    const store = new Store(c.env.DB);
    const licenceId = c.get("licence").id;
    // Several PCs usually join within the same seconds. One of them talks to Zoom (a refresh
    // token works once, so two at a time would break the sign-in); the others wait for its result.
    for (let round = 0; round < 16; round++) {
      const now = outside.now();
      const seen = await store.signinOf(licenceId);
      if (seen === null) return c.json({ error: "not_signed_in" }, 404);
      if (seen.zakCipher !== null && now - seen.zakAt < ZAK_KEEP_SECONDS) {
        const kept = await vault.unseal(seen.zakCipher);
        if (kept !== null) return c.json({ zak: kept });
      }
      if (!(await store.lockSignin(licenceId, now, LOCK_SECONDS))) {
        await outside.sleep(500);
        continue;
      }
      try {
        // Read again now that nobody else can change it: between the look above and the
        // lock, another PC may have renewed the tokens, and the old refresh token is dead.
        const signin = await store.signinOf(licenceId);
        if (signin === null) return c.json({ error: "not_signed_in" }, 404);
        if (signin.zakCipher !== null && now - signin.zakAt < ZAK_KEEP_SECONDS) {
          const kept = await vault.unseal(signin.zakCipher);
          if (kept !== null) return c.json({ zak: kept });
        }
        const access = signin.accessExpiresAt - now > 120 ? await vault.unseal(signin.accessCipher) : null;
        let zak: string | null = null;
        if (access !== null) {
          try {
            zak = await fetchZak(outside.fetcher, access);
          } catch (error) {
            // The remembered access token may simply have been withdrawn early: renew and try once more.
            if (!(error instanceof ZoomError) || !error.signedOut) throw error;
          }
        }
        if (zak === null) {
          const refreshToken = await vault.unseal(signin.refreshCipher);
          if (refreshToken === null) {
            // Sealed with another key (the secret was replaced): nobody can use it any more.
            await store.removeSignin(licenceId);
            return c.json({ error: "signed_out" }, 401);
          }
          const tokens = await refresh(keys, outside.fetcher, refreshToken);
          // Kept before anything else: the old refresh token is dead from here on.
          await store.renewSignin(licenceId, await vault.seal(tokens.refreshToken), await vault.seal(tokens.accessToken), now + tokens.expiresIn, now);
          zak = await fetchZak(outside.fetcher, tokens.accessToken);
        }
        await store.saveZak(licenceId, await vault.seal(zak), now);
        return c.json({ zak });
      } catch (error) {
        if (!(error instanceof ZoomError)) throw error;
        if (error.signedOut) {
          await store.removeSignin(licenceId);
          return c.json({ error: "signed_out", detail: error.message }, 401);
        }
        return c.json({ error: "zoom_failed", detail: error.message }, 502);
      } finally {
        await store.unlockSignin(licenceId);
      }
    }
    return c.json({ error: "zoom_failed", detail: "another PC of this licence is still asking Zoom" }, 503);
  });

  pc.post("/zoom/signout", async (c) => {
    const keys = zoomKeys(c.env);
    const vault = await Vault.open(c.env.SIGNIN_KEY);
    if (keys === null || vault === null) return c.json({ error: "not_configured" }, 503);
    const store = new Store(c.env.DB);
    const licenceId = c.get("licence").id;
    const signin = await store.signinOf(licenceId);
    if (signin === null) return c.json({ revoked: false });
    await store.removeSignin(licenceId); // gone here whatever Zoom answers
    const refreshToken = await vault.unseal(signin.refreshCipher);
    return c.json({ revoked: refreshToken !== null && (await revoke(keys, outside.fetcher, refreshToken)) });
  });

  // Zoom sends the browser here after the sign-in. No token of the PC is involved.
  app.get("/v1/zoom/callback", async (c) => {
    const state = c.req.query("state") ?? "";
    const code = c.req.query("code") ?? "";
    const store = new Store(c.env.DB);
    const login = state.length > 0 ? await store.loginByState(state, outside.now()) : null;
    if (login === null) {
      return c.html(page("サインインをやり直してください", "時間が空きすぎたか、別の画面から開かれました。backhaul の「Zoomにサインイン」をもう一度押してください。"), 400);
    }
    if (code.length === 0 || code.length > 2000) {
      await store.answerLogin(state, null);
      return c.html(page("サインインは行われませんでした", "許可されなかったため、何も変わっていません。backhaul に戻ってください。"));
    }
    await store.answerLogin(state, code);
    return c.html(page("サインインできました", "このページは閉じて、backhaul に戻ってください。"));
  });

  app.route("/v1", pc);

  // ---- for the owner: issuing and stopping licences ----

  const admin = new Hono<App>();
  admin.use("*", async (c, next) => {
    const expected = c.env.ADMIN_TOKEN ?? "";
    const header = c.req.header("authorization") ?? "";
    if (expected.length < 20) return c.json({ error: "not_configured" }, 503);
    if (!header.startsWith("Bearer ") || !sameText(header.slice(7), expected)) return c.json({ error: "forbidden" }, 403);
    await next();
  });

  // The page itself holds nothing secret; every request it makes needs the admin token.
  // Privacy, terms, support and the guide: what the Zoom listing links to.
  for (const path of publicPaths) {
    app.get(path, (c) => c.html(publicPage(path) ?? "", 200, { "cache-control": "public, max-age=300", "x-frame-options": "DENY", "x-content-type-options": "nosniff" }));
  }

  app.get("/admin", (c) => c.html(adminPage(), 200, { "cache-control": "no-store", "x-frame-options": "DENY", "referrer-policy": "no-referrer" }));

  admin.post("/licences", async (c) => {
    const body = await readJson(c);
    const seats = body?.seats;
    const days = body?.days ?? null;
    const note = text(body?.note ?? "", 0, 200);
    if (typeof seats !== "number" || !Number.isInteger(seats) || seats < 1 || seats > MAX_SEATS || note === null) {
      return c.json({ error: "bad_request" }, 400);
    }
    if (days !== null && (typeof days !== "number" || !Number.isInteger(days) || days < 1 || days > 3660)) {
      return c.json({ error: "bad_request" }, 400);
    }
    const now = outside.now();
    const key = newLicenceKey();
    const expiresAt = days === null ? null : now + (days as number) * 86400;
    const id = await new Store(c.env.DB).createLicence(await sha256Hex(key), key.slice(-4), note, seats, expiresAt, now);
    return c.json({ id, key, seats, expiresAt, note }); // the only time the key is shown
  });

  admin.get("/licences", async (c) => {
    const store = new Store(c.env.DB);
    const now = outside.now();
    const list = [];
    for (const licence of await store.listLicences()) {
      const devices = await store.devicesOf(licence.id);
      list.push({
        ...licence,
        problem: problemOf(licence, now),
        devices: devices.map((d) => ({ name: d.name, lastSeenAt: d.lastSeenAt })),
      });
    }
    return c.json({ licences: list });
  });

  admin.post("/licences/:id/:action{revoke|restore}", async (c) => {
    const id = Number(c.req.param("id"));
    if (!Number.isInteger(id)) return c.json({ error: "bad_request" }, 400);
    const store = new Store(c.env.DB);
    const stopping = c.req.param("action") === "revoke";
    const found = await store.setRevoked(id, stopping);
    // A stopped licence keeps no Zoom sign-in: restoring it later means signing in again.
    if (found && stopping) await store.removeSignin(id);
    return found ? c.json({ ok: true }) : c.json({ error: "unknown_licence" }, 404);
  });

  app.route("/admin", admin);
  return app;
}

export default createApp({
  now: () => Math.floor(Date.now() / 1000),
  fetcher: (input, init) => fetch(input, init),
  sleep: (ms) => new Promise((done) => setTimeout(done, ms)),
});
