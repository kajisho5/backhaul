// Public pages the Zoom listing links to: privacy, terms, support and the guide.
// Plain text in one layout; nothing here is secret and nothing is read from a request.

const UPDATED = "November 28, 2026";
const CONTACT = "info@seventh-well.com";

type Page = { path: string; title: string; lead: string; body: string };

const mail = `<a href="mailto:${CONTACT}">${CONTACT}</a>`;

const pages: Page[] = [
  {
    path: "/privacy",
    title: "Privacy Policy",
    lead: "What backhaul and its licence server keep, why, and how to have it removed.",
    body: `
<h2>Who we are</h2>
<p>backhaul is a Windows application made and sold by SEVENTHWELL (Tokyo, Japan). It brings the picture and sound of people in a Zoom meeting to the equipment of a live venue, and sends the venue's picture and sound back to the meeting. Contact: ${mail}.</p>

<h2>What stays on your PC</h2>
<p>Everything from a meeting stays on the PC that runs backhaul: video, audio, shared screens, participant names and the meeting passcode. None of it is sent to us or to anyone else, and backhaul does not record meetings. The licence key and the token that proves your PC is registered are kept in the Windows Credential Manager. Your settings are kept in a file on the PC.</p>

<h2>What our licence server keeps</h2>
<ul>
<li><b>Licence.</b> A one-way hash and the last four characters of your licence key, a note for our records (for example the buyer's name), the number of PCs allowed, the expiry date, and for each registered PC a random device ID, the PC's name, a hash of its registration token, and the times it was registered and last used.</li>
<li><b>Zoom sign-in (only if you sign in with Zoom in backhaul).</b> The OAuth tokens Zoom gives us for your account, encrypted with AES-256-GCM. We use them for one thing: to ask Zoom for a short-lived Zoom Access Key (ZAK) each time one of your PCs joins a meeting, so that Zoom knows which user the PC acts for. The ZAK is passed to your PC and kept on the server for at most 10 minutes. We do not read your profile, meetings, recordings, contacts or chat.</li>
<li><b>Subscription (only if you buy one).</b> Payment is handled by Stripe; we never see or store your card number. Our server keeps the ID of your Stripe subscription next to your licence, and reads from Stripe the e-mail address you gave there when it sends your licence key. Until the key has reached you (shown once after payment and sent by e-mail), an encrypted copy of it is kept, for at most a week after the e-mail.</li>
<li><b>Free trial (only if you ask for one).</b> The e-mail address you enter is used once, to send the trial key. We keep a one-way hash of it, so that each address gets one trial, and not the address itself.</li>
<li><b>Technical logs.</b> Our hosting provider records ordinary request logs (time, address, path) for operation and abuse prevention.</li>
</ul>
<p>We do not use analytics or advertising, we do not sell personal information, and we do not use your data to train AI models.</p>

<h2>Who processes it</h2>
<p>The licence server runs on Cloudflare (Workers and D1), which processes the data above on our behalf. Zoom Video Communications receives the requests your PC and our server make to join meetings and to obtain a ZAK. Stripe processes payments and keeps your name, e-mail address, billing address, payment details and invoices as our payment processor. Resend delivers the e-mails that carry licence keys. We share data with nobody else unless the law requires it.</p>

<h2>How long we keep it</h2>
<ul>
<li>Zoom sign-in tokens: until you press サインアウト (Sign out) in backhaul (this also revokes them at Zoom), until you remove backhaul from your Zoom account (the server discards them the next time they would be used), or until we stop your licence. Tokens left with an expired licence are deleted together with its records.</li>
<li>Licence and PC records: while the licence is valid and up to 12 months after, for support and accounting. Records we must keep by law (for example invoices) are kept as long as the law requires.</li>
</ul>

<h2>Your rights</h2>
<p>You may ask us for a copy of the data we hold about you, to correct it, to delete it, to restrict or object to its use, or to receive it in a portable form. Write to ${mail}; we answer within 30 days. You can withdraw the Zoom sign-in at any time as described above. If you are in the EEA or the UK you may also complain to your data protection authority. In Japan, requests under the Act on the Protection of Personal Information go to the same address.</p>

<h2>Security</h2>
<p>All connections use TLS. Licence keys are stored only as hashes, Zoom tokens only encrypted, and the encryption key is held separately as a platform secret. Access to the server is limited to its operator.</p>

<h2>Children</h2>
<p>backhaul is a professional tool and is not directed at children.</p>

<h2>Changes</h2>
<p>If this policy changes we will post the new version here and change the date above.</p>`,
  },
  {
    path: "/terms",
    title: "Terms of Use",
    lead: "The agreement between you and SEVENTHWELL for using backhaul.",
    body: `
<h2>1. The agreement</h2>
<p>These terms apply when you install or use backhaul, a Windows application by SEVENTHWELL (Tokyo, Japan). If you use it for an organisation, you accept the terms for that organisation.</p>

<h2>2. Licence</h2>
<p>A licence key lets you run backhaul on the number of PCs stated at purchase, until the stated expiry date. The licence is not exclusive and may not be resold, rented or shared outside your organisation. You may not copy, modify, reverse engineer or remove protections from the software except where the law allows it regardless of these terms.</p>

<h2>3. Zoom</h2>
<p>backhaul uses the Zoom Meeting SDK. You need your own Zoom account where Zoom requires one, and your use of Zoom is governed by Zoom's own terms. You are responsible for having the permission of the meeting host and of the people taking part, including the host's consent that Zoom asks for before backhaul can receive video and audio. Zoom is a trademark of Zoom Video Communications, Inc.; backhaul is not made or endorsed by Zoom.</p>

<h2>4. Acceptable use</h2>
<p>Do not use backhaul to join meetings you are not entitled to join, to capture people without their knowledge, to break the law, or to interfere with Zoom's services or our licence server.</p>

<h2>5. Fees and refunds</h2>
<p>Prices, the licence period and the payment method are shown at purchase. Subscriptions are paid monthly or yearly in advance through Stripe and renew automatically until you cancel. You can cancel, change the plan or change the card at any time on the billing page linked in the e-mail that brought your key; after cancelling, the licence works until the end of the period already paid. If a renewal cannot be charged, the licence stops working three days after the unpaid period began, and works again once the payment succeeds. When you change to a plan with fewer PCs, the PCs that were unused the longest lose their registration. A free trial key works for 14 days on up to 3 PCs, once per customer.</p>
<p>Unless the law gives you a right to withdraw, fees are not refundable once a licence key has been issued; if backhaul does not work as described and we cannot fix it in a reasonable time, write to us and we will refund the unused period.</p>

<h2>6. Third-party software</h2>
<p>backhaul includes third-party components (among them Qt, the Zoom Meeting SDK and the Steinberg VST 3 SDK) under their own licences, which are included with the software.</p>

<h2>7. Live events</h2>
<p>backhaul is used in live production, where networks, Zoom's services and hardware can fail. Keep a fallback and test before each event.</p>

<h2>8. Warranty and liability</h2>
<p>backhaul is provided "as is". To the extent the law allows, we give no warranty of uninterrupted or error-free operation, and our total liability for any claim is limited to the fees you paid for the licence in the 12 months before the claim. We are not liable for indirect or consequential loss, such as loss of profit or the cost of a cancelled or disrupted event. Nothing here limits liability that cannot be limited by law, such as for intent or gross negligence.</p>

<h2>9. Ending</h2>
<p>You may stop using backhaul at any time. We may suspend or end a licence if these terms are seriously broken. When a licence ends, remove the software; sections 6, 8 and 10 continue to apply.</p>

<h2>10. Law</h2>
<p>These terms are governed by the laws of Japan, and the Tokyo District Court has exclusive jurisdiction at first instance, without taking away consumer rights you have under the law of the country where you live.</p>

<h2>11. Changes and contact</h2>
<p>We may update these terms; the new version is posted here with a new date, and continued use after that means acceptance. Questions: ${mail}.</p>`,
  },
  {
    path: "/support",
    title: "Support",
    lead: "How to reach us, and what to send so we can help quickly.",
    body: `
<h2>Contact</h2>
<p>Email ${mail}. We answer in English or Japanese, normally within 3 business days (Monday to Friday, Japan time). Support is by email only.</p>

<h2>What to include</h2>
<ul>
<li>The backhaul version and your Windows version.</li>
<li>What you did, what you expected, and what happened. A photo or screenshot of the screen helps most.</li>
<li>The role the PC was in (for example "Audio" or "All in one") and the audio and capture devices in use.</li>
</ul>
<p>Never send your licence key, Zoom password or meeting passcode. We will not ask for them.</p>

<h2>Common questions</h2>
<ul>
<li><b>"映像と音を受け取る許可を待っています" (waiting for permission to receive video and audio).</b> Zoom requires the host's consent. Ask the host to make the backhaul participant a co-host, or to allow the request shown in the meeting.</li>
<li><b>Cannot join a meeting hosted by another account.</b> Sign in under 設定 (Settings) &gt; Zoom接続 &gt; Zoomアカウント, then join again.</li>
<li><b>Moving a licence to another PC.</b> Press このPCを外す (Remove this PC) under 設定 &gt; Zoom接続 on the old PC, then enter the key on the new one.</li>
</ul>
<p>The <a href="/docs">guide</a> explains adding, using and removing backhaul. Privacy and data deletion requests: see the <a href="/privacy">privacy policy</a>.</p>`,
  },
  {
    path: "/docs",
    title: "Guide: adding, using and removing backhaul",
    lead: "backhaul for Zoom, step by step.",
    body: `
<h2>What backhaul does</h2>
<p>backhaul is a Windows application for live events with remote presenters. It joins a Zoom meeting as a participant, shows each chosen presenter full-screen on its own monitor output, sends each presenter's voice to its own channel of an audio interface, and sends the venue's programme picture and mix-minus sound back to the meeting.</p>

<h2>Requirements</h2>
<ul>
<li>Windows 10 or 11 (64-bit) and a backhaul licence key.</li>
<li>A Zoom account. To join meetings hosted by another Zoom account, you sign in with Zoom once (below).</li>
</ul>

<p>The interface is currently in Japanese. Below, names are written as they appear on screen, with English in brackets.</p>

<h2>Adding backhaul</h2>
<ol>
<li>Install backhaul on the PC and start it.</li>
<li>Open 設定 (Settings) &gt; Zoom接続 (Zoom connection), and enter your licence key under ライセンス (Licence).</li>
<li>Under Zoomアカウント (Zoom account), press サインイン (Sign in). Your browser opens Zoom's consent page; it asks for one permission, "View a user's token", which lets backhaul obtain a short-lived Zoom Access Key so that Zoom knows which user this PC acts for. Press Allow, then return to backhaul. One sign-in covers all PCs on the same licence.</li>
</ol>

<h2>Using backhaul</h2>
<ol>
<li>Choose the role of this PC, このPCの役割: カメラ (Camera), 2ショット (Two-shot), スライド (Slides), 音声 (Audio), 返し (Return) or 1台でぜんぶ (All in one).</li>
<li>Under 設定 &gt; Zoom接続, enter the meeting ID and passcode and press 会議に入る (Join). backhaul appears in the meeting as a participant with the name you set.</li>
<li>Zoom asks the host to allow backhaul to receive video and audio. The host allows the request or makes the backhaul participant a co-host.</li>
<li>Assign presenters to slots, choose the monitor and audio channel for each slot, and, if you send a return, choose the capture device and audio input under 設定 &gt; 返し (Return).</li>
<li>Press 会議から出る (Leave) to leave the meeting.</li>
</ol>

<h2>Removing backhaul</h2>
<ol>
<li>In backhaul, open 設定 &gt; Zoom接続 and press サインアウト (Sign out). This revokes backhaul's access at Zoom and deletes the stored sign-in from our server, for all PCs on the licence.</li>
<li>Or remove it from Zoom: sign in to the <a href="https://marketplace.zoom.us/user/installed" rel="noopener">Zoom App Marketplace</a>, open Manage &gt; Added Apps, find backhaul and press Remove.</li>
<li>Press このPCを外す (Remove this PC) to free the licence seat, then uninstall backhaul from Windows (Settings &gt; Apps).</li>
</ol>
<p>To have all remaining records deleted, write to ${mail}. See the <a href="/privacy">privacy policy</a> for what is kept and for how long.</p>

<h2>Help</h2>
<p>See <a href="/support">support</a>.</p>`,
  },
];

