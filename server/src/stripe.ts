// Everything that talks to Stripe. No Stripe library: the service makes five kinds of
// request, and checks the signature of what Stripe sends, with what Workers already have.
import { sameText } from "./licence";
import type { Fetcher } from "./zoom";

/** The API version every request asks for, so that a change at Stripe cannot change the answers. */
export const STRIPE_VERSION = "2026-11-18.endive";

/** Consumption tax on payments in yen. When the rate changes, change both lines: a new rate is then made at Stripe. */
export const JAPAN_TAX_PERCENT = 10;
const JAPAN_TAX_MARK = "jct10";

/** How old a message from Stripe may be (Stripe's own libraries use five minutes). */
export const WEBHOOK_TOLERANCE_SECONDS = 300;

export class StripeError extends Error {
  constructor(
    message: string,
    readonly status: number,
  ) {
    super(message);
  }
}

function hex(bytes: Uint8Array): string {
  return [...bytes].map((b) => b.toString(16).padStart(2, "0")).join("");
}

/**
 * True when `header` (Stripe-Signature) proves that Stripe sent exactly `body`, recently.
 * The header reads "t=<unix seconds>,v1=<hex>[,v1=<hex>…]"; the signed text is "<t>.<body>",
 * HMAC-SHA256 with the endpoint's secret. Other entries (v0 and whatever comes later) are ignored.
 */
