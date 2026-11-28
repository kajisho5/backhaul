// Selling: Stripe takes the payment, this service turns a paid subscription into a licence
// and keeps the licence in step with it (PCs, end date). Also the free trial.
//
//   POST /v1/shop/checkout   the site asks for Stripe's payment page
//   POST /v1/shop/webhook    Stripe tells what happened (signed)
//   POST /v1/shop/order      the "thank you" page asks for the key (shown once)
//   POST /v1/shop/trial      a trial key, sent to the address given
//
// Whatever Stripe sends is only a hint to look: the subscription is always read again from
// Stripe, so late, repeated or out-of-order messages cannot leave a wrong state behind.
import { Hono } from "hono";
import type { Context } from "hono";
import { cors } from "hono/cors";
import type { Env, Outside } from "./index";
import { newLicenceKey, sha256Hex } from "./licence";
import { MailError, keyMail, looksLikeEmail, sendMail, trialIdentity } from "./mail";
import { Store } from "./store";
import type { Sale } from "./store";
import { Stripe, StripeError, verifyWebhook } from "./stripe";
import type { Subscription } from "./stripe";
import { Vault } from "./vault";

export const PLAN_SEATS = [5, 10, 20] as const;
export const TRIAL_SEATS = 3;
export const TRIAL_DAYS = 14;
/** After the paid time ends (a card that failed, a renewal that is late) the licence still works this long. */
export const GRACE_SECONDS = 3 * 86400;
const MAX_SEATS = 100;
const CONTACT = "info@seventh-well.com";

interface Shop {
  stripe: Stripe;
  vault: Vault;
  webhookSecret: string;
  mailKey: string;
  mailFrom: string;
  siteUrl: string;
  billingUrl: string;
}

/** Null until everything selling needs is set: the three secrets, the sealing key, the site and the sender. */
export async function openShop(env: Env, outside: Outside): Promise<Shop | null> {
  const vault = await Vault.open(env.SIGNIN_KEY);
  if (!env.STRIPE_SECRET_KEY || !env.STRIPE_WEBHOOK_SECRET || !env.RESEND_API_KEY || !env.SITE_URL || !env.MAIL_FROM || vault === null) {
    return null;
  }
  return {
    stripe: new Stripe(env.STRIPE_SECRET_KEY, outside.fetcher),
    vault,
    webhookSecret: env.STRIPE_WEBHOOK_SECRET,
    mailKey: env.RESEND_API_KEY,
    mailFrom: env.MAIL_FROM,
    siteUrl: env.SITE_URL.replace(/\/+$/, ""),
    billingUrl: env.BILLING_URL ?? "",
  };
}

function paying(status: string): boolean {
  return status === "active" || status === "trialing";
}

/** When the licence of this subscription stops working. */
export function expiryOf(sub: Subscription, now: number, current: number | null): number {
  if (paying(sub.status)) return sub.periodEnd + GRACE_SECONDS;
  // The renewal was not paid: the unpaid period has already begun, so the grace counts from its start.
  if (sub.status === "past_due") return sub.periodStart + GRACE_SECONDS;
  return Math.min(now, current ?? now); // cancelled, unpaid, paused, never completed
}

/**
 * Makes the licence of a paid subscription if there is none yet, and sets its PCs and end
 * date to what the subscription says. Null when there is nothing to have a licence for.
 */
async function settle(shop: Shop, store: Store, sub: Subscription, now: number): Promise<Sale | null> {
  // Without the paid period the end date would be a guess: better to fail loudly and let Stripe repeat the message.
  if (paying(sub.status) && sub.periodEnd <= 0) throw new Error(`Stripe did not say until when ${sub.id} is paid`);
  let sale = await store.subscriptionOf(sub.id);
  if (sale === null) {
    if (!paying(sub.status)) return null;
    if (sub.seats === null) throw new Error(`the price of ${sub.id} does not say how many PCs it is for (metadata "seats")`);
    const key = newLicenceKey();
    const licenceId = await store.createLicence(await sha256Hex(key), key.slice(-4), `Stripe ${sub.id}`, Math.min(sub.seats, MAX_SEATS), expiryOf(sub, now, null), now);
    if (!(await store.addSubscription(sub.id, licenceId, sub.status, await shop.vault.seal(key), now))) {
      await store.dropLicence(licenceId); // another request made the licence in the same moment
    }
    sale = await store.subscriptionOf(sub.id);
    if (sale === null) throw new Error("the subscription was not stored");
  }
  const licence = await store.licenceById(sale.licenceId);
  if (licence === null) throw new Error("the licence of the subscription is gone");
  if (paying(sub.status) && sub.seats === null) throw new Error(`the price of ${sub.id} does not say how many PCs it is for (metadata "seats")`);
  const seats = paying(sub.status) && sub.seats !== null ? Math.min(sub.seats, MAX_SEATS) : licence.seats;
  await store.followSubscription(sub.id, licence.id, sub.status, seats, expiryOf(sub, now, licence.expiresAt), now);
  return sale;
}

