// Asking the licence server from a page.

export interface Answer {
  /** The HTTP status, or 0 when the server could not be reached at all. */
  status: number;
  body: Record<string, unknown>;
  /** The server's word for what went wrong ("slow_down", …), or "network". Empty when all is well. */
  error: string;
}

export async function ask(gate: string, what: "checkout" | "order" | "trial", body: unknown): Promise<Answer> {
  try {
    const answer = await fetch(`${gate}/v1/shop/${what}`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(body),
    });
    const parsed: unknown = await answer.json().catch(() => null);
    const json = parsed !== null && typeof parsed === "object" ? (parsed as Record<string, unknown>) : {};
    const error = answer.ok ? "" : typeof json.error === "string" ? json.error : "network";
    return { status: answer.status, body: json, error };
  } catch {
    return { status: 0, body: {}, error: "network" };
  }
}
