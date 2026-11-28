// Encrypts what is kept of a Zoom sign-in. The Worker secret SIGNIN_KEY is any random text of
// 32 characters or more; the AES key is its SHA-256. Each value gets its own random 12-byte
// start value (IV).

/** Shorter secrets are refused: they would be too easy to guess. */
export const MIN_SIGNIN_KEY_LENGTH = 32;

function fromBase64(text: string): Uint8Array | null {
  try {
    return Uint8Array.from(atob(text.replace(/-/g, "+").replace(/_/g, "/")), (ch) => ch.charCodeAt(0));
  } catch {
    return null;
  }
}

function toBase64(bytes: Uint8Array): string {
  let binary = "";
  for (const b of bytes) binary += String.fromCharCode(b);
  return btoa(binary);
}

export class Vault {
  private constructor(private readonly key: CryptoKey) {}

  /** Null when the secret is missing or shorter than MIN_SIGNIN_KEY_LENGTH. */
  static async open(secret: string | undefined): Promise<Vault | null> {
    const text = secret?.trim() ?? "";
    if (text.length < MIN_SIGNIN_KEY_LENGTH) return null;
    const raw = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(text));
    return new Vault(await crypto.subtle.importKey("raw", raw, "AES-GCM", false, ["encrypt", "decrypt"]));
  }

  async seal(text: string): Promise<string> {
    const iv = crypto.getRandomValues(new Uint8Array(12));
    const sealed = new Uint8Array(await crypto.subtle.encrypt({ name: "AES-GCM", iv }, this.key, new TextEncoder().encode(text)));
    const all = new Uint8Array(iv.length + sealed.length);
    all.set(iv);
    all.set(sealed, iv.length);
    return toBase64(all);
  }

  /** Null when the value was not sealed with this key (or was changed). */
  async unseal(sealed: string | null): Promise<string | null> {
    const all = sealed ? fromBase64(sealed) : null;
    if (all === null || all.length < 13) return null;
    try {
      const plain = await crypto.subtle.decrypt({ name: "AES-GCM", iv: all.slice(0, 12) }, this.key, all.slice(12));
      return new TextDecoder().decode(plain);
    } catch {
      return null;
    }
  }
}
