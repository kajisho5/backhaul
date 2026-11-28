// Licence keys and the rules around them. Nothing here touches the network or the database.

const ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"; // Crockford base32: no I, L, O, U

/** A new key: "BH-" and four groups of five characters (100 random bits). */
export function newLicenceKey(): string {
  const bytes = crypto.getRandomValues(new Uint8Array(20));
  let text = "";
  for (const b of bytes) text += ALPHABET[b & 31];
  return `BH-${text.slice(0, 5)}-${text.slice(5, 10)}-${text.slice(10, 15)}-${text.slice(15, 20)}`;
}

/**
 * What the operator typed, as the key was issued: upper case, without spaces and
 * hyphens, with the letters people confuse mapped back. Null when it cannot be a key.
 */
export function normaliseKey(typed: string): string | null {
  let text = typed.toUpperCase().replace(/[\s\-‐-―ー]/g, "");
  if (text.startsWith("BH")) text = text.slice(2);
  text = text.replace(/O/g, "0").replace(/[IL]/g, "1");
  if (text.length !== 20) return null;
  for (const ch of text) if (!ALPHABET.includes(ch)) return null;
  return `BH-${text.slice(0, 5)}-${text.slice(5, 10)}-${text.slice(10, 15)}-${text.slice(15, 20)}`;
}

export async function sha256Hex(text: string): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
  return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

export function randomToken(bytes = 32): string {
  return base64Url(crypto.getRandomValues(new Uint8Array(bytes)));
}

export function base64Url(bytes: Uint8Array): string {
  let binary = "";
  for (const b of bytes) binary += String.fromCharCode(b);
  return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_").replace(/=+$/, "");
}

export interface Licence {
  id: number;
  keyTail: string;
  note: string;
  seats: number;
  expiresAt: number | null;
  revoked: boolean;
  createdAt: number;
}

export type LicenceProblem = "revoked" | "expired" | null;

export function problemOf(licence: Licence, now: number): LicenceProblem {
  if (licence.revoked) return "revoked";
  if (licence.expiresAt !== null && now >= licence.expiresAt) return "expired";
  return null;
}

/** Compares without giving away, through timing, how much of the text matched. */
export function sameText(a: string, b: string): boolean {
  const x = new TextEncoder().encode(a);
  const y = new TextEncoder().encode(b);
  let diff = x.length ^ y.length;
  for (let i = 0; i < Math.max(x.length, y.length); i++) diff |= (x[i] ?? 0) ^ (y[i] ?? 0);
  return diff === 0;
}
