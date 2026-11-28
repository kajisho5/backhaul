// The two e-mails the service sends (a bought licence key, a trial key), through Resend.
import { keyMailContent, keyMailText } from "./mail_content";
import type { KeyMailView } from "./mail_content";
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

export type KeyMail = KeyMailView;

export function keyMail(to: string, m: KeyMail): Mail {
  const content = keyMailContent(m);
  return { to, subject: content.subject, text: keyMailText(content), html: keyMailHtml(content, `${m.siteUrl}/mail-icon.png`) };
}
