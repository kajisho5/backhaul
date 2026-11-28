import { beforeEach, describe, expect, it } from "vitest";
import { createApp } from "../src/index";
import type { Env } from "../src/index";
import { keyMail, looksLikeEmail, trialIdentity } from "../src/mail";
import { GRACE_SECONDS, expiryOf } from "../src/shop";
import { STRIPE_VERSION, readSubscription, verifyWebhook } from "../src/stripe";
import { memoryD1 } from "./d1";

const ADMIN = "sample-admin-token-0123456789";
const SITE = "https://shop.example.invalid";
const WEBHOOK_SECRET = "whsec_sample_otter_0123456789";
const DAY = 86400;

let now = 1_800_000_000;
let env: Env;

/** The stand-in for Stripe: subscriptions and payment pages by id, prices by lookup key. */
interface FakeSub {
  status: string;
  seats: string | null;
  quantity: number;
  periodStart: number;
  periodEnd: number;
  email: string | null;
  lang?: string;
}
let subs: Record<string, FakeSub>;
let sessions: Record<string, { status: string; payment_status: string; subscription: string | null; email: string }>;
let prices: Record<string, string>;
let taxRates: { id: string; percentage: number; metadata: Record<string, string> }[];
let stripeCalls: { method: string; url: string; body: string; headers: Headers }[];
let mails: { to: string[]; subject: string; text: string; from: string; reply_to: string; once: string }[];
let mailWorks: boolean;

function subJson(id: string, s: FakeSub) {
  return {
    id,
    object: "subscription",
    status: s.status,
    metadata: s.lang ? { lang: s.lang } : {},
    customer: { id: "cus_SampleOtter", email: s.email },
    items: {
      data: [
        {
          quantity: s.quantity,
          current_period_start: s.periodStart,
          current_period_end: s.periodEnd,
          price: { id: "price_Sample", metadata: s.seats === null ? {} : { seats: s.seats } },
        },
      ],
    },
  };
}

const json = (value: unknown, status = 200) => new Response(JSON.stringify(value), { status, headers: { "content-type": "application/json" } });

const app = createApp({
  now: () => now,
  fetcher: async (url, init) => {
    const headers = new Headers(init?.headers);
    const body = typeof init?.body === "string" ? init.body : "";
    if (url === "https://api.resend.com/emails") {
      if (!mailWorks) return json({ message: "down" }, 500);
      const sent = JSON.parse(body) as { to: string[]; subject: string; text: string; from: string; reply_to: string };
      const once = headers.get("idempotency-key") ?? "";
      if (!mails.some((m) => m.once === once)) mails.push({ ...sent, once }); // the same name is delivered once
      return json({ id: "sample-mail-id" });
    }
    stripeCalls.push({ method: init?.method ?? "GET", url, body, headers });
    const at = new URL(url);
    if (at.pathname === "/v1/prices") {
      const id = prices[at.searchParams.get("lookup_keys[0]") ?? ""];
      return json({ object: "list", data: id ? [{ id }] : [] });
    }
    if (at.pathname === "/v1/tax_rates") {
      if (init?.method === "POST") {
        const sent = new URLSearchParams(body);
        const made = { id: `txr_Sample${taxRates.length + 1}`, percentage: Number(sent.get("percentage")), metadata: { backhaul: sent.get("metadata[backhaul]") ?? "" } };
        taxRates.push(made);
        return json(made);
      }
      return json({ object: "list", data: taxRates });
    }
    if (at.pathname === "/v1/checkout/sessions" && init?.method === "POST") {
      return json({ id: "cs_test_SampleNew0001", url: "https://checkout.stripe.example.invalid/pay/cs_test_SampleNew0001" });
    }
    if (at.pathname.startsWith("/v1/checkout/sessions/")) {
      const s = sessions[at.pathname.split("/").pop() ?? ""];
      if (!s) return json({ error: { message: "No such checkout.session" } }, 404);
      return json({ id: "cs", status: s.status, payment_status: s.payment_status, subscription: s.subscription, customer_details: { email: s.email } });
    }
    if (at.pathname.startsWith("/v1/subscriptions/")) {
      const id = at.pathname.split("/").pop() ?? "";
      const s = subs[id];
      return s ? json(subJson(id, s)) : json({ error: { message: "No such subscription" } }, 404);
    }
    return json({ error: { message: "unexpected request" } }, 500);
  },
  sleep: async () => {},
});

