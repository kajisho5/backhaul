// Everything that talks to Zoom or is made for Zoom. `fetcher` is the network, so that
// tests can stand in for Zoom.
import { base64Url } from "./licence";

export type Fetcher = (input: string, init?: RequestInit) => Promise<Response>;

export interface ZoomKeys {
  clientId: string;
  clientSecret: string;
}

export const SDK_TOKEN_SECONDS = 24 * 60 * 60; // Zoom: at least 30 minutes, at most 48 hours

/** The token the Meeting SDK is authorised with (HS256, signed with the Client Secret). */
export async function makeSdkToken(keys: ZoomKeys, now: number): Promise<{ token: string; expiresAt: number }> {
  const issuedAt = now - 30; // a PC clock that runs a little behind must not see a token "from the future"
  const expiresAt = now + SDK_TOKEN_SECONDS;
  const text = new TextEncoder();
  const header = base64Url(text.encode(JSON.stringify({ alg: "HS256", typ: "JWT" })));
  const payload = base64Url(
    text.encode(JSON.stringify({ appKey: keys.clientId, iat: issuedAt, exp: expiresAt, tokenExp: expiresAt })),
  );
  const key = await crypto.subtle.importKey("raw", text.encode(keys.clientSecret), { name: "HMAC", hash: "SHA-256" }, false, [
    "sign",
  ]);
  const signature = new Uint8Array(await crypto.subtle.sign("HMAC", key, text.encode(`${header}.${payload}`)));
  return { token: `${header}.${payload}.${base64Url(signature)}`, expiresAt };
}

/** Where the browser is sent to sign in. `challenge` is the PC's PKCE challenge (S256). */
export function authoriseUrl(keys: ZoomKeys, redirectUri: string, state: string, challenge: string): string {
  const url = new URL("https://zoom.us/oauth/authorize");
  url.searchParams.set("response_type", "code");
  url.searchParams.set("client_id", keys.clientId);
  url.searchParams.set("redirect_uri", redirectUri);
  url.searchParams.set("state", state);
  url.searchParams.set("code_challenge", challenge);
  url.searchParams.set("code_challenge_method", "S256");
  return url.toString();
}

export interface ZoomTokens {
  accessToken: string;
  refreshToken: string;
  expiresIn: number; // seconds the access token lasts
}

/** Why a call to Zoom did not give what was asked for. `signedOut`: the user has to sign in again. */
export class ZoomError extends Error {
  constructor(
    message: string,
    readonly signedOut: boolean,
  ) {
    super(message);
  }
}

async function tokenRequest(keys: ZoomKeys, fetcher: Fetcher, form: Record<string, string>): Promise<ZoomTokens> {
  const response = await fetcher("https://zoom.us/oauth/token", {
    method: "POST",
    headers: {
      authorization: `Basic ${btoa(`${keys.clientId}:${keys.clientSecret}`)}`,
      "content-type": "application/x-www-form-urlencoded",
    },
    body: new URLSearchParams(form).toString(),
  });
  const body = (await response.json().catch(() => null)) as Record<string, unknown> | null;
  if (!response.ok || body === null) {
    // 400/401 here means the code or the refresh token is no longer good. Anything else
    // (429 "too many requests", 5xx) says nothing about the sign-in, which is then kept.
    const reason = typeof body?.reason === "string" ? body.reason : typeof body?.error === "string" ? body.error : "";
    throw new ZoomError(`Zoom refused the sign-in (${response.status}${reason ? `: ${reason}` : ""})`, response.status === 400 || response.status === 401);
  }
  if (typeof body.access_token !== "string" || typeof body.refresh_token !== "string") {
    throw new ZoomError("Zoom answered without tokens", false);
  }
  const expiresIn = typeof body.expires_in === "number" && body.expires_in > 0 ? body.expires_in : 0;
  return { accessToken: body.access_token, refreshToken: body.refresh_token, expiresIn };
}

export function exchangeCode(keys: ZoomKeys, fetcher: Fetcher, code: string, verifier: string, redirectUri: string) {
  return tokenRequest(keys, fetcher, {
    grant_type: "authorization_code",
    code,
    redirect_uri: redirectUri,
    code_verifier: verifier,
  });
}

/** Zoom gives a new refresh token every time; the old one stops working. */
export function refresh(keys: ZoomKeys, fetcher: Fetcher, refreshToken: string) {
  return tokenRequest(keys, fetcher, { grant_type: "refresh_token", refresh_token: refreshToken });
}

/** The signed-in user's ZAK: what lets the Meeting SDK join as that user. */
export async function fetchZak(fetcher: Fetcher, accessToken: string): Promise<string> {
  const response = await fetcher("https://api.zoom.us/v2/users/me/token?type=zak", {
    headers: { authorization: `Bearer ${accessToken}` },
  });
  const body = (await response.json().catch(() => null)) as Record<string, unknown> | null;
  if (!response.ok || body === null || typeof body.token !== "string" || body.token.length === 0) {
    const message = typeof body?.message === "string" ? body.message : "";
    throw new ZoomError(`Zoom did not give a ZAK (${response.status}${message ? `: ${message}` : ""})`, response.status === 401);
  }
  return body.token;
}

export async function revoke(keys: ZoomKeys, fetcher: Fetcher, token: string): Promise<boolean> {
  const response = await fetcher("https://zoom.us/oauth/revoke", {
    method: "POST",
    headers: {
      authorization: `Basic ${btoa(`${keys.clientId}:${keys.clientSecret}`)}`,
      "content-type": "application/x-www-form-urlencoded",
    },
    body: new URLSearchParams({ token }).toString(),
  });
  return response.ok;
}
