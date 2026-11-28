// Public pages the Zoom listing links to: privacy, terms, support and the guide.
// Plain text in one layout; nothing here is secret and nothing is read from a request.
// Privacy and terms also exist in Japanese (under /ja/), saying the same as the English ones:
// change the two languages together.

const UPDATED = "November 28, 2026";
const UPDATED_JA = "2026年11月28日";
const CONTACT = "backhaul@seventh-well.com";

type Page = { path: string; lang?: "ja"; title: string; lead: string; body: string };

/** The same page in the other language, where there is one. */
const TWINS: Record<string, string> = { "/privacy": "/ja/privacy", "/terms": "/ja/terms", "/ja/privacy": "/privacy", "/ja/terms": "/terms" };

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
<li>Zoom sign-in tokens: until you press Sign out in backhaul (this also revokes them at Zoom), until you remove backhaul from your Zoom account (the server discards them the next time they would be used), or until we stop your licence. Tokens left with an expired licence are deleted together with its records.</li>
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
<li><b>"Waiting for permission to receive video and audio".</b> Zoom requires the host's consent. Ask the host to make the backhaul participant a co-host, or to allow the request shown in the meeting.</li>
<li><b>Cannot join a meeting hosted by another account.</b> Press Sign in with Zoom under Settings &gt; Zoom connection &gt; Zoom account, then join again.</li>
<li><b>Moving a licence to another PC.</b> Press Remove this PC under Settings &gt; Zoom connection on the old PC, then enter the key on the new one.</li>
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

<p>The interface is in English; Japanese can be chosen at the top of Settings. Below, names are written as they appear on screen in English.</p>

<h2>Adding backhaul</h2>
<ol>
<li>Install backhaul on the PC and start it.</li>
<li>Open Settings &gt; Zoom connection, and enter your licence key under Licence.</li>
<li>Under Zoom account, press Sign in with Zoom. Your browser opens Zoom's consent page; it asks for one permission, "View a user's token", which lets backhaul obtain a short-lived Zoom Access Key so that Zoom knows which user this PC acts for. Press Allow, then return to backhaul. One sign-in covers all PCs on the same licence.</li>
</ol>

<h2>Using backhaul</h2>
<ol>
<li>Choose the Role of this PC: Camera, Two-shot, Slides, Audio, Return or All in one.</li>
<li>Under Settings &gt; Zoom connection, enter the meeting ID and passcode and press Join. backhaul appears in the meeting as a participant with the name you set.</li>
<li>Zoom asks the host to allow backhaul to receive video and audio. The host allows the request or makes the backhaul participant a co-host.</li>
<li>Assign presenters to slots, choose the monitor and audio channel for each slot, and, if you send a return, choose the capture device and audio input under Settings &gt; Return.</li>
<li>Press Leave to leave the meeting.</li>
</ol>

<h2>Removing backhaul</h2>
<ol>
<li>In backhaul, open Settings &gt; Zoom connection and press Sign out. This revokes backhaul's access at Zoom and deletes the stored sign-in from our server, for all PCs on the licence.</li>
<li>Or remove it from Zoom: sign in to the <a href="https://marketplace.zoom.us/user/installed" rel="noopener">Zoom App Marketplace</a>, open Manage &gt; Added Apps, find backhaul and press Remove.</li>
<li>Press Remove this PC to free the licence seat, then uninstall backhaul from Windows (Settings &gt; Apps).</li>
</ol>
<p>To have all remaining records deleted, write to ${mail}. See the <a href="/privacy">privacy policy</a> for what is kept and for how long.</p>

<h2>Help</h2>
<p>See <a href="/support">support</a>.</p>`,
  },
  {
    path: "/ja/privacy",
    lang: "ja",
    title: "プライバシーポリシー",
    lead: "backhaul とそのライセンスサーバーが何を保存するか、なぜ保存するか、どうすれば削除できるかを説明します。",
    body: `
<h2>運営者</h2>
<p>backhaul は SEVENTHWELL（東京）が開発・販売している Windows 用のアプリです。Zoom の会議に参加している人の映像と音を会場の機材へ出し、会場の映像と音を会議へ返します。連絡先：${mail}</p>

<h2>お使いのPCの中に残るもの</h2>
<p>会議の内容は、すべて backhaul を動かしているPCの中にとどまります。映像、音声、共有された画面、参加者の名前、会議のパスコードがこれに当たります。これらが当方やほかの誰かに送られることはなく、backhaul は会議を録画・録音しません。ライセンスキーと、PCが登録済みであることを示すトークンは、Windows の資格情報マネージャーに保存します。設定はPCの中のファイルに保存します。</p>

