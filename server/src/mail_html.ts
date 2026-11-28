// The picture form of the key mail (the text form in mail.ts carries the same facts).
// Mail programs understand little: tables, colours written on each element, no scripts,
// no fonts of our own. Every value that comes from outside is escaped.

export interface KeyMailView {
  lang: "en" | "ja";
  key: string;
  seats: number;
  /** Trial only: the day the key stops working, as "2026-12-12". */
  trialUntil: string | null;
  /** The page that explains how to start. */
  guideUrl: string;
  /** The small picture of the program's icon. */
  iconUrl: string;
  /** Where a subscriber changes plan, card or cancels; empty when there is none. */
  billingUrl: string;
}

const FONT = "-apple-system,BlinkMacSystemFont,'Segoe UI','Hiragino Sans','Noto Sans JP',Meiryo,sans-serif";
const MONO = "ui-monospace,'SF Mono',Menlo,Consolas,monospace";
const MONTHS = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];

export function escapeHtml(text: string): string {
  return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#39;");
}

/** "2026-12-12" as people write a day: "12月12日" or "Dec 12". Anything else is shown as it came. */
export function dayText(day: string, lang: "en" | "ja"): string {
  const parts = /^(\d{4})-(\d{2})-(\d{2})$/.exec(day);
  if (parts === null) return day;
  const month = Number(parts[2]);
  const date = Number(parts[3]);
  const name = MONTHS[month - 1];
  if (name === undefined || date < 1 || date > 31) return day;
  return lang === "ja" ? `${month}月${date}日` : `${name} ${date}`;
}

interface Words {
  title: string[];
  lead: string;
  key: string;
  keyNote: string;
  seats: string;
  seatsValue: string;
  until: string;
  button: string;
  where: string;
  billing: string;
  help: string;
}

function words(m: KeyMailView): Words {
  const trial = m.trialUntil !== null;
  if (m.lang === "ja") {
    return {
      title: trial ? ["ようこそ。", "14日間、ぜんぶ使えます。"] : ["ありがとうございます。", "キーをお届けします。"],
      lead: "これがあなたのライセンスキーです。backhaul に入れると、Zoom の演者をそのまま会場へ出せます。",
      key: "ライセンスキー",
      keyNote: "同じキーを各PCに入れます",
      seats: "使えるPC",
      seatsValue: `${m.seats}台`,
      until: "期限",
      button: "使い始める",
      where: "設定 › Zoom接続 › ライセンスキー に入れるだけ",
      billing: "プランの変更・カードの変更・解約・領収書",
      help: "困ったときは、このメールにそのまま返信してください。",
    };
  }
  return {
    title: trial ? ["Welcome.", "14 days, nothing held back."] : ["Thank you.", "Here is your key."],
    lead: "This is your licence key. Enter it in backhaul and your Zoom presenters go straight to the venue.",
    key: "Licence key",
    keyNote: "Use the same key on each PC",
    seats: "PCs",
    seatsValue: `Up to ${m.seats}`,
    until: "Ends",
    button: "Get started",
    where: "Settings › Zoom connection › Licence key",
    billing: "Change plan or card, cancel, receipts",
    help: "Need a hand? Just reply to this mail.",
  };
}

function tile(label: string, value: string): string {
  return (
    `<table role="presentation" width="100%" cellpadding="0" cellspacing="0" bgcolor="#0b1512" style="background:#0b1512;border:1px solid #1e2c26;border-radius:16px"><tr><td style="padding:16px 18px">` +
    `<div style="font-size:12px;color:#9db0a6">${label}</div>` +
    `<div style="margin-top:4px;font-size:20px;font-weight:800;color:#eef3ef">${value}</div></td></tr></table>`
  );
}

