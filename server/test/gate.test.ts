import { beforeEach, describe, expect, it } from "vitest";
import { createApp } from "../src/index";
import type { Env } from "../src/index";
import { newLicenceKey, normaliseKey, sameText } from "../src/licence";
import { memoryD1 } from "./d1";

const ADMIN = "sample-admin-token-0123456789";
const PC_A = "sample-device-aaaaaaaa";
const PC_B = "sample-device-bbbbbbbb";
const VERIFIER = "v".repeat(64);
const CHALLENGE = "c".repeat(43);

let now = 1_800_000_000;
let env: Env;
/** What "Zoom" was asked, and what it answers. */
let zoomCalls: { url: string; body: string; auth: string }[] = [];
let zoomAnswer: (url: string, body: string) => Response;

const app = createApp({
  now: () => now,
  fetcher: async (url, init) => {
    const body = typeof init?.body === "string" ? init.body : "";
    const auth = new Headers(init?.headers).get("authorization") ?? "";
    zoomCalls.push({ url, body, auth });
    return zoomAnswer(url, body);
  },
  // Waiting for another PC's request: time moves on instead of the test standing still.
  sleep: async (ms) => {
    slept += ms;
    now += 1;
  },
});
let slept = 0;
const SIGNIN_KEY = "sample-signin-key-otter-0123456789-abcdef";

function call(path: string, body?: unknown, token?: string, method = "POST") {
  const headers: Record<string, string> = { "content-type": "application/json", "cf-connecting-ip": "192.0.2.7" };
  if (token !== undefined) headers.authorization = `Bearer ${token}`;
  return app.request(path, { method, headers, body: body === undefined ? undefined : JSON.stringify(body) }, env);
}

async function issue(seats = 2, days: number | null = 30): Promise<{ id: number; key: string }> {
  const r = await call("/admin/licences", { seats, days, note: "Sample Hall Co." }, ADMIN);
  expect(r.status).toBe(200);
  return (await r.json()) as { id: number; key: string };
}

async function activate(key: string, deviceId = PC_A, deviceName = "SAMPLE-PC-01") {
  return call("/v1/activate", { key, deviceId, deviceName });
}

async function tokenFor(key: string, deviceId = PC_A, name = "SAMPLE-PC-01"): Promise<string> {
  const r = await activate(key, deviceId, name);
  expect(r.status).toBe(200);
  return ((await r.json()) as { deviceToken: string }).deviceToken;
}

/** A second app over the same database whose Zoom is slow to renew tokens. */
function createSlowApp(onRefresh: () => Promise<Response>) {
  let reached: () => void = () => {};
  const reachedZoom = new Promise<void>((resolve) => (reached = resolve));
  const slowApp = createApp({
    now: () => now,
    fetcher: async (url, init) => {
      if (url.includes("/oauth/token")) {
        reached();
        return onRefresh();
      }
      return zoomAnswer(url, typeof init?.body === "string" ? init.body : "");
    },
    sleep: () => new Promise((r) => setTimeout(r, 5)),
  });
  return {
    reachedZoom,
    call: (path: string, token: string) =>
      slowApp.request(path, { method: "POST", headers: { "content-type": "application/json", authorization: `Bearer ${token}` }, body: "{}" }, env),
  };
}

beforeEach(() => {
  now = 1_800_000_000;
  env = { DB: memoryD1(), PUBLIC_URL: "https://gate.example.invalid", ZOOM_CLIENT_ID: "SampleClientId", ZOOM_CLIENT_SECRET: "sample-secret", ADMIN_TOKEN: ADMIN, SIGNIN_KEY };
  slept = 0;
  zoomCalls = [];
  zoomAnswer = () => new Response("{}", { status: 500 });
});