<h2>ライセンスサーバーが保存するもの</h2>
<ul>
<li><b>ライセンス。</b> ライセンスキーの一方向ハッシュと末尾の4文字、当方の記録用のメモ（購入者の名前など）、使えるPCの台数、有効期限。登録したPCごとに、ランダムな端末ID、PCの名前、登録トークンのハッシュ、登録した日時と最後に使った日時。</li>
<li><b>Zoom のサインイン（backhaul で Zoom にサインインした場合だけ）。</b> Zoom がお客様のアカウントについて当方に発行する OAuth トークンを、AES-256-GCM で暗号化して保存します。使う目的は1つだけです。お客様のPCが会議に参加するたびに、有効期間の短い Zoom Access Key（ZAK）を Zoom に求め、そのPCがどのユーザーとして参加するのかを Zoom に伝えます。ZAK はお客様のPCに渡し、サーバーには最長10分だけ保持します。当方は、お客様のプロフィール、会議、録画、連絡先、チャットを読みません。</li>
<li><b>契約（購入した場合だけ）。</b> 支払いは Stripe が処理します。当方がカード番号を見ることも保存することもありません。サーバーはお客様の Stripe の契約のIDをライセンスと合わせて保存し、ライセンスキーを送るときに、お客様が Stripe に登録したメールアドレスを Stripe から読み取ります。キーがお客様に届くまで（支払いの直後に1回表示し、メールでも送ります）、暗号化したキーの控えを保持します。保持するのは、メールを送ってから最長1週間です。</li>
<li><b>無料体験（申し込んだ場合だけ）。</b> 入力されたメールアドレスは体験用のキーを送るために1回だけ使います。同じアドレスの体験を1回に限るため、アドレスの一方向ハッシュを保存し、アドレスそのものは保存しません。</li>
<li><b>技術的なログ。</b> 運用と不正利用の防止のため、ホスティング事業者が通常のアクセスログ（日時、アドレス、パス）を記録します。</li>
</ul>
<p>当方は、アクセス解析や広告を使わず、個人情報を販売せず、お客様のデータをAIモデルの学習に使いません。</p>

<h2>処理を委託している事業者</h2>
<p>ライセンスサーバーは Cloudflare（Workers と D1）の上で動いており、Cloudflare は当方に代わって上記のデータを処理します。Zoom Video Communications は、会議への参加と ZAK の取得のためにお客様のPCと当方のサーバーが送る要求を受け取ります。Stripe は当方の決済代行事業者として支払いを処理し、お客様の氏名、メールアドレス、請求先住所、支払い情報、請求書を保持します。Resend は、ライセンスキーを載せたメールを配送します。法令で求められる場合を除き、これら以外の第三者にデータを提供しません。</p>

<h2>保存する期間</h2>
<ul>
<li>Zoom のサインインのトークン：backhaul で「サインアウト」を押すまで（Zoom 側でも無効になります）、Zoom のアカウントから backhaul を削除するまで（次に使おうとした時点でサーバーが破棄します）、または当方がライセンスを停止するまで保存します。期限の切れたライセンスに残ったトークンは、そのライセンスの記録と一緒に削除します。</li>
<li>ライセンスとPCの記録：ライセンスが有効なあいだと、その後最長12か月、サポートと会計のために保存します。法令で保存が義務づけられている記録（請求書など）は、法令が定める期間、保存します。</li>
</ul>

<h2>お客様の権利</h2>
<p>お客様は、当方が保有するお客様のデータについて、写しの提供、訂正、削除、利用の制限、利用への異議、ほかへ移せる形式での提供を求めることができます。${mail} までご連絡ください。30日以内に回答します。Zoom のサインインは、上に書いた方法でいつでも取り消せます。EEA または英国にお住まいの場合は、お住まいの国のデータ保護機関に苦情を申し立てることもできます。日本の個人情報の保護に関する法律に基づく請求も、同じ連絡先で受け付けます。</p>

<h2>安全管理</h2>
<p>通信はすべて TLS で保護します。ライセンスキーはハッシュだけを、Zoom のトークンは暗号化したものだけを保存し、暗号化の鍵はプラットフォームの秘密の値として別に保管します。サーバーを操作できるのは運営者だけです。</p>

<h2>子ども</h2>
<p>backhaul は業務用の道具であり、子どもを対象としていません。</p>

<h2>変更</h2>
<p>このポリシーを変更したときは、新しいものをこのページに掲載し、上の日付を改めます。</p>`,
  },
  {
    path: "/ja/terms",
    lang: "ja",
    title: "利用規約",
    lead: "backhaul の利用について、お客様と SEVENTHWELL のあいだで交わす契約です。",
    body: `
<h2>1. この契約</h2>
<p>この規約は SEVENTHWELL（東京）の Windows 用アプリ backhaul をインストールまたは使用するときに適用されます。組織のために使用する場合は、その組織としてこの規約に同意したものとします。</p>

<h2>2. ライセンス</h2>
<p>ライセンスキーがあれば、購入時に示された台数のPCで、示された有効期限まで backhaul を動かせます。このライセンスは非独占的で、転売、貸与、所属する組織の外との共有はできません。この規約にかかわらず法令が認める場合を除き、ソフトウェアの複製、改変、リバースエンジニアリング、保護の解除はできません。</p>

