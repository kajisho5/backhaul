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
});

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

beforeEach(() => {
  now = 1_800_000_000;
  env = { DB: memoryD1(), PUBLIC_URL: "https://gate.example.invalid", ZOOM_CLIENT_ID: "SampleClientId", ZOOM_CLIENT_SECRET: "sample-secret", ADMIN_TOKEN: ADMIN };
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

    zoomAnswer = () => Response.json({ access_token: "sample-access", refresh_token: "sample-refresh-1" });
    const done = await call("/v1/zoom/collect", { state, verifier: VERIFIER }, token);
    expect(done.status).toBe(200);
    expect(await done.json()).toEqual({ refreshToken: "sample-refresh-1" });
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

  it("turns the PC's sign-in into a ZAK, and passes on the new refresh token", async () => {
    const { key } = await issue();
    const token = await tokenFor(key);
    zoomAnswer = (url) =>
      url.includes("/oauth/token")
        ? Response.json({ access_token: "sample-access-2", refresh_token: "sample-refresh-2" })
        : Response.json({ token: "sample-zak" });
    const r = await call("/v1/zoom/zak", { refreshToken: "sample-refresh-1" }, token);
    expect(r.status).toBe(200);
    expect(await r.json()).toEqual({ zak: "sample-zak", refreshToken: "sample-refresh-2" });
    expect(Object.fromEntries(new URLSearchParams(zoomCalls[0]!.body))).toEqual({ grant_type: "refresh_token", refresh_token: "sample-refresh-1" });
    expect(zoomCalls[1]).toMatchObject({ url: "https://api.zoom.us/v2/users/me/token?type=zak", auth: "Bearer sample-access-2" });
  });

  it("says when the sign-in is no longer good, and never loses a new refresh token", async () => {
    const { key } = await issue();
    const token = await tokenFor(key);
    zoomAnswer = () => Response.json({ reason: "Invalid Token!" }, { status: 401 });
    const gone = await call("/v1/zoom/zak", { refreshToken: "sample-refresh-old" }, token);
    expect(gone.status).toBe(401);
    expect(((await gone.json()) as { error: string }).error).toBe("signed_out");

    zoomAnswer = () => new Response("oops", { status: 503 });
    const down = await call("/v1/zoom/zak", { refreshToken: "sample-refresh-1" }, token);
    expect(down.status).toBe(502);
    expect(((await down.json()) as { error: string }).error).toBe("zoom_failed");

    // Refreshed, but no ZAK (the permission is missing, say): the new refresh token still reaches the PC.
    zoomAnswer = (url) =>
      url.includes("/oauth/token")
        ? Response.json({ access_token: "a", refresh_token: "sample-refresh-3" })
        : Response.json({ code: 4711, message: "Invalid access token, does not contain scopes" }, { status: 400 });
    const noZak = await call("/v1/zoom/zak", { refreshToken: "sample-refresh-2" }, token);
    expect(noZak.status).toBe(502);
    expect(await noZak.json()).toMatchObject({ error: "zoom_failed", refreshToken: "sample-refresh-3" });
    expect((await call("/v1/zoom/zak", {}, token)).status).toBe(400);
  });

  it("signs out at Zoom", async () => {
    const { key } = await issue();
    const token = await tokenFor(key);
    zoomAnswer = () => Response.json({ status: "success" });
    expect(await (await call("/v1/zoom/signout", { refreshToken: "sample-refresh-1" }, token)).json()).toEqual({ revoked: true });
    expect(zoomCalls[0]).toMatchObject({ url: "https://zoom.us/oauth/revoke", body: "token=sample-refresh-1" });
    zoomAnswer = () => new Response("{}", { status: 400 });
    expect(await (await call("/v1/zoom/signout", { refreshToken: "sample-refresh-1" }, token)).json()).toEqual({ revoked: false });
  });
});