describe("licence keys", () => {
  it("are 100 random bits in a form people can type", () => {
    const key = newLicenceKey();
    expect(key).toMatch(/^BH-[0-9A-HJKMNP-TV-Z]{5}(-[0-9A-HJKMNP-TV-Z]{5}){3}$/);
    expect(newLicenceKey()).not.toBe(key);
  });
  it("are understood however they are typed", () => {
    expect(normaliseKey("bh-abcde-fgh1j-kmnpq-rstvw")).toBe("BH-ABCDE-FGH1J-KMNPQ-RSTVW");
    expect(normaliseKey("  BH ABCDE FGHIJ KMNPQ RSTVW ")).toBe("BH-ABCDE-FGH1J-KMNPQ-RSTVW"); // I read as 1
    expect(normaliseKey("ABCDEFGH1JKMNPQRSTVW")).toBe("BH-ABCDE-FGH1J-KMNPQ-RSTVW");
    expect(normaliseKey("BH-0O0O0-LLLLL-AAAAA-BBBBB")).toBe("BH-00000-11111-AAAAA-BBBBB");
    expect(normaliseKey("BH-ABCDE")).toBeNull();
    expect(normaliseKey("BH-ABCDE-FGH1J-KMNPQ-RSTV!")).toBeNull();
    expect(normaliseKey("")).toBeNull();
  });
  it("compare without shortcuts", () => {
    expect(sameText("abc", "abc")).toBe(true);
    expect(sameText("abc", "abd")).toBe(false);
    expect(sameText("abc", "abcd")).toBe(false);
  });
});

describe("the owner's side", () => {
  it("needs the admin token", async () => {
    expect((await call("/admin/licences", { seats: 1 })).status).toBe(403);
    expect((await call("/admin/licences", { seats: 1 }, "wrong-token-wrong-token-wrong")).status).toBe(403);
    env.ADMIN_TOKEN = undefined;
    expect((await call("/admin/licences", { seats: 1 }, ADMIN)).status).toBe(503);
  });
  it("issues, lists and stops licences; the key is shown once and not stored", async () => {
    const { id, key } = await issue(3, 365);
    const list = (await (await call("/admin/licences", undefined, ADMIN, "GET")).json()) as { licences: Record<string, unknown>[] };
    expect(list.licences).toHaveLength(1);
    expect(list.licences[0]).toMatchObject({ id, seats: 3, note: "Sample Hall Co.", keyTail: key.slice(-4), revoked: false, problem: null });
    expect(JSON.stringify(list)).not.toContain(key);
    expect((await call(`/admin/licences/${id}/revoke`, undefined, ADMIN)).status).toBe(200);
    expect((await activate(key)).status).toBe(403);
    expect((await call(`/admin/licences/${id}/restore`, undefined, ADMIN)).status).toBe(200);
    expect((await activate(key)).status).toBe(200);
    expect((await call("/admin/licences/999/revoke", undefined, ADMIN)).status).toBe(404);
  });
  it("refuses nonsense", async () => {
    for (const body of [{}, { seats: 0 }, { seats: 1.5 }, { seats: 101 }, { seats: 1, days: 0 }, { seats: 1, note: 5 }]) {
      expect((await call("/admin/licences", body, ADMIN)).status).toBe(400);
    }
  });
});