<h2>3. Zoom</h2>
<p>backhaul は Zoom Meeting SDK を使用しています。Zoom がアカウントを求める場面では、お客様自身の Zoom のアカウントが必要です。Zoom の利用には、Zoom の規約が適用されます。会議のホストと参加者から許可を得ることは、お客様の責任です。これには、backhaul が映像と音を受け取る前に Zoom が求めるホストの同意も含まれます。Zoom は Zoom Video Communications, Inc. の商標です。backhaul は Zoom が作ったものでも、Zoom が推奨するものでもありません。</p>

<h2>4. してはいけない使い方</h2>
<p>参加する権限のない会議に参加すること、本人に知らせずに人を撮影・録音すること、法令に違反すること、Zoom のサービスや当方のライセンスサーバーを妨害することに、backhaul を使わないでください。</p>

<h2>5. 料金と返金</h2>
<p>価格、ライセンスの期間、支払い方法は、購入時に表示します。契約の料金は、月ごとまたは年ごとに Stripe を通じて前払いで支払い、解約するまで自動で更新されます。解約、プランの変更、カードの変更は、キーをお送りしたメールにある契約者用のページからいつでもできます。解約したあとも、支払い済みの期間の終わりまでライセンスは有効です。更新の料金を請求できなかった場合、ライセンスは未払いの期間が始まってから3日後に使えなくなり、支払いが完了すると再び使えるようになります。台数の少ないプランに変更したときは、長く使っていないPCから順に登録が外れます。無料体験のキーは、お客様1人につき1回、最大3台のPCで14日間使えます。</p>
<p>法令がお客様に撤回の権利を認めている場合を除き、ライセンスキーの発行後は、料金を返金しません。backhaul が説明どおりに動かず、当方が相当の期間内に直せない場合は、ご連絡いただければ未使用の期間分を返金します。</p>

<h2>6. 第三者のソフトウェア</h2>
<p>backhaul には、第三者の部品（Qt、Zoom Meeting SDK、Steinberg VST 3 SDK など）が、それぞれのライセンスのもとで含まれています。各ライセンスの文面はソフトウェアに同梱しています。</p>

<h2>7. 本番での利用</h2>
<p>backhaul は、ネットワーク、Zoom のサービス、機材が止まることのあるライブの現場で使われます。予備の手段を用意し、イベントごとに事前のテストをしてください。</p>

<h2>8. 保証と責任</h2>
<p>backhaul は「現状のまま」提供します。法令が認める範囲で、当方は中断や誤りなく動作することを保証しません。いかなる請求についても、当方の責任の総額は、その請求の前の12か月間にお客様がライセンスに支払った料金の額を上限とします。当方は、逸失利益や、中止または支障の生じたイベントの費用など、間接的・派生的な損害について責任を負いません。故意または重大な過失による場合など、法令上制限できない責任は、この規約によって制限されません。</p>

<h2>9. 終了</h2>
<p>お客様は、いつでも backhaul の使用をやめることができます。この規約に対する重大な違反があった場合、当方はライセンスを停止または終了することがあります。ライセンスが終了したときは、ソフトウェアを削除してください。第6条、第8条、第10条は、終了後も適用されます。</p>

<h2>10. 準拠法</h2>
<p>この規約は日本法に準拠し、東京地方裁判所を第一審の専属的合意管轄裁判所とします。ただし、お住まいの国の法令がお客様に認めている消費者としての権利は、これによって失われません。</p>

<h2>11. 変更と連絡先</h2>
<p>当方は、この規約を変更することがあります。新しい規約は、新しい日付とともにこのページに掲載します。掲載のあとも使用を続けた場合は、新しい規約に同意したものとします。お問い合わせ：${mail}</p>`,
  },
];

function layout(page: Page): string {
  const ja = page.lang === "ja";
  // Japanese pages list the Japanese pages, and the English-only ones (support, guide) marked as such.
  const listed = pages.filter((p) => (ja ? p.lang === "ja" || TWINS[p.path] === undefined : p.lang === undefined));
  const label = (p: Page) => p.title.split(":")[0] + (ja && p.lang === undefined ? "（英語）" : "");
  const twin = TWINS[page.path];
  const nav =
    listed.map((p) => (p.path === page.path ? `<b aria-current="page">${label(p)}</b>` : `<a href="${p.path}">${label(p)}</a>`)).join("") +
    (twin === undefined ? "" : ja ? `<a href="${twin}" lang="en">English</a>` : `<a href="${twin}" lang="ja">日本語</a>`);
  return `<!doctype html>
<html lang="${ja ? "ja" : "en"}">
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
<header><span class="name">backhaul</span><nav aria-label="${ja ? "ページ" : "Pages"}">${nav}</nav></header>
<main>
<h1>${page.title}</h1>
<p class="lead">${page.lead}</p>
<p class="date">${ja ? `最終更新 ${UPDATED_JA}` : `Last updated ${UPDATED}`}</p>
${page.body}
</main>
<footer>${ja ? "SEVENTHWELL（東京）" : "SEVENTHWELL, Tokyo, Japan."} ${mail}</footer>
</body>
</html>`;
}

/** The page for `path`, or null when there is none. */
export function publicPage(path: string): string | null {
  const page = pages.find((p) => p.path === path);
  return page ? layout(page) : null;
}

export const publicPaths: readonly string[] = pages.map((p) => p.path);