function layout(page: Page): string {
  const nav = pages
    .map((p) => (p.path === page.path ? `<b aria-current="page">${p.title.split(":")[0]}</b>` : `<a href="${p.path}">${p.title.split(":")[0]}</a>`))
    .join("");
  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>${page.title} | backhaul</title>
<meta name="description" content="${page.lead}">
<style>
:root{--bg:#0a0b0c;--line:#2a2d31;--ink:#f2f2f0;--mute:#9a9fa6;--ok:#3ddc84;
--sans:system-ui,-apple-system,"Segoe UI","Hiragino Sans","Noto Sans JP",sans-serif;--mono:ui-monospace,"SF Mono",Menlo,Consolas,monospace}
*{box-sizing:border-box}
html{background:var(--bg);color:var(--ink);font:16px/1.7 var(--sans);-webkit-text-size-adjust:100%}
body{margin:0;padding:0 20px 72px}
header,main,footer{max-width:720px;margin:0 auto}
header{display:flex;flex-wrap:wrap;align-items:baseline;gap:8px 20px;padding:22px 0 16px;border-bottom:1px solid var(--line)}
header .name{font:700 14px var(--mono);letter-spacing:.08em;margin-right:auto}
nav{display:flex;flex-wrap:wrap;gap:6px 18px;font-size:14px}
nav a{color:var(--mute);text-decoration:none}
nav a:hover,nav a:focus-visible{color:var(--ink);text-decoration:underline}
nav b{font-weight:700}
h1{font-size:30px;line-height:1.25;margin:44px 0 8px;letter-spacing:-.01em}
.lead{color:var(--mute);margin:0 0 6px}
.date{font:13px var(--mono);color:var(--mute);margin:0 0 28px}
h2{font-size:18px;margin:36px 0 8px}
p,li{color:#d9dad7}
p{margin:0 0 14px}
ul,ol{margin:0 0 14px;padding-left:22px}
li{margin:0 0 8px}
a{color:var(--ok)}
footer{margin-top:56px;padding-top:16px;border-top:1px solid var(--line);font-size:13px;color:var(--mute)}
</style>
</head>
<body>
<header><span class="name">backhaul</span><nav aria-label="Pages">${nav}</nav></header>
<main>
<h1>${page.title}</h1>
<p class="lead">${page.lead}</p>
<p class="date">Last updated ${UPDATED}</p>
${page.body}
</main>
<footer>SEVENTHWELL, Tokyo, Japan. ${mail}</footer>
</body>
</html>`;
}

/** The page for `path`, or null when there is none. */
export function publicPage(path: string): string | null {
  const page = pages.find((p) => p.path === path);
  return page ? layout(page) : null;
}

export const publicPaths: readonly string[] = pages.map((p) => p.path);