describe("registering PCs", () => {
  it("gives a PC a token and says where the licence stands", async () => {
    const { key } = await issue(2, 30);
    const r = await activate(key.toLowerCase().replace(/-/g, " "));
    expect(r.status).toBe(200);
    const body = (await r.json()) as { deviceToken: string; licence: Record<string, unknown> };
    expect(body.deviceToken.length).toBeGreaterThan(40);
    expect(body.licence).toMatchObject({ keyTail: key.slice(-4), seats: 2, used: 1, deviceName: "SAMPLE-PC-01", expiresAt: now + 30 * 86400 });
  });
  it("keeps to the number of PCs, and names who holds the places", async () => {
    const { key } = await issue(2);
    await tokenFor(key, PC_A, "SAMPLE-PC-01");
    await tokenFor(key, PC_B, "SAMPLE-PC-02");
    const full = await activate(key, "sample-device-cccccccc", "SAMPLE-PC-03");
    expect(full.status).toBe(409);
    expect(await full.json()).toEqual({ error: "seats_full", seats: 2, devices: ["SAMPLE-PC-01", "SAMPLE-PC-02"] });
  });
  it("lets a registered PC register again without taking a second place; its old token stops", async () => {
    const { key } = await issue(1);
    const first = await tokenFor(key, PC_A, "SAMPLE-PC-01");
    const second = await tokenFor(key, PC_A, "SAMPLE-PC-01B");
    expect(second).not.toBe(first);
    expect((await call("/v1/status", {}, first)).status).toBe(401);
    const status = (await (await call("/v1/status", {}, second)).json()) as { licence: Record<string, unknown> };
    expect(status.licence).toMatchObject({ used: 1, deviceName: "SAMPLE-PC-01B" });
  });
  it("frees a place when a PC is taken out", async () => {
    const { key } = await issue(1);
    const token = await tokenFor(key, PC_A);
    expect((await activate(key, PC_B)).status).toBe(409);
    expect((await call("/v1/deactivate", {}, token)).status).toBe(200);
    expect((await call("/v1/status", {}, token)).status).toBe(401);
    expect((await activate(key, PC_B)).status).toBe(200);
  });
  it("says why a key does not work", async () => {
    expect((await activate("BH-AAAAA-AAAAA-AAAAA-AAAAA")).status).toBe(404);
    expect((await activate("not a key")).status).toBe(404);
    const { key } = await issue(1, 1);
    expect((await call("/v1/activate", { key, deviceId: "short", deviceName: "x" })).status).toBe(400);
    expect((await call("/v1/activate", { key, deviceId: PC_A, deviceName: "" })).status).toBe(400);
    now += 86400;
    const late = await activate(key);
    expect(late.status).toBe(403);
    expect(((await late.json()) as { error: string }).error).toBe("expired");
  });
  it("slows down guessing", async () => {
    for (let i = 0; i < 10; i++) expect((await activate("BH-AAAAA-AAAAA-AAAAA-AAAAA")).status).toBe(404);
    expect((await activate("BH-AAAAA-AAAAA-AAAAA-AAAAA")).status).toBe(429);
    now += 601;
    expect((await activate("BH-AAAAA-AAAAA-AAAAA-AAAAA")).status).toBe(404);
  });
});

describe("the Meeting SDK token", () => {
  it("is signed with the Zoom keys and lasts a day", async () => {
    const { key } = await issue();
    const token = await tokenFor(key);
    const r = await call("/v1/sdk-token", {}, token);
    expect(r.status).toBe(200);
    const body = (await r.json()) as { token: string; expiresAt: number };
    expect(body.expiresAt).toBe(now + 86400);
    const [header, payload, signature] = body.token.split(".") as [string, string, string];
    expect(JSON.parse(atob(header.replace(/-/g, "+").replace(/_/g, "/")))).toEqual({ alg: "HS256", typ: "JWT" });
    expect(JSON.parse(atob(payload.replace(/-/g, "+").replace(/_/g, "/")))).toEqual({ appKey: "SampleClientId", iat: now - 30, exp: now + 86400, tokenExp: now + 86400 });
    // Check the signature the way Zoom would.
    const hmacKey = await crypto.subtle.importKey("raw", new TextEncoder().encode("sample-secret"), { name: "HMAC", hash: "SHA-256" }, false, ["verify"]);
    const raw = Uint8Array.from(atob(signature.replace(/-/g, "+").replace(/_/g, "/")), (ch) => ch.charCodeAt(0));
    expect(await crypto.subtle.verify("HMAC", hmacKey, raw, new TextEncoder().encode(`${header}.${payload}`))).toBe(true);
    expect(body.token).not.toContain("sample-secret");
  });
  it("is only for PCs with a good licence", async () => {
    const { id, key } = await issue(1, 2);
    const token = await tokenFor(key);
    expect((await call("/v1/sdk-token", {})).status).toBe(401);
    expect((await call("/v1/sdk-token", {}, "x".repeat(43))).status).toBe(401);
    await call(`/admin/licences/${id}/revoke`, undefined, ADMIN);
    expect((await call("/v1/sdk-token", {}, token)).status).toBe(403);
    const status = (await (await call("/v1/status", {}, token)).json()) as { problem: string };
    expect(status.problem).toBe("revoked"); // the PC can still see why
    await call(`/admin/licences/${id}/restore`, undefined, ADMIN);
    now += 2 * 86400;
    const late = await call("/v1/sdk-token", {}, token);
    expect(late.status).toBe(403);
    expect(((await late.json()) as { error: string }).error).toBe("expired");
  });
  it("says so when the Zoom keys were not put in yet", async () => {
    const { key } = await issue();
    const token = await tokenFor(key);
    env.ZOOM_CLIENT_SECRET = undefined;
    expect((await call("/v1/sdk-token", {}, token)).status).toBe(503);
    expect(((await (await app.request("/", {}, env)).json()) as { zoomConfigured: boolean }).zoomConfigured).toBe(false);
  });
});