export async function verifyWebhook(body: string, header: string, secret: string, now: number): Promise<boolean> {
  if (secret.length === 0) return false;
  let timestamp: number | null = null;
  const signatures: string[] = [];
  for (const part of header.split(",")) {
    const at = part.indexOf("=");
    if (at < 0) continue;
    const name = part.slice(0, at).trim();
    const value = part.slice(at + 1).trim();
    if (name === "t" && /^\d{1,12}$/.test(value)) timestamp = Number(value);
    if (name === "v1") signatures.push(value);
  }
  if (timestamp === null || signatures.length === 0) return false;
  if (Math.abs(now - timestamp) > WEBHOOK_TOLERANCE_SECONDS) return false;
  const text = new TextEncoder();
  const key = await crypto.subtle.importKey("raw", text.encode(secret), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const expected = hex(new Uint8Array(await crypto.subtle.sign("HMAC", key, text.encode(`${timestamp}.${body}`))));
  let good = false;
  for (const signature of signatures) if (sameText(signature, expected)) good = true; // every entry is compared
  return good;
}

type Json = Record<string, unknown>;

function isObject(value: unknown): value is Json {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

export class Stripe {
  constructor(
    private readonly secretKey: string,
    private readonly fetcher: Fetcher,
  ) {}

  private async send(method: "GET" | "POST", path: string, fields: [string, string][], idempotencyKey?: string): Promise<Json> {
    const encoded = new URLSearchParams(fields).toString();
    const headers: Record<string, string> = { authorization: `Bearer ${this.secretKey}`, "stripe-version": STRIPE_VERSION };
    if (idempotencyKey !== undefined) headers["idempotency-key"] = idempotencyKey;
    let url = `https://api.stripe.com${path}`;
    let body: string | undefined;
    if (method === "GET") {
      if (encoded.length > 0) url += `?${encoded}`;
    } else {
      headers["content-type"] = "application/x-www-form-urlencoded";
      body = encoded;
    }
    const answer = await this.fetcher(url, { method, headers, body });
    const json: unknown = await answer.json().catch(() => null);
    if (!answer.ok || !isObject(json)) {
      const error = isObject(json) && isObject(json.error) ? json.error : null;
      const message = typeof error?.message === "string" ? error.message : `Stripe answered ${answer.status}`;
      throw new StripeError(message, answer.status);
    }
    return json;
  }

  /** The id of the price that carries `lookupKey`, or null when there is none. */
  async priceId(lookupKey: string): Promise<string | null> {
    const list = await this.send("GET", "/v1/prices", [
      ["lookup_keys[0]", lookupKey],
      ["active", "true"],
      ["limit", "1"],
    ]);
    const first = Array.isArray(list.data) ? (list.data[0] as unknown) : null;
    return isObject(first) && typeof first.id === "string" ? first.id : null;
  }

  /**
   * The id of the tax rate added to payments in yen: Japanese consumption tax, 10 %, on top of
   * the price. It is looked up at Stripe by its mark and made there if it does not exist yet,
   * so the test environment and the real one need no setting of their own.
   */
  async japanTaxRate(): Promise<string> {
    const list = await this.send("GET", "/v1/tax_rates", [
      ["active", "true"],
      ["inclusive", "false"],
      ["limit", "100"],
    ]);
    for (const rate of Array.isArray(list.data) ? (list.data as unknown[]) : []) {
      if (!isObject(rate) || typeof rate.id !== "string") continue;
      const mark = isObject(rate.metadata) ? rate.metadata.backhaul : null;
      if (mark === JAPAN_TAX_MARK && rate.percentage === JAPAN_TAX_PERCENT) return rate.id;
    }
    const made = await this.send(
      "POST",
      "/v1/tax_rates",
      [
        ["display_name", "消費税"],
        ["description", "Japanese consumption tax, added to payments in yen"],
        ["percentage", String(JAPAN_TAX_PERCENT)],
        ["inclusive", "false"],
        ["country", "JP"],
        ["jurisdiction", "JP"],
        ["tax_type", "jct"],
        ["metadata[backhaul]", JAPAN_TAX_MARK],
      ],
      `backhaul-${JAPAN_TAX_MARK}`, // two first purchases in the same moment make one rate, not two
    );
    if (typeof made.id !== "string") throw new StripeError("Stripe gave no tax rate", 502);
    return made.id;
  }

  /** Opens Stripe's payment page for one subscription; returns the address to send the buyer to. */
  async startCheckout(o: {
    priceId: string;
    currency: "jpy" | "usd";
    lang: "en" | "ja";
    taxRate: string | null;
    successUrl: string;
    cancelUrl: string;
  }): Promise<string> {
    const fields: [string, string][] = [
      ["mode", "subscription"],
      ["line_items[0][price]", o.priceId],
      ["line_items[0][quantity]", "1"],
      ["currency", o.currency],
      ["locale", o.lang],
      ["success_url", o.successUrl],
      ["cancel_url", o.cancelUrl],
      ["billing_address_collection", "required"],
      ["subscription_data[metadata][lang]", o.lang],
      // SEVENTHWELL is the seller, with its own tax rate and its own legal page. A Stripe
      // account can have "Managed Payments" (Stripe as the seller) on by default; such a
      // session refuses products without a tax code, so it is switched off here.
      ["managed_payments[enabled]", "false"],
    ];
    if (o.taxRate !== null) fields.push(["subscription_data[default_tax_rates][0]", o.taxRate]);
    const session = await this.send("POST", "/v1/checkout/sessions", fields);
    if (typeof session.url !== "string") throw new StripeError("Stripe gave no payment page", 502);
    return session.url;
  }

  /** What became of a payment page: null when Stripe knows no such session. */
  async checkout(sessionId: string): Promise<{ complete: boolean; subscriptionId: string | null; email: string | null } | null> {
    let session: Json;
    try {
      session = await this.send("GET", `/v1/checkout/sessions/${encodeURIComponent(sessionId)}`, []);
    } catch (error) {
      if (error instanceof StripeError && error.status === 404) return null;
      throw error;
    }
    const details = isObject(session.customer_details) ? session.customer_details : null;
    return {
      complete: session.status === "complete" && session.payment_status !== "unpaid",
      subscriptionId: typeof session.subscription === "string" ? session.subscription : null,
      email: typeof details?.email === "string" ? details.email : null,
    };
  }

  async subscription(id: string): Promise<Subscription> {
    const sub = await this.send("GET", `/v1/subscriptions/${encodeURIComponent(id)}`, [["expand[0]", "customer"]]);
    return readSubscription(sub);
  }
}

/** The part of a Stripe subscription the licence depends on. */
export interface Subscription {
  id: string;
  status: string;
  /** PCs the plan allows; null when the price says nothing (then no licence is made or changed). */
  seats: number | null;
  periodStart: number;
  periodEnd: number;
  email: string | null;
  lang: "en" | "ja";
}

export function readSubscription(sub: Json): Subscription {
  const items = isObject(sub.items) && Array.isArray(sub.items.data) ? (sub.items.data as unknown[]) : [];
  const item = isObject(items[0]) ? items[0] : null;
  const price = isObject(item?.price) ? item.price : null;
  const priceData = isObject(price?.metadata) ? price.metadata : null;
  const perUnit = typeof priceData?.seats === "string" && /^\d{1,3}$/.test(priceData.seats) ? Number(priceData.seats) : null;
  const quantity = typeof item?.quantity === "number" && Number.isInteger(item.quantity) && item.quantity > 0 ? item.quantity : 1;
  const customer = isObject(sub.customer) ? sub.customer : null;
  const data = isObject(sub.metadata) ? sub.metadata : null;
  if (typeof sub.id !== "string" || typeof sub.status !== "string") throw new StripeError("not a subscription", 502);
  return {
    id: sub.id,
    status: sub.status,
    seats: perUnit === null || perUnit < 1 ? null : perUnit * quantity,
    periodStart: typeof item?.current_period_start === "number" ? item.current_period_start : 0,
    periodEnd: typeof item?.current_period_end === "number" ? item.current_period_end : 0,
    email: typeof customer?.email === "string" ? customer.email : null,
    lang: data?.lang === "ja" ? "ja" : "en",
  };
}