export function keyMailHtml(m: KeyMailView): string {
  const w = words(m);
  const seats = tile(escapeHtml(w.seats), escapeHtml(w.seatsValue));
  const facts =
    m.trialUntil === null
      ? seats
      : `<table role="presentation" width="100%" cellpadding="0" cellspacing="0"><tr>` +
        `<td width="50%" style="padding-right:5px;vertical-align:top">${seats}</td>` +
        `<td width="50%" style="padding-left:5px;vertical-align:top">${tile(escapeHtml(w.until), escapeHtml(dayText(m.trialUntil, m.lang)))}</td></tr></table>`;
  const billing =
    m.trialUntil === null && m.billingUrl.length > 0
      ? `<tr><td style="padding:18px 4px 0;font-size:13px;line-height:1.8;text-align:center"><a href="${escapeHtml(m.billingUrl)}" style="color:#5df0a0;text-decoration:underline">${escapeHtml(w.billing)}</a></td></tr>`
      : "";
  return (
    `<!doctype html><html lang="${m.lang}"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">` +
    // The mail is dark by design: saying so keeps mail programs from recolouring it.
    `<meta name="color-scheme" content="dark"><meta name="supported-color-schemes" content="dark"></head>` +
    `<body style="margin:0;background:#050a08;font-family:${FONT};-webkit-text-size-adjust:100%">` +
    `<table role="presentation" width="100%" cellpadding="0" cellspacing="0" bgcolor="#050a08" style="background:#050a08"><tr><td align="center" style="padding:30px 16px 34px">` +
    `<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="max-width:520px">` +
    `<tr><td style="padding:0 4px 34px"><table role="presentation" cellpadding="0" cellspacing="0"><tr>` +
    `<td><img src="${escapeHtml(m.iconUrl)}" width="36" height="36" alt="" style="display:block;border-radius:9px"></td>` +
    `<td style="padding-left:10px;font-size:17px;font-weight:700;color:#eef3ef">backhaul</td></tr></table></td></tr>` +
    `<tr><td style="padding:0 4px">` +
    `<div style="font-size:27px;line-height:1.35;font-weight:800;letter-spacing:-.02em;color:#eef3ef">${w.title.map(escapeHtml).join("<br>")}</div>` +
    `<div style="margin-top:14px;font-size:15px;line-height:1.8;color:#9db0a6">${escapeHtml(w.lead)}</div></td></tr>` +
    `<tr><td style="padding:28px 0 0">` +
    `<table role="presentation" width="100%" cellpadding="0" cellspacing="0" bgcolor="#0b1512" style="background:#0b1512;background-image:radial-gradient(120% 140% at 0% 0%,rgba(93,240,160,.22) 0%,rgba(15,182,162,.08) 42%,rgba(11,21,18,0) 70%);border:1px solid #24382f;border-radius:20px"><tr><td style="padding:22px 18px 24px">` +
    `<div style="font-size:12px;font-weight:600;color:#5df0a0">${escapeHtml(w.key)}</div>` +
    `<div style="margin-top:10px;font-family:${MONO};font-size:18px;line-height:1.5;font-weight:700;color:#ffffff;word-break:break-all">${escapeHtml(m.key)}</div>` +
    `<div style="margin-top:6px;font-size:12px;color:#9db0a6">${escapeHtml(w.keyNote)}</div></td></tr></table></td></tr>` +
    `<tr><td style="padding:10px 0 0">${facts}</td></tr>` +
    `<tr><td style="padding:22px 0 0"><a href="${escapeHtml(m.guideUrl)}" style="display:block;background:#5df0a0;color:#04130c;text-decoration:none;text-align:center;font-size:16px;font-weight:700;padding:16px;border-radius:999px">${escapeHtml(w.button)}</a></td></tr>` +
    `<tr><td style="padding:14px 4px 0;font-size:13px;line-height:1.8;color:#9db0a6;text-align:center">${escapeHtml(w.where)}</td></tr>` +
    billing +
    `<tr><td style="padding:26px 4px 0;font-size:12px;line-height:1.8;color:#6f8278">${escapeHtml(w.help)}<br>SEVENTHWELL</td></tr>` +
    `</table></td></tr></table></body></html>`
  );
}