describe("signing in to Zoom", () => {
  async function begin(token: string) {
    const r = await call("/v1/zoom/begin", { challenge: CHALLENGE }, token);
    expect(r.status).toBe(200);
    return (await r.json()) as { state: string; url: string };
  }

  it("sends the browser to Zoom, waits for the answer and hands the tokens to the PC only", async () => {
    const { key } = await issue();
    const token = await tokenFor(key);
    const { state, url } = await begin(token);
    const zoom = new URL(url);
    expect(zoom.origin + zoom.pathname).toBe("https://zoom.us/oauth/authorize");
    expect(Object.fromEntries(zoom.searchParams)).toEqual({
      response_type: "code",
      client_id: "SampleClientId",
      redirect_uri: "https://gate.example.invalid/v1/zoom/callback",
      state,
      code_challenge: CHALLENGE,
      code_challenge_method: "S256",
    });
    expect(url).not.toContain("sample-secret");

    // Nothing yet: the operator is still looking at the browser.
    expect((await call("/v1/zoom/collect", { state, verifier: VERIFIER }, token)).status).toBe(202);

    const back = await app.request(`/v1/zoom/callback?state=${state}&code=sample-code`, {}, env);
    expect(back.status).toBe(200);
    expect(await back.text()).toContain("サインインできました");

    zoomAnswer = () => Response.json({ access_token: "sample-access", refresh_token: "sample-refresh-1", expires_in: 3600 });
    const done = await call("/v1/zoom/collect", { state, verifier: VERIFIER }, token);
    expect(done.status).toBe(200);
    // Nothing of Zoom's tokens goes to the PC, and nothing readable is in the database.
    expect(await done.json()).toEqual({ signedIn: true });
    const kept = JSON.stringify(await env.DB.prepare("SELECT * FROM zoom_signins").all());
    expect(kept).not.toContain("sample-refresh-1");
    expect(kept).not.toContain("sample-access");
    const status = (await (await call("/v1/status", {}, token)).json()) as { licence: { zoomSignedIn: boolean } };
    expect(status.licence.zoomSignedIn).toBe(true);
    expect(zoomCalls).toHaveLength(1);
    expect(zoomCalls[0]!.url).toBe("https://zoom.us/oauth/token");
    expect(zoomCalls[0]!.auth).toBe(`Basic ${btoa("SampleClientId:sample-secret")}`);
    expect(Object.fromEntries(new URLSearchParams(zoomCalls[0]!.body))).toEqual({
      grant_type: "authorization_code",
      code: "sample-code",
      redirect_uri: "https://gate.example.invalid/v1/zoom/callback",
      code_verifier: VERIFIER,
    });
    // The answer can be collected once.
    expect((await call("/v1/zoom/collect", { state, verifier: VERIFIER }, token)).status).toBe(404);
  });

  it("belongs to the PC that started it, and does not wait for ever", async () => {
    const { key } = await issue(2);
    const a = await tokenFor(key, PC_A);
    const b = await tokenFor(key, PC_B, "SAMPLE-PC-02");
    const { state } = await begin(a);
    expect((await call("/v1/zoom/collect", { state, verifier: VERIFIER }, b)).status).toBe(404);
    now += 601;
    expect((await call("/v1/zoom/collect", { state, verifier: VERIFIER }, a)).status).toBe(404);
    expect((await app.request(`/v1/zoom/callback?state=${state}&code=x`, {}, env)).status).toBe(400);
    expect((await app.request("/v1/zoom/callback", {}, env)).status).toBe(400);
  });

  it("tells the PC when the operator said no, or Zoom refused", async () => {
    const { key } = await issue();
    const token = await tokenFor(key);
    const first = await begin(token);
    const no = await app.request(`/v1/zoom/callback?state=${first.state}&error=access_denied`, {}, env);
    expect(await no.text()).toContain("行われませんでした");
    expect((await call("/v1/zoom/collect", { state: first.state, verifier: VERIFIER }, token)).status).toBe(410);

    const second = await begin(token);
    await app.request(`/v1/zoom/callback?state=${second.state}&code=sample-code`, {}, env);
    zoomAnswer = () => Response.json({ reason: "Invalid authorization code" }, { status: 400 });
    const refused = await call("/v1/zoom/collect", { state: second.state, verifier: VERIFIER }, token);
    expect(refused.status).toBe(502);
    expect(((await refused.json()) as { detail: string }).detail).toContain("Invalid authorization code");
    expect((await call("/v1/zoom/begin", { challenge: "short" }, token)).status).toBe(400);
  });

  /** Signs the licence in (through the PC with `token`) and forgets what Zoom was asked so far. */
  async function signedIn(token: string) {
    const { state } = await begin(token);
    await app.request(`/v1/zoom/callback?state=${state}&code=sample-code`, {}, env);
    zoomAnswer = () => Response.json({ access_token: "sample-access-1", refresh_token: "sample-refresh-1", expires_in: 3600 });
    expect((await call("/v1/zoom/collect", { state, verifier: VERIFIER }, token)).status).toBe(200);
    zoomCalls = [];
  }
  const zoomUrls = () => zoomCalls.map((c) => c.url.replace("https://", "").split("?")[0]);

  it("serves every PC of the licence from one sign-in, and asks Zoom as little as it can", async () => {
    const { key } = await issue(3);
    const a = await tokenFor(key, PC_A);
    const b = await tokenFor(key, PC_B, "SAMPLE-PC-02");
    expect((await call("/v1/zoom/zak", {}, a)).status).toBe(404); // nobody signed in yet
    await signedIn(a);
    expect(((await (await call("/v1/status", {}, b)).json()) as { licence: { zoomSignedIn: boolean } }).licence.zoomSignedIn).toBe(true);

    let zaks = 0;
    zoomAnswer = (url) => (url.includes("/users/me/token") ? Response.json({ token: `sample-zak-${++zaks}` }) : new Response("{}", { status: 500 }));
    // The other PC joins: the access token from the sign-in is still good, so only the ZAK is fetched.
    expect(await (await call("/v1/zoom/zak", {}, b)).json()).toEqual({ zak: "sample-zak-1" });
    expect(zoomUrls()).toEqual(["api.zoom.us/v2/users/me/token"]);
    expect(zoomCalls[0]!.auth).toBe("Bearer sample-access-1");
    // The rest of the PCs, seconds later: the same ZAK, without asking Zoom again.
    now += 5;
    expect(await (await call("/v1/zoom/zak", {}, a)).json()).toEqual({ zak: "sample-zak-1" });
    expect(zoomCalls).toHaveLength(1);
    // Later the same day: a fresh ZAK, and by then the access token has to be renewed first.
    now += 2 * 3600;
    zoomAnswer = (url) =>
      url.includes("/oauth/token")
        ? Response.json({ access_token: "sample-access-2", refresh_token: "sample-refresh-2", expires_in: 3600 })
        : Response.json({ token: "sample-zak-later" });
    expect(await (await call("/v1/zoom/zak", {}, a)).json()).toEqual({ zak: "sample-zak-later" });
    expect(Object.fromEntries(new URLSearchParams(zoomCalls[1]!.body))).toEqual({ grant_type: "refresh_token", refresh_token: "sample-refresh-1" });
    expect(zoomCalls[2]).toMatchObject({ url: "https://api.zoom.us/v2/users/me/token?type=zak", auth: "Bearer sample-access-2" });
    // The renewed refresh token is the one used next time (the old one is dead at Zoom).
    now += 2 * 3600;
    await call("/v1/zoom/zak", {}, b);
    expect(Object.fromEntries(new URLSearchParams(zoomCalls[3]!.body)).refresh_token).toBe("sample-refresh-2");
  });

  it("lets one PC at a time talk to Zoom; the others wait for its result", async () => {
    const { key } = await issue(2);
    const a = await tokenFor(key, PC_A);
    const b = await tokenFor(key, PC_B, "SAMPLE-PC-02");
    await signedIn(a);
    now += 2 * 3600; // so that a refresh is needed: the dangerous case
    let release: (r: Response) => void = () => {};
    let refreshes = 0;
    zoomAnswer = (url) => (url.includes("/users/me/token") ? Response.json({ token: "sample-zak" }) : new Response("{}", { status: 500 }));
    const slow = createSlowApp(() => {
      refreshes++;
      return new Promise<Response>((resolve) => (release = resolve));
    });
    const first = slow.call("/v1/zoom/zak", a);
    await slow.reachedZoom;
    // The second PC asks while the first is still at Zoom: it must not use the refresh token too.
    const second = slow.call("/v1/zoom/zak", b);
    await new Promise((r) => setTimeout(r, 20));
    expect(refreshes).toBe(1);
    release(Response.json({ access_token: "sample-access-2", refresh_token: "sample-refresh-2", expires_in: 3600 }));
    expect(await (await first).json()).toEqual({ zak: "sample-zak" });
    expect(await (await second).json()).toEqual({ zak: "sample-zak" });
    expect(refreshes).toBe(1);
  });

  it("drops a sign-in Zoom no longer accepts, and keeps one through a hiccup", async () => {
    const { key } = await issue();
    const token = await tokenFor(key);
    await signedIn(token);
    now += 2 * 3600;
    zoomAnswer = () => new Response("oops", { status: 503 });
    const down = await call("/v1/zoom/zak", {}, token);
    expect(down.status).toBe(502);
    expect(((await down.json()) as { error: string }).error).toBe("zoom_failed");
    // Still signed in, and not left locked: the next try goes through.
    zoomAnswer = (url) =>
      url.includes("/oauth/token")
        ? Response.json({ access_token: "sample-access-2", refresh_token: "sample-refresh-2", expires_in: 3600 })
        : Response.json({ token: "sample-zak" });
    expect((await call("/v1/zoom/zak", {}, token)).status).toBe(200);

    // Renewed, but no ZAK (the permission is missing, say): the renewed token is kept all the same.
    now += 2 * 3600;
    zoomAnswer = (url) =>
      url.includes("/oauth/token")
        ? Response.json({ access_token: "sample-access-3", refresh_token: "sample-refresh-3", expires_in: 3600 })
        : Response.json({ code: 4711, message: "Invalid access token, does not contain scopes" }, { status: 400 });
    const noZak = await call("/v1/zoom/zak", {}, token);
    expect(noZak.status).toBe(502);
    expect(((await noZak.json()) as { detail: string }).detail).toContain("does not contain scopes");
    zoomCalls = [];
    zoomAnswer = () => Response.json({ token: "sample-zak-3" });
    expect(await (await call("/v1/zoom/zak", {}, token)).json()).toEqual({ zak: "sample-zak-3" });
    expect(zoomCalls[0]!.auth).toBe("Bearer sample-access-3");

    // An access token withdrawn early: renewed once and tried again.
    now += 700;
    zoomCalls = [];
    zoomAnswer = (url, body) =>
      url.includes("/oauth/token")
        ? Response.json({ access_token: "sample-access-4", refresh_token: "sample-refresh-4", expires_in: 3600 })
        : zoomCalls.length === 1
          ? Response.json({ message: "Invalid access token" }, { status: 401 })
          : Response.json({ token: `sample-zak-4${body}` });
    expect(await (await call("/v1/zoom/zak", {}, token)).json()).toEqual({ zak: "sample-zak-4" });
    expect(zoomUrls()).toEqual(["api.zoom.us/v2/users/me/token", "zoom.us/oauth/token", "api.zoom.us/v2/users/me/token"]);

    // The user took the permission away at Zoom: signed out here too, for every PC.
    now += 2 * 3600;
    zoomAnswer = () => Response.json({ reason: "Invalid Token!" }, { status: 401 });
    const gone = await call("/v1/zoom/zak", {}, token);
    expect(gone.status).toBe(401);
    expect(((await gone.json()) as { error: string }).error).toBe("signed_out");
    expect((await call("/v1/zoom/zak", {}, token)).status).toBe(404);
    expect(((await (await call("/v1/status", {}, token)).json()) as { licence: { zoomSignedIn: boolean } }).licence.zoomSignedIn).toBe(false);
  });

  it("signs out at Zoom and here; needs its own key to be set", async () => {
    const { key } = await issue();
    const token = await tokenFor(key);
    await signedIn(token);
    zoomAnswer = () => Response.json({ status: "success" });
    expect(await (await call("/v1/zoom/signout", {}, token)).json()).toEqual({ revoked: true });
    expect(zoomCalls[0]).toMatchObject({ url: "https://zoom.us/oauth/revoke", body: "token=sample-refresh-1" });
    expect((await call("/v1/zoom/zak", {}, token)).status).toBe(404);
    expect(await (await call("/v1/zoom/signout", {}, token)).json()).toEqual({ revoked: false });

    // A sign-in sealed with a key that was replaced since is of no use to anyone.
    await signedIn(token);
    now += 2 * 3600;
    env.SIGNIN_KEY = "another-sample-signin-key-heron-9876543210";
    expect((await call("/v1/zoom/zak", {}, token)).status).toBe(401);
    for (const bad of [undefined, "too-short", "x".repeat(31), " ".repeat(40)]) {
      env.SIGNIN_KEY = bad;
      expect((await call("/v1/zoom/begin", { challenge: CHALLENGE }, token)).status).toBe(503);
      expect((await call("/v1/zoom/zak", {}, token)).status).toBe(503);
    }
  });
});