function call(path: string, body?: unknown, o: { token?: string; ip?: string; origin?: string; method?: string } = {}) {
  const headers: Record<string, string> = { "content-type": "application/json", "cf-connecting-ip": o.ip ?? "192.0.2.7" };
  if (o.token !== undefined) headers.authorization = `Bearer ${o.token}`;
  if (o.origin !== undefined) headers.origin = o.origin;
  return app.request(path, { method: o.method ?? "POST", headers, body: body === undefined ? undefined : JSON.stringify(body) }, env);
}

async function sign(body: string, at = now, secret = WEBHOOK_SECRET): Promise<string> {
  const text = new TextEncoder();
  const key = await crypto.subtle.importKey("raw", text.encode(secret), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const mac = new Uint8Array(await crypto.subtle.sign("HMAC", key, text.encode(`${at}.${body}`)));
  return `t=${at},v1=${[...mac].map((b) => b.toString(16).padStart(2, "0")).join("")}`;
}

/** Stripe telling about a subscription. */
async function tell(type: string, object: Record<string, unknown>) {
  const body = JSON.stringify({ id: "evt_Sample", type, data: { object } });
  return app.request("/v1/shop/webhook", { method: "POST", headers: { "stripe-signature": await sign(body) }, body }, env);
}

async function licences() {
  const r = await call("/admin/licences", undefined, { token: ADMIN, method: "GET" });
  return ((await r.json()) as { licences: { id: number; seats: number; expiresAt: number | null; note: string; problem: string | null; devices: { name: string }[] }[] }).licences;
}

/** The n-th item, which the test expects to be there. */
function at<T>(list: T[], index: number): T {
  const item = list[index];
  if (item === undefined) throw new Error(`nothing at ${index}`);
  return item;
}

function keyIn(text: string): string {
  const found = /BH-[0-9A-Z]{5}-[0-9A-Z]{5}-[0-9A-Z]{5}-[0-9A-Z]{5}/.exec(text);
  expect(found).not.toBeNull();
  return at([...(found ?? [])], 0);
}

async function activate(key: string, deviceId: string, name: string) {
  return call("/v1/activate", { key, deviceId, deviceName: name });
}

const paid = (over: Partial<FakeSub> = {}): FakeSub => ({ status: "active", seats: "5", quantity: 1, periodStart: now, periodEnd: now + 30 * DAY, email: "buyer@otter.example", ...over });

beforeEach(() => {
  now = 1_800_000_000;
  env = {
    DB: memoryD1(),
    PUBLIC_URL: "https://gate.example.invalid",
    ADMIN_TOKEN: ADMIN,
    SIGNIN_KEY: "sample-signin-key-otter-0123456789-abcdef",
    SITE_URL: SITE,
    MAIL_FROM: "backhaul <licence@otter.example>",
    BILLING_URL: "https://billing.example.invalid/p/login/sample",
    STRIPE_SECRET_KEY: "sk_test_sample_otter",
    STRIPE_WEBHOOK_SECRET: WEBHOOK_SECRET,
    RESEND_API_KEY: "re_sample_otter",
  };
  subs = {};
  sessions = {};
  prices = { backhaul_5_month: "price_5m", backhaul_10_year: "price_10y" };
  taxRates = [{ id: "txr_SampleOther", percentage: 8, metadata: {} }];
  stripeCalls = [];
  mails = [];
  mailWorks = true;
});

describe("messages from Stripe", () => {
  it("are accepted only with a fresh, correct signature", async () => {
    const body = '{"id":"evt_Sample"}';
    expect(await verifyWebhook(body, await sign(body), WEBHOOK_SECRET, now)).toBe(true);
    expect(await verifyWebhook(body + " ", await sign(body), WEBHOOK_SECRET, now)).toBe(false);
    expect(await verifyWebhook(body, await sign(body, now, "whsec_another"), WEBHOOK_SECRET, now)).toBe(false);
    expect(await verifyWebhook(body, await sign(body, now - 301), WEBHOOK_SECRET, now)).toBe(false);
    expect(await verifyWebhook(body, await sign(body, now - 299), WEBHOOK_SECRET, now)).toBe(true);
    expect(await verifyWebhook(body, "", WEBHOOK_SECRET, now)).toBe(false);
    expect(await verifyWebhook(body, "t=abc,v1=00", WEBHOOK_SECRET, now)).toBe(false);
    expect(await verifyWebhook(body, await sign(body), "", now)).toBe(false);
  });

  it("may carry several signatures (while a secret is being replaced) and other schemes", async () => {
    const body = "{}";
    const good = at((await sign(body)).split(",v1="), 1);
    expect(await verifyWebhook(body, `t=${now},v0=ffff,v1=${"0".repeat(64)},v1=${good}`, WEBHOOK_SECRET, now)).toBe(true);
    expect(await verifyWebhook(body, `t=${now},v0=${good}`, WEBHOOK_SECRET, now)).toBe(false);
  });

  it("are refused at the door when the signature is wrong, and nothing is asked of Stripe", async () => {
    subs.sub_SampleA1 = paid();
    const body = JSON.stringify({ type: "customer.subscription.created", data: { object: { id: "sub_SampleA1" } } });
    const r = await app.request("/v1/shop/webhook", { method: "POST", headers: { "stripe-signature": await sign(body, now, "whsec_wrong") }, body }, env);
    expect(r.status).toBe(400);
    expect(stripeCalls).toHaveLength(0);
    expect(await licences()).toHaveLength(0);
  });

  it("about other things are answered and ignored", async () => {
    const r = await tell("charge.succeeded", { id: "ch_Sample" });
    expect(r.status).toBe(200);
    expect(await r.json()).toEqual({ ignored: true });
  });
});

describe("a paid subscription", () => {
  it("becomes one licence with the plan's PCs, and the key goes out by mail once", async () => {
    subs.sub_SampleA1 = paid({ lang: "ja" });
    // Stripe sends several messages about one purchase, in no fixed order.
    expect((await tell("customer.subscription.created", { id: "sub_SampleA1" })).status).toBe(200);
    expect((await tell("checkout.session.completed", { id: "cs_test_x", subscription: "sub_SampleA1" })).status).toBe(200);
    expect((await tell("customer.subscription.updated", { id: "sub_SampleA1" })).status).toBe(200);

    const list = await licences();
    expect(list).toHaveLength(1);
    expect(at(list, 0)).toMatchObject({ seats: 5, expiresAt: now + 30 * DAY + GRACE_SECONDS, note: "Stripe sub_SampleA1", problem: null });
    expect(mails).toHaveLength(1);
    expect(at(mails, 0)).toMatchObject({ to: ["buyer@otter.example"], from: "backhaul <licence@otter.example>", reply_to: "info@seventh-well.com", subject: "backhaul のライセンスキー" });
    expect(at(mails, 0).text).toContain("https://billing.example.invalid/p/login/sample");
    expect(at(mails, 0).text).toContain(`${SITE}/ja/start/`);
    expect((await activate(keyIn(at(mails, 0).text), "sample-device-aaaaaaaa", "SAMPLE-PC-01")).status).toBe(200);
  });

  it("asks Stripe with the secret key and a fixed API version", async () => {
    subs.sub_SampleA1 = paid();
    await tell("customer.subscription.created", { id: "sub_SampleA1" });
    expect(at(stripeCalls, 0).url).toBe("https://api.stripe.com/v1/subscriptions/sub_SampleA1?expand%5B0%5D=customer");
    expect(at(stripeCalls, 0).headers.get("authorization")).toBe("Bearer sk_test_sample_otter");
    expect(at(stripeCalls, 0).headers.get("stripe-version")).toBe(STRIPE_VERSION);
  });

  it("is not trusted from the message: what Stripe itself says counts", async () => {
    subs.sub_SampleA1 = paid({ status: "incomplete" });
    // The message claims "active"; Stripe says the first payment has not happened.
    await tell("customer.subscription.updated", { id: "sub_SampleA1", status: "active" });
    expect(await licences()).toHaveLength(0);
    expect(mails).toHaveLength(0);
  });

  it("with a price that does not say its PCs makes no licence and reports the fault", async () => {
    subs.sub_SampleA1 = paid({ seats: null });
    expect((await tell("customer.subscription.created", { id: "sub_SampleA1" })).status).toBe(500);
    expect(await licences()).toHaveLength(0);
  });

  it("without a paid period makes no licence either", async () => {
    subs.sub_SampleA1 = paid({ periodEnd: 0 });
    expect((await tell("customer.subscription.created", { id: "sub_SampleA1" })).status).toBe(500);
    expect(await licences()).toHaveLength(0);
  });

  it("keeps the request failing while the mail cannot be sent, and sends it on a later message", async () => {
    subs.sub_SampleA1 = paid();
    mailWorks = false;
    expect((await tell("customer.subscription.created", { id: "sub_SampleA1" })).status).toBe(500);
    expect(await licences()).toHaveLength(1); // the licence exists already
    mailWorks = true;
    expect((await tell("customer.subscription.created", { id: "sub_SampleA1" })).status).toBe(200);
    expect(mails).toHaveLength(1);
    expect(await licences()).toHaveLength(1);
  });

  it("is extended when a renewal is paid", async () => {
    subs.sub_SampleA1 = paid();
    await tell("customer.subscription.created", { id: "sub_SampleA1" });
    now += 30 * DAY;
    subs.sub_SampleA1 = paid({ periodStart: now, periodEnd: now + 30 * DAY });
    await tell("customer.subscription.updated", { id: "sub_SampleA1" });
    expect(at(await licences(), 0).expiresAt).toBe(now + 30 * DAY + GRACE_SECONDS);
    expect(mails).toHaveLength(1); // no second key mail
  });

  it("gets three days when the renewal fails, counted from the start of the unpaid period", async () => {
    subs.sub_SampleA1 = paid();
    await tell("customer.subscription.created", { id: "sub_SampleA1" });
    const key = keyIn(at(mails, 0).text);
    now += 30 * DAY;
    subs.sub_SampleA1 = paid({ status: "past_due", periodStart: now, periodEnd: now + 30 * DAY });
    await tell("customer.subscription.updated", { id: "sub_SampleA1" });
    expect(at(await licences(), 0)).toMatchObject({ expiresAt: now + GRACE_SECONDS, problem: null });
    now += GRACE_SECONDS;
    expect((await activate(key, "sample-device-aaaaaaaa", "SAMPLE-PC-01")).status).toBe(403);
    // Paid after all: it works again.
    subs.sub_SampleA1 = paid({ periodStart: now - GRACE_SECONDS, periodEnd: now - GRACE_SECONDS + 30 * DAY });
    await tell("customer.subscription.updated", { id: "sub_SampleA1" });
    expect((await activate(key, "sample-device-aaaaaaaa", "SAMPLE-PC-01")).status).toBe(200);
  });

  it("ends the licence when the subscription has ended", async () => {
    subs.sub_SampleA1 = paid();
    await tell("customer.subscription.created", { id: "sub_SampleA1" });
    now += 10 * DAY;
    subs.sub_SampleA1 = paid({ status: "canceled" });
    await tell("customer.subscription.deleted", { id: "sub_SampleA1" });
    expect(at(await licences(), 0)).toMatchObject({ expiresAt: now, problem: "expired" });
    // A late repeat of the message does not move the end.
    now += DAY;
    await tell("customer.subscription.deleted", { id: "sub_SampleA1" });
    expect(at(await licences(), 0).expiresAt).toBe(now - DAY);
  });

  it("follows a change of plan: more PCs at once, fewer PCs drop the longest unused", async () => {
    subs.sub_SampleA1 = paid({ seats: "10" });
    await tell("customer.subscription.created", { id: "sub_SampleA1" });
    const key = keyIn(at(mails, 0).text);
    for (let i = 0; i < 7; i++) {
      now += 60;
      expect((await activate(key, `sample-device-${String(i).padStart(8, "0")}`, `SAMPLE-PC-${i}`)).status).toBe(200);
    }
    subs.sub_SampleA1 = paid({ seats: "5" });
    await tell("customer.subscription.updated", { id: "sub_SampleA1" });
    const after = at(await licences(), 0);
    expect(after.seats).toBe(5);
    expect(after.devices.map((d) => d.name)).toEqual(["SAMPLE-PC-2", "SAMPLE-PC-3", "SAMPLE-PC-4", "SAMPLE-PC-5", "SAMPLE-PC-6"]);
    expect((await activate(key, "sample-device-00000000", "SAMPLE-PC-0")).status).toBe(409);
    subs.sub_SampleA1 = paid({ seats: "20" });
    await tell("customer.subscription.updated", { id: "sub_SampleA1" });
    expect((await activate(key, "sample-device-00000000", "SAMPLE-PC-0")).status).toBe(200);
  });

  it("does not undo a licence the owner stopped by hand", async () => {
    subs.sub_SampleA1 = paid();
    await tell("customer.subscription.created", { id: "sub_SampleA1" });
    const id = at(await licences(), 0).id;
    await call(`/admin/licences/${id}/revoke`, undefined, { token: ADMIN });
    await tell("customer.subscription.updated", { id: "sub_SampleA1" });
    expect(at(await licences(), 0).problem).toBe("revoked");
  });

  it("made twice in the same moment still gives one licence and one mail", async () => {
    subs.sub_SampleA1 = paid();
    const answers = await Promise.all([
      tell("customer.subscription.created", { id: "sub_SampleA1" }),
      tell("checkout.session.completed", { id: "cs_test_x", subscription: "sub_SampleA1" }),
      tell("customer.subscription.updated", { id: "sub_SampleA1" }),
    ]);
    expect(answers.map((a) => a.status)).toEqual([200, 200, 200]);
    expect(await licences()).toHaveLength(1);
    expect(mails).toHaveLength(1);
    expect((await activate(keyIn(at(mails, 0).text), "sample-device-aaaaaaaa", "SAMPLE-PC-01")).status).toBe(200);
  });
});

describe("the thank-you page", () => {
  beforeEach(() => {
    subs.sub_SampleA1 = paid();
    sessions.cs_test_SampleDone01 = { status: "complete", payment_status: "paid", subscription: "sub_SampleA1", email: "buyer@otter.example" };
  });

  it("shows the key once, even before Stripe's message arrived, and the mail carries the same key", async () => {
    const first = await call("/v1/shop/order", { sessionId: "cs_test_SampleDone01" });
    expect(first.status).toBe(200);
    expect(first.headers.get("cache-control")).toBe("no-store");
    const shown = (await first.json()) as { key: string; seats: number; mailed: boolean; email: string; keyTail: string };
    expect(shown).toMatchObject({ seats: 5, mailed: true, email: "b***@otter.example" });
    expect(shown.key).toBe(keyIn(at(mails, 0).text));
    expect(shown.keyTail).toBe(shown.key.slice(-4));

    const again = (await (await call("/v1/shop/order", { sessionId: "cs_test_SampleDone01" })).json()) as { key: string | null; keyTail: string };
    expect(again.key).toBeNull();
    expect(again.keyTail).toBe(shown.keyTail);
    expect(await licences()).toHaveLength(1);
  });

  it("still shows the key when the mail cannot be sent; the mail follows with Stripe's next message", async () => {
    mailWorks = false;
    const shown = (await (await call("/v1/shop/order", { sessionId: "cs_test_SampleDone01" })).json()) as { key: string; mailed: boolean };
    expect(shown.mailed).toBe(false);
    expect(shown.key).toMatch(/^BH-/);
    mailWorks = true;
    expect((await tell("customer.subscription.updated", { id: "sub_SampleA1" })).status).toBe(200);
    expect(keyIn(at(mails, 0).text)).toBe(shown.key);
  });

  it("keeps no copy of the key once the buyer has it", async () => {
    await call("/v1/shop/order", { sessionId: "cs_test_SampleDone01" });
    const row = await env.DB.prepare("SELECT key_cipher, mailed_at, shown_at FROM subscriptions").first<{ key_cipher: string | null; mailed_at: number | null; shown_at: number | null }>();
    expect(row).toMatchObject({ key_cipher: null, mailed_at: now, shown_at: now });
  });

  it("forgets a mailed key after a week even if the page was never opened", async () => {
    await tell("customer.subscription.created", { id: "sub_SampleA1" });
    const sealed = () => env.DB.prepare("SELECT key_cipher FROM subscriptions").first<{ key_cipher: string | null }>();
    expect((await sealed())?.key_cipher).not.toBeNull();
    now += 7 * DAY + 1;
    await tell("customer.subscription.updated", { id: "sub_SampleA1" });
    expect((await sealed())?.key_cipher).toBeNull();
    expect(((await (await call("/v1/shop/order", { sessionId: "cs_test_SampleDone01" })).json()) as { key: string | null }).key).toBeNull();
  });

  it("waits while the payment is not complete, and knows no made-up orders", async () => {
    sessions.cs_test_SampleOpen01 = { status: "open", payment_status: "unpaid", subscription: null, email: "" };
    expect((await call("/v1/shop/order", { sessionId: "cs_test_SampleOpen01" })).status).toBe(202);
    expect((await call("/v1/shop/order", { sessionId: "cs_test_SampleNone01" })).status).toBe(404);
    expect((await call("/v1/shop/order", { sessionId: "../../v1/customers" })).status).toBe(400);
    expect(await licences()).toHaveLength(0);
  });

  it("is slowed down when asked too often", async () => {
    for (let i = 0; i < 30; i++) await call("/v1/shop/order", { sessionId: "cs_test_SampleNone01" });
    expect((await call("/v1/shop/order", { sessionId: "cs_test_SampleDone01" })).status).toBe(429);
  });
});

describe("starting a purchase", () => {
  it("opens Stripe's page for the chosen plan; yen gets the consumption tax rate, made at Stripe on first use", async () => {
    const r = await call("/v1/shop/checkout", { seats: 5, period: "month", currency: "jpy", lang: "ja" });
    expect(r.status).toBe(200);
    expect(((await r.json()) as { url: string }).url).toBe("https://checkout.stripe.example.invalid/pay/cs_test_SampleNew0001");
    const made = stripeCalls.find((c) => c.method === "POST" && c.url.endsWith("/v1/tax_rates"));
    expect(Object.fromEntries(new URLSearchParams(made?.body))).toEqual({
      display_name: "消費税",
      description: "Japanese consumption tax, added to payments in yen",
      percentage: "10",
      inclusive: "false",
      country: "JP",
      jurisdiction: "JP",
      tax_type: "jct",
      "metadata[backhaul]": "jct10",
    });
    expect(made?.headers.get("idempotency-key")).toBe("backhaul-jct10");
    const sent = new URLSearchParams(at(stripeCalls, stripeCalls.length - 1).body);
    expect(Object.fromEntries(sent)).toEqual({
      mode: "subscription",
      "line_items[0][price]": "price_5m",
      "line_items[0][quantity]": "1",
      currency: "jpy",
      locale: "ja",
      success_url: `${SITE}/ja/thanks/?session_id={CHECKOUT_SESSION_ID}`,
      cancel_url: `${SITE}/ja/#pricing`,
      billing_address_collection: "required",
      "managed_payments[enabled]": "false",
      "subscription_data[metadata][lang]": "ja",
      "subscription_data[default_tax_rates][0]": "txr_Sample2",
    });
  });

  it("uses the same tax rate from then on, and never one it did not mark", async () => {
    await call("/v1/shop/checkout", { seats: 5, period: "month", currency: "jpy" });
    await call("/v1/shop/checkout", { seats: 5, period: "month", currency: "jpy" });
    expect(taxRates).toHaveLength(2); // the stranger's 8 % rate and ours
    expect(stripeCalls.filter((c) => c.method === "POST" && c.url.endsWith("/v1/tax_rates"))).toHaveLength(1);
    expect(new URLSearchParams(at(stripeCalls, stripeCalls.length - 1).body).get("subscription_data[default_tax_rates][0]")).toBe("txr_Sample2");
  });

  it("adds no Japanese tax to a payment in dollars", async () => {
    await call("/v1/shop/checkout", { seats: 10, period: "year", currency: "usd", lang: "en" });
    const sent = new URLSearchParams(at(stripeCalls, 1).body);
    expect(sent.get("line_items[0][price]")).toBe("price_10y");
    expect(sent.get("currency")).toBe("usd");
    expect(sent.get("success_url")).toBe(`${SITE}/thanks/?session_id={CHECKOUT_SESSION_ID}`);
    expect(sent.has("subscription_data[default_tax_rates][0]")).toBe(false);
    expect(stripeCalls.some((c) => c.url.includes("/v1/tax_rates"))).toBe(false);
  });

  it("refuses plans that are not sold", async () => {
    for (const body of [{ seats: 7, period: "month", currency: "jpy" }, { seats: 5, period: "week", currency: "jpy" }, { seats: 5, period: "month", currency: "eur" }, {}]) {
      expect((await call("/v1/shop/checkout", body)).status).toBe(400);
    }
    expect(stripeCalls).toHaveLength(0);
  });

  it("says so when the plan has no price at Stripe, or Stripe fails", async () => {
    expect((await call("/v1/shop/checkout", { seats: 20, period: "month", currency: "jpy" })).status).toBe(503);
    prices = {};
    env.STRIPE_SECRET_KEY = undefined;
    expect((await call("/v1/shop/checkout", { seats: 5, period: "month", currency: "jpy" })).status).toBe(503);
  });

  it("lets only the sales site call from a browser", async () => {
    const own = await call("/v1/shop/checkout", { seats: 5, period: "month", currency: "jpy" }, { origin: SITE });
    expect(own.headers.get("access-control-allow-origin")).toBe(SITE);
    const other = await call("/v1/shop/checkout", { seats: 5, period: "month", currency: "jpy" }, { origin: "https://elsewhere.example.invalid" });
    expect(other.headers.get("access-control-allow-origin")).toBeNull();
    const ask = await call("/v1/shop/trial", undefined, { origin: SITE, method: "OPTIONS" });
    expect(ask.status).toBe(204);
    expect(ask.headers.get("access-control-allow-origin")).toBe(SITE);
  });
});

describe("the free trial", () => {
  it("mails a key for 3 PCs and 14 days, and shows no key in the answer", async () => {
    const r = await call("/v1/shop/trial", { email: "Crew@Otter.example", lang: "en" });
    expect(r.status).toBe(200);
    const answer = await r.text();
    expect(answer).not.toContain("BH-");
    expect(JSON.parse(answer)).toEqual({ ok: true, seats: 3, days: 14 });
    expect(at(mails, 0)).toMatchObject({ to: ["Crew@Otter.example"], subject: "Your backhaul trial key" });
    expect(at(mails, 0).text).toContain("Trial ends: 2027-01-29");
    expect(at(mails, 0).text).not.toContain("billing.example.invalid");
    const list = await licences();
    expect(at(list, 0)).toMatchObject({ seats: 3, expiresAt: now + 14 * DAY, note: "Trial" });
    expect((await activate(keyIn(at(mails, 0).text), "sample-device-aaaaaaaa", "SAMPLE-PC-01")).status).toBe(200);
  });

  it("is given once per mailbox, however the address is written", async () => {
    expect((await call("/v1/shop/trial", { email: "crew@otter.example" })).status).toBe(200);
    expect((await call("/v1/shop/trial", { email: "CREW@otter.example" })).status).toBe(409);
    expect((await call("/v1/shop/trial", { email: "crew+two@otter.example" })).status).toBe(409);
    expect((await call("/v1/shop/trial", { email: "other@otter.example" })).status).toBe(200);
    expect(await licences()).toHaveLength(2);
  });

  it("stores only a hash of the address", async () => {
    await call("/v1/shop/trial", { email: "crew@otter.example" });
    const row = await env.DB.prepare("SELECT email_hash FROM trials").first<{ email_hash: string }>();
    expect(row?.email_hash).toMatch(/^[0-9a-f]{64}$/);
  });

  it("can be asked for again when the mail could not be sent", async () => {
    mailWorks = false;
    expect((await call("/v1/shop/trial", { email: "crew@otter.example" })).status).toBe(502);
    expect(await licences()).toHaveLength(0);
    mailWorks = true;
    expect((await call("/v1/shop/trial", { email: "crew@otter.example" })).status).toBe(200);
    expect(await licences()).toHaveLength(1);
  });

  it("refuses what is not an address, and floods from one caller", async () => {
    for (const email of ["", "otter", "a@b", "a b@otter.example", "a@otter.example, b@otter.example", 7]) {
      expect((await call("/v1/shop/trial", { email })).status).toBe(400);
    }
    for (let i = 0; i < 5; i++) expect((await call("/v1/shop/trial", { email: `crew${i}@otter.example` })).status).toBe(200);
    expect((await call("/v1/shop/trial", { email: "crew9@otter.example" })).status).toBe(429);
    expect((await call("/v1/shop/trial", { email: "crew9@otter.example" }, { ip: "192.0.2.8" })).status).toBe(200);
  });
});

describe("when selling is not set up", () => {
  it("every shop request says so, and the rest of the service works as before", async () => {
    env.RESEND_API_KEY = undefined;
    for (const path of ["checkout", "order", "trial", "webhook"]) {
      expect((await call(`/v1/shop/${path}`, {})).status).toBe(503);
    }
    const home = (await (await call("/", undefined, { method: "GET" })).json()) as { shopConfigured: boolean };
    expect(home.shopConfigured).toBe(false);
    env.RESEND_API_KEY = "re_sample_otter";
    expect(((await (await call("/", undefined, { method: "GET" })).json()) as { shopConfigured: boolean }).shopConfigured).toBe(true);
  });
});

describe("the small rules", () => {
  it("the end date follows the subscription's state", () => {
    const base = { id: "sub_x", seats: 5, periodStart: 1000, periodEnd: 5000, email: null, lang: "en" as const };
    expect(expiryOf({ ...base, status: "active" }, 2000, null)).toBe(5000 + GRACE_SECONDS);
    expect(expiryOf({ ...base, status: "trialing" }, 2000, null)).toBe(5000 + GRACE_SECONDS);
    expect(expiryOf({ ...base, status: "past_due" }, 2000, 9999)).toBe(1000 + GRACE_SECONDS);
    expect(expiryOf({ ...base, status: "canceled" }, 2000, 9999)).toBe(2000);
    expect(expiryOf({ ...base, status: "unpaid" }, 2000, 1500)).toBe(1500);
  });

  it("a subscription is read from where Stripe keeps things now (periods on the item)", () => {
    const read = readSubscription(subJson("sub_SampleA1", paid({ seats: "10", quantity: 2, lang: "ja" })) as unknown as Record<string, unknown>);
    expect(read).toEqual({ id: "sub_SampleA1", status: "active", seats: 20, periodStart: now, periodEnd: now + 30 * DAY, email: "buyer@otter.example", lang: "ja" });
    expect(readSubscription({ id: "sub_SampleA1", status: "active", customer: "cus_x" }).seats).toBeNull();
  });

  it("addresses", () => {
    expect(looksLikeEmail("crew@otter.example")).toBe(true);
    expect(looksLikeEmail("crew@otter")).toBe(false);
    expect(looksLikeEmail(`${"a".repeat(250)}@otter.example`)).toBe(false);
    expect(trialIdentity(" Crew+Hall@Otter.Example ")).toBe("crew@otter.example");
    expect(trialIdentity("+crew@otter.example")).toBe("+crew@otter.example");
  });

  it("the mail names the PCs and where to start, in the buyer's language", () => {
    const m = keyMail("buyer@otter.example", { lang: "en", key: "BH-AAAAA-BBBBB-CCCCC-DDDDD", seats: 10, trialUntil: null, siteUrl: SITE, billingUrl: "", contact: "info@seventh-well.com" });
    expect(m.subject).toBe("Your backhaul licence key");
    expect(m.text).toContain("Licence key: BH-AAAAA-BBBBB-CCCCC-DDDDD");
    expect(m.text).toContain("PCs: up to 10");
    expect(m.text).toContain(`${SITE}/start/`);
    expect(m.text).not.toContain("Change plan");
  });
});