/** Sends the key by e-mail if that has not happened yet. Throws MailError when it could not be sent. */
async function deliver(shop: Shop, outside: Outside, store: Store, sale: Sale, sub: Subscription, now: number) {
  if (sale.keyCipher === null || !(await store.claimKeyMail(sub.id, now))) return;
  try {
    const key = await shop.vault.unseal(sale.keyCipher);
    const licence = await store.licenceById(sale.licenceId);
    if (key === null || licence === null) throw new MailError("the key can no longer be read");
    if (sub.email === null) throw new MailError("Stripe has no e-mail address for this customer");
    const mail = keyMail(sub.email, { lang: sub.lang, key, seats: licence.seats, trialUntil: null, siteUrl: shop.siteUrl, billingUrl: shop.billingUrl, contact: CONTACT });
    await sendMail(outside.fetcher, shop.mailKey, shop.mailFrom, CONTACT, mail, `key-${sub.id}`);
  } catch (error) {
    await store.releaseKeyMail(sub.id);
    throw error;
  }
}

async function readJson(c: Context): Promise<Record<string, unknown> | null> {
  const body: unknown = await c.req.json().catch(() => null);
  return body !== null && typeof body === "object" && !Array.isArray(body) ? (body as Record<string, unknown>) : null;
}

async function callerOf(c: Context): Promise<string> {
  return sha256Hex(c.req.header("cf-connecting-ip") ?? "unknown");
}

function maskEmail(email: string): string {
  const at = email.lastIndexOf("@");
  return at < 1 ? "" : `${email.slice(0, 1)}***${email.slice(at)}`;
}

