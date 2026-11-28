// The designed form of the key mails (the content itself is in mail_content.ts).
// Mail programs understand little: tables, colours written on each element, no scripts,
// no fonts of our own. Every value is escaped here, whatever its origin.
import type { Link, MailContent } from "./mail_content";

const FONT = "-apple-system,BlinkMacSystemFont,'Segoe UI','Hiragino Sans','Noto Sans JP',Meiryo,sans-serif";
const MONO = "ui-monospace,'SF Mono',Menlo,Consolas,monospace";

interface Look {
  scheme: string;
  bg: string;
  ink: string;
  mute: string;
  faint: string;
  line: string;
  panel: string;
  panelLine: string;
  /** A soft light on the key's panel; empty where the look has none. */
  glow: string;
  accent: string;
  keyInk: string;
  button: string;
  buttonInk: string;
}

const LOOKS: Record<"dark" | "light", Look> = {
  dark: {
    scheme: "dark",
    bg: "#050a08",
    ink: "#eef3ef",
    mute: "#9db0a6",
    faint: "#6f8278",
    line: "#1e2c26",
    panel: "#0b1512",
    panelLine: "#24382f",
    glow: "background-image:radial-gradient(120% 140% at 0% 0%,rgba(93,240,160,.22) 0%,rgba(15,182,162,.08) 42%,rgba(11,21,18,0) 70%);",
    accent: "#5df0a0",
    keyInk: "#ffffff",
    button: "#5df0a0",
    buttonInk: "#04130c",
  },
  light: {
    scheme: "light",
    bg: "#ffffff",
    ink: "#0a0f0d",
    mute: "#3d4a44",
    faint: "#6b7a73",
    line: "#e1e7e3",
    panel: "#f4f6f4",
    panelLine: "#d5ddd8",
    glow: "",
    accent: "#0a7d5a",
    keyInk: "#0a0f0d",
    button: "#050a08",
    buttonInk: "#ffffff",
  },
};

export function escapeHtml(text: string): string {
  return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#39;");
}

export function keyMailHtml(c: MailContent, iconUrl: string): string {
  const t = LOOKS[c.look];
  const e = escapeHtml;
  const link = (l: Link) => `<a href="${e(l.url)}" style="color:${t.accent};text-decoration:underline">${e(l.label)}</a>`;
  const heading = (text: string) => `<div style="font-size:15px;font-weight:700;color:${t.ink};padding:28px 0 8px">${e(text)}</div>`;
  const para = (inner: string) => `<div style="font-size:14px;line-height:1.85;color:${t.mute}">${inner}</div>`;
  const rows = c.rows
    .map((row, i) => {
      const under = i === c.rows.length - 1 ? "" : `border-bottom:1px solid ${t.line};`;
      return (
        `<tr><td style="${under}padding:11px 12px 11px 0;font-size:13px;color:${t.mute};vertical-align:top;width:38%">${e(row.label)}</td>` +
        `<td align="right" style="${under}padding:11px 0;font-size:14px;font-weight:${row.strong ? "700" : "400"};color:${t.ink};vertical-align:top">${e(row.value)}</td></tr>`
      );
    })
    .join("");
  const receipt =
    c.receipt === null
      ? ""
      : `<div style="padding-top:18px"><a href="${e(c.receipt.url)}" style="display:inline-block;background:${t.button};color:${t.buttonInk};text-decoration:none;font-size:15px;font-weight:700;padding:13px 26px;border-radius:999px">${e(c.receipt.label)}</a></div>` +
        `<div style="padding-top:10px;font-size:12px;line-height:1.8;color:${t.faint}">${e(c.receipt.note)}</div>`;
  const steps = c.steps.map((step, i) => `${i + 1}. ${e(step)}`).join("<br>");
  const manage = e(c.manage.before) + (c.manage.link === null ? "" : link(c.manage.link)) + e(c.manage.after);
  const footer = c.footerLinks.map(link).join(`<span style="color:${t.line}">　|　</span>`);
  return (
    `<!doctype html><html lang="${c.lang}"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">` +
    // Saying which look this is keeps mail programs from recolouring it.
    `<meta name="color-scheme" content="${t.scheme}"><meta name="supported-color-schemes" content="${t.scheme}"></head>` +
    `<body style="margin:0;background:${t.bg};font-family:${FONT};-webkit-text-size-adjust:100%">` +
    `<table role="presentation" width="100%" cellpadding="0" cellspacing="0" bgcolor="${t.bg}" style="background:${t.bg}"><tr><td align="center" style="padding:30px 20px 36px">` +
    `<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="max-width:520px">` +
    `<tr><td style="padding:0 0 28px"><table role="presentation" cellpadding="0" cellspacing="0"><tr>` +
    `<td><img src="${e(iconUrl)}" width="36" height="36" alt="" style="display:block;border-radius:9px"></td>` +
    `<td style="padding-left:10px;font-size:17px;font-weight:700;color:${t.ink}">backhaul</td></tr></table></td></tr>` +
    `<tr><td><div style="font-size:24px;line-height:1.4;font-weight:800;letter-spacing:-.01em;color:${t.ink}">${e(c.title)}</div>` +
    `<div style="padding-top:12px">${para(e(c.lead))}</div></td></tr>` +
    `<tr><td style="padding-top:22px">` +
    `<table role="presentation" width="100%" cellpadding="0" cellspacing="0" bgcolor="${t.panel}" style="background:${t.panel};${t.glow}border:1px solid ${t.panelLine};border-radius:16px"><tr><td style="padding:20px 18px 22px">` +
    `<div style="font-size:12px;font-weight:600;color:${t.accent}">${e(c.keyLabel)}</div>` +
    `<div style="margin-top:8px;font-family:${MONO};font-size:18px;line-height:1.5;font-weight:700;color:${t.keyInk};word-break:break-all">${e(c.key)}</div>` +
    `<div style="margin-top:6px;font-size:12px;color:${t.mute}">${e(c.keyNote)}</div></td></tr></table></td></tr>` +
    `<tr><td>${heading(c.tableTitle)}<table role="presentation" width="100%" cellpadding="0" cellspacing="0" style="border-top:1px solid ${t.line};border-bottom:1px solid ${t.line}">${rows}</table>${receipt}</td></tr>` +
    `<tr><td>${heading(c.stepsTitle)}<div style="font-size:14px;line-height:2;color:${t.mute}">${steps}</div><div style="padding-top:10px;font-size:14px">${link(c.guide)}</div></td></tr>` +
    `<tr><td>${heading(c.manageTitle)}${para(manage)}</td></tr>` +
    `<tr><td>${heading(c.contactTitle)}${para(e(c.contactText))}</td></tr>` +
    `<tr><td>${heading(c.whyTitle)}${para(e(c.whyText))}</td></tr>` +
    `<tr><td style="padding-top:30px"><div style="border-top:1px solid ${t.line};padding-top:18px;font-size:12px;line-height:1.9;color:${t.faint}">${e(c.seller)}<br>${footer}</div></td></tr>` +
    `</table></td></tr></table></body></html>`
  );
}
