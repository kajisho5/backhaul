// The two e-mails the service sends (a bought licence key, a trial key), through Resend.
import { keyMailHtml } from "./mail_html";
import type { Fetcher } from "./zoom";

export class MailError extends Error {}

export interface Mail {
  to: string;
  subject: string;
  text: string;
  /** The same message with its design, for mail programs that show one. */
  html: string;
}

/**
 * Sends one e-mail. `once` names this message: Resend sends a message with the same name
 * only once within 24 hours, so a repeated request cannot deliver the same key twice.
 */
export async function sendMail(fetcher: Fetcher, apiKey: string, from: string, replyTo: string, mail: Mail, once: string) {
  const answer = await fetcher("https://api.resend.com/emails", {
    method: "POST",
    headers: { authorization: `Bearer ${apiKey}`, "content-type": "application/json", "idempotency-key": once },
    body: JSON.stringify({ from, to: [mail.to], subject: mail.subject, text: mail.text, html: mail.html, reply_to: replyTo }),
  });
  if (!answer.ok) throw new MailError(`the mail service answered ${answer.status}`);
}

/** Roughly an e-mail address: one @, a dot in the domain, no spaces. The mail itself is the real check. */
export function looksLikeEmail(text: string): boolean {
  return text.length <= 254 && /^[^\s@<>"',;]+@[^\s@<>"',;]+\.[^\s@<>"',;.]{2,}$/.test(text);
}

/**
 * The form under which an address counts once for the trial: lower case, without a
 * "+tag" in front of the @ (name+a@… and name+b@… are the same mailbox).
 */
export function trialIdentity(email: string): string {
  const lower = email.trim().toLowerCase();
  const at = lower.lastIndexOf("@");
  const plus = lower.indexOf("+");
  return plus > 0 && plus < at ? lower.slice(0, plus) + lower.slice(at) : lower;
}

export interface KeyMail {
  lang: "en" | "ja";
  key: string;
  seats: number;
  /** Trial only: the day the key stops working, as "2026-12-12". */
  trialUntil: string | null;
  siteUrl: string;
  /** Where a subscriber changes plan, card or cancels; empty when there is none. */
  billingUrl: string;
  contact: string;
}

export function keyMail(to: string, m: KeyMail): Mail {
  const guide = `${m.siteUrl}${m.lang === "ja" ? "/ja" : ""}/start/`;
  const html = keyMailHtml({ lang: m.lang, key: m.key, seats: m.seats, trialUntil: m.trialUntil, guideUrl: guide, iconUrl: `${m.siteUrl}/mail-icon.png`, billingUrl: m.billingUrl });
  if (m.lang === "ja") {
    const lines = [
      m.trialUntil === null ? "backhaul をご契約いただき、ありがとうございます。" : "backhaul の無料体験にお申し込みいただき、ありがとうございます。",
      "",
      `ライセンスキー： ${m.key}`,
      `使えるPC： ${m.seats}台まで（同じキーを各PCに入れます）`,
    ];
    if (m.trialUntil !== null) lines.push(`体験の期限： ${m.trialUntil} まで`);
    lines.push("", "使い始め方", `${guide}`, "backhaul を起動し、設定の「Zoom接続」にある「ライセンスキー」にこのキーを入れてください。");
    if (m.trialUntil === null && m.billingUrl.length > 0) {
      lines.push("", "プランの変更・カードの変更・解約・領収書", m.billingUrl);
    }
    lines.push("", `ご不明な点は ${m.contact} へご連絡ください。`, "", "SEVENTHWELL");
    return {
      to,
      subject: m.trialUntil === null ? "backhaul のライセンスキー" : "backhaul 無料体験のライセンスキー",
      text: lines.join("\n"),
    html,
    };
  }
  const lines = [
    m.trialUntil === null ? "Thank you for subscribing to backhaul." : "Thank you for trying backhaul.",
    "",
    `Licence key: ${m.key}`,
    `PCs: up to ${m.seats} (enter the same key on each PC)`,
  ];
  if (m.trialUntil !== null) lines.push(`Trial ends: ${m.trialUntil}`);
  lines.push("", "Getting started", guide, "Start backhaul and enter this key under Settings > Zoom connection > Licence key.");
  if (m.trialUntil === null && m.billingUrl.length > 0) {
    lines.push("", "Change plan or card, cancel, receipts", m.billingUrl);
  }
  lines.push("", `Questions: ${m.contact}`, "", "SEVENTHWELL");
  return {
    to,
    subject: m.trialUntil === null ? "Your backhaul licence key" : "Your backhaul trial key",
    text: lines.join("\n"),
    html,
  };
}
