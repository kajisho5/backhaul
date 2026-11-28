// What the key mails say (a bought key with the order, a trial key), in both languages.
// One description of the content; mail.ts turns it into the text form and mail_html.ts
// into the designed form, so the two cannot drift apart.

/** What was bought, as Stripe's invoice says it. Amounts are in the currency's smallest unit. */
export interface MailOrder {
  number: string | null;
  /** Seconds since 1970. */
  created: number;
  currency: string;
  subtotal: number;
  total: number;
  /** Stripe's page with the receipt (the invoice); null when Stripe gave none. */
  receiptUrl: string | null;
  card: { brand: string; last4: string } | null;
}

export interface KeyMailView {
  lang: "en" | "ja";
  key: string;
  seats: number;
  /** Trial only: the day the key stops working, as "2026-12-12". */
  trialUntil: string | null;
  /** Bought keys: the order, when Stripe told it. */
  order: MailOrder | null;
  interval: "month" | "year" | null;
  /** Bought keys: when the next payment is taken (seconds since 1970); 0 when unknown. */
  nextBillingAt: number;
  siteUrl: string;
  /** Where a subscriber changes plan, card or cancels; empty when there is none. */
  billingUrl: string;
  privacyUrl: string;
  contact: string;
  /** The seller's registration number for Japanese invoices; empty when there is none. */
  sellerTaxId: string;
}

export interface Row {
  label: string;
  value: string;
  strong: boolean;
}
export interface Link {
  label: string;
  url: string;
}
/** A paragraph that may end in a link: text before, the link, text after. */
export interface Passage {
  before: string;
  link: Link | null;
  after: string;
}
export interface MailContent {
  lang: "en" | "ja";
  /** Bought keys are a paper to keep (light); the trial key keeps the program's own dark look. */
  look: "dark" | "light";
  subject: string;
  title: string;
  lead: string;
  keyLabel: string;
  key: string;
  keyNote: string;
  tableTitle: string;
  rows: Row[];
  receipt: (Link & { note: string }) | null;
  stepsTitle: string;
  steps: string[];
  guide: Link;
  manageTitle: string;
  manage: Passage;
  contactTitle: string;
  contactText: string;
  whyTitle: string;
  whyText: string;
  seller: string;
  footerLinks: Link[];
}

const MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
/** Currencies whose amounts Stripe gives in whole units. Others are in hundredths. */
const WHOLE_UNIT = new Set(["jpy", "krw", "vnd", "clp", "pyg", "xaf", "xof", "xpf", "bif", "djf", "gnf", "kmf", "mga", "rwf", "ugx", "vuv"]);
const SIGNS: Record<string, string> = { jpy: "¥", usd: "$", eur: "€", gbp: "£" };
const CARDS: Record<string, string> = { visa: "Visa", mastercard: "Mastercard", amex: "American Express", jcb: "JCB", diners: "Diners Club", discover: "Discover", unionpay: "UnionPay" };

function grouped(whole: number): string {
  return String(Math.trunc(Math.abs(whole))).replace(/\B(?=(\d{3})+(?!\d))/g, ",");
}

/** 10780 yen as "¥10,780", 6900 US cents as "$69.00"; an unknown currency keeps its code. */
export function moneyText(amount: number, currency: string): string {
  const code = currency.toLowerCase();
  const minus = amount < 0 ? "-" : "";
  const sign = SIGNS[code];
  const number = WHOLE_UNIT.has(code)
    ? grouped(amount)
    : `${grouped(Math.abs(amount) / 100)}.${String(Math.abs(amount) % 100).padStart(2, "0")}`;
  return sign === undefined ? `${minus}${number} ${code.toUpperCase()}` : `${minus}${sign}${number}`;
}

function written(year: number, month: number, date: number, lang: "en" | "ja"): string {
  return lang === "ja" ? `${year}年${month}月${date}日` : `${MONTHS[month - 1] ?? month} ${date}, ${year}`;
}

/** "2026-12-12" as people write a day. Anything else is shown as it came. */
export function dayText(day: string, lang: "en" | "ja"): string {
  const parts = /^(\d{4})-(\d{2})-(\d{2})$/.exec(day);
  if (parts === null) return day;
  const month = Number(parts[2]);
  const date = Number(parts[3]);
  if (month < 1 || month > 12 || date < 1 || date > 31) return day;
  return written(Number(parts[1]), month, date, lang);
}