describe("the owner's page", () => {
  it("is served without secrets and may not be framed", async () => {
    const r = await app.request("/admin", {}, env);
    expect(r.status).toBe(200);
    expect(r.headers.get("x-frame-options")).toBe("DENY");
    expect(r.headers.get("cache-control")).toBe("no-store");
    const html = await r.text();
    expect(html).toContain("ライセンス");
    expect(html).not.toContain(ADMIN);
    expect(html).not.toContain("sample-secret");
    expect(html).not.toMatch(/innerHTML|document\.write|\balert\(|\bconfirm\(/);
  });
});

describe("the public pages", () => {
  it("serves privacy, terms, support and the guide, each linking to the others", async () => {
    for (const path of ["/privacy", "/terms", "/support", "/docs"]) {
      const r = await app.request(path, {}, env);
      expect(r.status).toBe(200);
      expect(r.headers.get("content-type")).toContain("text/html");
      expect(r.headers.get("x-frame-options")).toBe("DENY");
      const html = await r.text();
      expect(html).toContain("info@seventh-well.com");
      for (const other of ["/privacy", "/terms", "/support", "/docs"]) {
        if (other !== path) expect(html).toContain(`href="${other}"`);
      }
      expect(html).not.toContain(ADMIN);
      expect(html).not.toContain("sample-secret");
      expect(html).not.toContain("<script");
    }
    expect((await app.request("/privacy/", {}, env)).status).toBe(404);
  });

  it("says what is kept and how to remove the app", async () => {
    const privacy = await (await app.request("/privacy", {}, env)).text();
    expect(privacy).toContain("AES-256-GCM");
    expect(privacy).toContain("Your rights");
    const guide = await (await app.request("/docs", {}, env)).text();
    expect(guide).toContain("Removing backhaul");
    expect(guide).toContain("Added Apps");
  });
});