export function shopRoutes(outside: Outside) {
  const shop = new Hono<{ Bindings: Env }>();

  // Stripe's messages come first: they carry no Origin and need no CORS.
  shop.post("/webhook", async (c) => {
    const open = await openShop(c.env, outside);
    if (open === null) return c.json({ error: "not_configured" }, 503);
    const body = await c.req.text();
    const now = outside.now();
    if (!(await verifyWebhook(body, c.req.header("stripe-signature") ?? "", open.webhookSecret, now))) {
      return c.json({ error: "bad_signature" }, 400);
    }
    let event: { type?: unknown; data?: { object?: { id?: unknown; subscription?: unknown } } };
    try {
      event = JSON.parse(body) as typeof event;
    } catch {
      return c.json({ error: "bad_request" }, 400);
    }
    const object = event.data?.object;
    const type = typeof event.type === "string" ? event.type : "";
    let subscriptionId: unknown = null;
    if (type === "checkout.session.completed") subscriptionId = object?.subscription;
    else if (type.startsWith("customer.subscription.")) subscriptionId = object?.id;
    if (typeof subscriptionId !== "string" || !/^sub_[A-Za-z0-9]{6,250}$/.test(subscriptionId)) return c.json({ ignored: true });

    const store = new Store(c.env.DB);
    const sub = await open.stripe.subscription(subscriptionId);
    const sale = await settle(open, store, sub, now);
    // A mail that cannot be sent makes this request fail, so that Stripe sends the message again later.
    if (sale !== null) await deliver(open, outside, store, sale, sub, now);
    await store.forgetDeliveredKeys(now);
    return c.json({ ok: true });
  });

  // The rest is called by the site's pages, from the browser.
  shop.use(
    "*",
    cors({
      origin: (origin, c) => (origin === ((c.env as Env).SITE_URL ?? "").replace(/\/+$/, "") ? origin : null),
      allowMethods: ["POST"],
      allowHeaders: ["content-type"],
      maxAge: 600,
    }),
  );

  shop.post("/checkout", async (c) => {
    const open = await openShop(c.env, outside);
    if (open === null) return c.json({ error: "not_configured" }, 503);
    const store = new Store(c.env.DB);
    if (!(await store.allow(`checkout:${await callerOf(c)}`, 20, 600, outside.now()))) return c.json({ error: "slow_down" }, 429);
    const body = await readJson(c);
    const seats = PLAN_SEATS.find((n) => n === body?.seats);
    const period = body?.period === "month" || body?.period === "year" ? body.period : null;
    const currency = body?.currency === "jpy" || body?.currency === "usd" ? body.currency : null;
    const lang = body?.lang === "ja" ? "ja" : "en";
    if (seats === undefined || period === null || currency === null) return c.json({ error: "bad_request" }, 400);
    try {
      const priceId = await open.stripe.priceId(`backhaul_${seats}_${period}`);
      if (priceId === null) return c.json({ error: "not_configured" }, 503);
      const home = `${open.siteUrl}${lang === "ja" ? "/ja" : ""}`;
      const url = await open.stripe.startCheckout({
        priceId,
        currency,
        lang,
        // Prices are without tax. Yen: Japanese consumption tax on top. Dollars: sold abroad, no Japanese tax.
        taxRate: currency === "jpy" ? await open.stripe.japanTaxRate() : null,
        // Stripe puts the session's id where the braces are; the page then asks /v1/shop/order with it.
        successUrl: `${home}/thanks/?session_id={CHECKOUT_SESSION_ID}`,
        cancelUrl: `${home}/#pricing`,
      });
      return c.json({ url });
    } catch (error) {
      if (!(error instanceof StripeError)) throw error;
      console.error(error);
      return c.json({ error: "stripe_failed" }, 502);
    }
  });

  shop.post("/order", async (c) => {
    const open = await openShop(c.env, outside);
    if (open === null) return c.json({ error: "not_configured" }, 503);
    const store = new Store(c.env.DB);
    const now = outside.now();
    if (!(await store.allow(`order:${await callerOf(c)}`, 30, 600, now))) return c.json({ error: "slow_down" }, 429);
    const body = await readJson(c);
    const sessionId = typeof body?.sessionId === "string" && /^cs_[A-Za-z0-9_]{10,250}$/.test(body.sessionId) ? body.sessionId : null;
    if (sessionId === null) return c.json({ error: "bad_request" }, 400);
    try {
      const session = await open.stripe.checkout(sessionId);
      if (session === null) return c.json({ error: "unknown_order" }, 404);
      if (!session.complete || session.subscriptionId === null) return c.json({ pending: true }, 202);
      const sub = await open.stripe.subscription(session.subscriptionId);
      const sale = await settle(open, store, sub, now);
      if (sale === null) return c.json({ pending: true }, 202);
      let mailed = true;
      try {
        await deliver(open, outside, store, sale, sub, now);
      } catch (error) {
        if (!(error instanceof MailError)) throw error;
        console.error(error);
        mailed = false; // the page still shows the key; Stripe's next message tries the mail again
      }
      const key = (await store.claimKeyShown(sub.id, now)) ? await open.vault.unseal(sale.keyCipher) : null;
      await store.forgetDeliveredKeys(now);
      const licence = await store.licenceById(sale.licenceId);
      if (licence === null) throw new Error("the licence of the subscription is gone");
      return c.json(
        { key, keyTail: licence.keyTail, seats: licence.seats, expiresAt: licence.expiresAt, mailed, email: maskEmail(sub.email ?? session.email ?? "") },
        200,
        { "cache-control": "no-store" },
      );
    } catch (error) {
      if (!(error instanceof StripeError)) throw error;
      console.error(error);
      return c.json({ error: "stripe_failed" }, 502);
    }
  });

  shop.post("/trial", async (c) => {
    const open = await openShop(c.env, outside);
    if (open === null) return c.json({ error: "not_configured" }, 503);
    const store = new Store(c.env.DB);
    const now = outside.now();
    const body = await readJson(c);
    const email = typeof body?.email === "string" ? body.email.trim() : "";
    const lang = body?.lang === "ja" ? "ja" : "en";
    if (!looksLikeEmail(email)) return c.json({ error: "bad_email" }, 400);
    // Few per caller, and a ceiling for everybody together: a flood of made-up addresses stops here.
    if (!(await store.allow(`trial:${await callerOf(c)}`, 5, 86400, now)) || !(await store.allow("trial:all", 300, 86400, now))) {
      return c.json({ error: "slow_down" }, 429);
    }
    const emailHash = await sha256Hex(`trial:${trialIdentity(email)}`);
    if (await store.trialExists(emailHash)) return c.json({ error: "already_used" }, 409);
    const key = newLicenceKey();
    const expiresAt = now + TRIAL_DAYS * 86400;
    const licenceId = await store.createLicence(await sha256Hex(key), key.slice(-4), "Trial", TRIAL_SEATS, expiresAt, now);
    if (!(await store.addTrial(emailHash, licenceId, now))) {
      await store.dropLicence(licenceId);
      return c.json({ error: "already_used" }, 409);
    }
    const mail = keyMail(email, {
      lang,
      key,
      seats: TRIAL_SEATS,
      trialUntil: new Date(expiresAt * 1000).toISOString().slice(0, 10),
      siteUrl: open.siteUrl,
      billingUrl: "",
      contact: CONTACT,
    });
    try {
      // The key goes out by mail only: that is the proof that the address belongs to the person asking.
      await sendMail(outside.fetcher, open.mailKey, open.mailFrom, CONTACT, mail, `trial-${licenceId}`);
    } catch (error) {
      if (!(error instanceof MailError)) throw error;
      console.error(error);
      await store.dropLicence(licenceId); // takes the trial entry with it: the address can try again
      return c.json({ error: "mail_failed" }, 502);
    }
    return c.json({ ok: true, seats: TRIAL_SEATS, days: TRIAL_DAYS });
  });

  return shop;
}