/** A moment as a day: in Japan's time for Japanese mails, in UTC for English ones. */
export function dateText(seconds: number, lang: "en" | "ja"): string {
  const d = new Date((seconds + (lang === "ja" ? 9 * 3600 : 0)) * 1000);
  return written(d.getUTCFullYear(), d.getUTCMonth() + 1, d.getUTCDate(), lang);
}

function cardText(card: { brand: string; last4: string }, lang: "en" | "ja"): string {
  const brand = CARDS[card.brand.toLowerCase()] ?? card.brand.slice(0, 1).toUpperCase() + card.brand.slice(1);
  return lang === "ja" ? `${brand}（末尾 ${card.last4}）` : `${brand} ending in ${card.last4}`;
}

function orderRows(m: KeyMailView): Row[] {
  const ja = m.lang === "ja";
  const rows: Row[] = [];
  const add = (label: string, value: string, strong = false) => rows.push({ label, value, strong });
  const o = m.order;
  if (o !== null && o.number !== null) add(ja ? "注文番号" : "Order number", o.number);
  if (o !== null) add(ja ? "ご注文日" : "Order date", dateText(o.created, m.lang));
  const period = m.interval === "year" ? (ja ? "年額" : "yearly") : m.interval === "month" ? (ja ? "月額" : "monthly") : "";
  add(ja ? "プラン" : "Plan", ja ? `backhaul ${m.seats}台${period ? `・${period}` : ""}` : `backhaul, ${m.seats} PCs${period ? `, ${period}` : ""}`);
  if (o !== null) {
    const tax = o.total - o.subtotal;
    if (tax > 0) {
      add(ja ? "小計" : "Subtotal", moneyText(o.subtotal, o.currency));
      // Only yen payments carry tax here: Japanese consumption tax, 10 %.
      add(ja ? "消費税（10%）" : "Consumption tax (10%)", moneyText(tax, o.currency));
    }
    add(ja ? "お支払い金額" : "Amount paid", moneyText(o.total, o.currency), true);
    if (o.card !== null) add(ja ? "お支払い方法" : "Paid with", cardText(o.card, m.lang));
  }
  if (m.nextBillingAt > 0) add(ja ? "次回のご請求日" : "Next payment", dateText(m.nextBillingAt, m.lang));
  return rows;
}

export function keyMailContent(m: KeyMailView): MailContent {
  const ja = m.lang === "ja";
  const trial = m.trialUntil !== null;
  const home = `${m.siteUrl}${ja ? "/ja" : ""}`;
  const footerLinks: Link[] = [{ label: m.siteUrl.replace(/^https?:\/\//, ""), url: `${home}/` }];
  if (ja) footerLinks.push({ label: "特定商取引法に基づく表記", url: `${m.siteUrl}/ja/legal/` });
  footerLinks.push({ label: ja ? "プライバシーポリシー" : "Privacy", url: m.privacyUrl });
  const receiptUrl = m.order?.receiptUrl ?? null;
  const common = {
    lang: m.lang,
    key: m.key,
    keyLabel: ja ? "ライセンスキー" : "Licence key",
    keyNote: ja ? "同じキーを、お使いになる各PCに入力してください。" : "Enter the same key on each PC you use.",
    stepsTitle: ja ? "ご利用の手順" : "Getting started",
    steps: ja
      ? ["backhaul を会場のPCにインストールします。", "設定の「Zoom接続」にある「ライセンスキー」に、上のキーを入力します。", "Zoom にサインインし、会議に参加します。"]
      : ["Install backhaul on the PC at the venue.", "Enter the key above under Settings > Zoom connection > Licence key.", "Sign in to Zoom and join the meeting."],
    guide: { label: ja ? "はじめかたを見る" : "Read the guide", url: `${home}/start/` },
    contactTitle: ja ? "お問い合わせ" : "Questions",
    contactText: ja ? `ご不明な点は、このメールにご返信いただくか、${m.contact} までご連絡ください。` : `Reply to this mail, or write to ${m.contact}.`,
    whyTitle: ja ? "このメールが届いた理由" : "Why you received this mail",
    footerLinks,
  };
  if (trial) {
    return {
      ...common,
      look: "dark",
      subject: ja ? "backhaul 無料体験のライセンスキー" : "Your backhaul trial key",
      title: ja ? "無料体験のキーをお送りします" : "Your trial licence key",
      lead: ja
        ? "backhaul の無料体験にお申し込みいただき、ありがとうございます。下記のキーで、すべての機能を14日間お試しいただけます。"
        : "Thank you for trying backhaul. With the key below, every feature is yours for 14 days.",
      tableTitle: ja ? "ご利用の内容" : "Your trial",
      rows: [
        { label: ja ? "プラン" : "Plan", value: ja ? "無料体験" : "Free trial", strong: false },
        { label: ja ? "使えるPC" : "PCs", value: ja ? `${m.seats}台まで` : `Up to ${m.seats}`, strong: false },
        { label: ja ? "ご利用期限" : "Ends", value: ja ? `${dayText(m.trialUntil ?? "", "ja")}まで` : dayText(m.trialUntil ?? "", "en"), strong: true },
        { label: ja ? "料金" : "Price", value: ja ? "無料（カードの登録は不要です）" : "Free (no card needed)", strong: false },
      ],
      receipt: null,
      manageTitle: ja ? "体験のあとについて" : "After the trial",
      manage: ja
        ? { before: "期限を過ぎるとキーは使えなくなります。自動で課金されることはありません。続けてお使いになる場合は、", link: { label: "料金のページ", url: `${home}/#pricing` }, after: "からお申し込みください。" }
        : { before: "The key stops working after that day, and nothing is charged. To keep using backhaul, choose a plan on the ", link: { label: "pricing page", url: `${home}/#pricing` }, after: "." },
      whyText: ja
        ? "このメールアドレスで backhaul の無料体験にお申し込みいただいたため、このメールをお送りしています。お心当たりがない場合は、このメールを破棄してください。"
        : "Someone asked for a backhaul trial with this address. If that was not you, you can ignore this mail.",
      seller: "SEVENTHWELL",
    };
  }
  const billing = m.billingUrl.length > 0;
  return {
    ...common,
    look: "light",
    subject: ja ? "backhaul のライセンスキー" : "Your backhaul licence key",
    title: ja ? "ご購入ありがとうございます" : "Thank you for your purchase",
    lead: ja
      ? "backhaul のご契約を確認いたしました。ライセンスキーとご注文の内容をお知らせします。"
      : "Your backhaul subscription is confirmed. Here are your licence key and the details of your order.",
    tableTitle: ja ? "ご注文の内容" : "Your order",
    rows: orderRows(m),
    receipt:
      receiptUrl === null
        ? null
        : {
            label: ja ? "領収書を表示" : "View receipt",
            url: receiptUrl,
            note: ja ? "領収書（請求書）は、決済会社 Stripe のページで表示・保存できます。" : "The receipt (invoice) opens on a page of Stripe, our payment provider, where you can save it.",
          },
    manageTitle: ja ? "ご契約の変更・解約" : "Change or cancel",
    manage: ja
      ? billing
        ? { before: "プランの変更、カードの変更、解約、過去の領収書の確認は、", link: { label: "ご契約者ページ", url: m.billingUrl }, after: "から行えます。" }
        : { before: `プランの変更、カードの変更、解約は、${m.contact} までご連絡ください。`, link: null, after: "" }
      : billing
        ? { before: "Change your plan or card, cancel, or find past receipts on the ", link: { label: "subscriber page", url: m.billingUrl }, after: "." }
        : { before: `To change your plan or card, or to cancel, write to ${m.contact}.`, link: null, after: "" },
    whyText: ja
      ? "お客様のメールアドレスで backhaul をご購入いただいたため、このメールをお送りしています。"
      : "You are receiving this mail because backhaul was bought with this address.",
    seller: ja && m.sellerTaxId.length > 0 ? `SEVENTHWELL　登録番号 ${m.sellerTaxId}` : "SEVENTHWELL",
  };
}

/** The mail for programs that show no design: the same content, line by line. */
export function keyMailText(c: MailContent): string {
  const lines = [c.title, "", c.lead, "", `${c.keyLabel}${c.lang === "ja" ? "： " : ": "}${c.key}`, c.keyNote, "", `■ ${c.tableTitle}`];
  for (const row of c.rows) lines.push(`${row.label}${c.lang === "ja" ? "： " : ": "}${row.value}`);
  if (c.receipt !== null) lines.push("", c.receipt.label, c.receipt.url, c.receipt.note);
  lines.push("", `■ ${c.stepsTitle}`);
  c.steps.forEach((step, i) => lines.push(`${i + 1}. ${step}`));
  lines.push(c.guide.url, "", `■ ${c.manageTitle}`, `${c.manage.before}${c.manage.link?.label ?? ""}${c.manage.after}`);
  if (c.manage.link !== null) lines.push(c.manage.link.url);
  lines.push("", `■ ${c.contactTitle}`, c.contactText, "", `■ ${c.whyTitle}`, c.whyText, "", "--", c.seller);
  for (const link of c.footerLinks) lines.push(`${link.label}  ${link.url}`);
  return lines.join("\n");
}
