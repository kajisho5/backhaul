// Every sentence of the site, in English and Japanese, and the prices shown.
// The amounts charged are the prices at Stripe; these numbers are what the page says and
// must be changed together with them.

export type Lang = "en" | "ja";

export const GATE = "https://backhaul-gate.seventh-well.com";
/** Stripe's sign-in page for subscribers (test environment until real payments are switched on). */
export const BILLING = "https://billing.stripe.com/p/login/test_aFadR8emVbTy4gugmU6oo00";
/** False until Zoom has approved the app: the pages then say that payments are only tests. */
export const SALES_OPEN = false;
/** Where the installer is downloaded: an address of this site that forwards to the newest one (public/_redirects). */
export const DOWNLOAD_URL = "/download";

export const PLANS = [
  { seats: 5, month: { usd: 69, jpy: 9800 }, year: { usd: 690, jpy: 98000 } },
  { seats: 10, month: { usd: 119, jpy: 16800 }, year: { usd: 1190, jpy: 168000 } },
  { seats: 20, month: { usd: 199, jpy: 29800 }, year: { usd: 1990, jpy: 298000 } },
] as const;

/** Japanese consumption tax, added to payments in yen (the server adds the same rate at Stripe). */
export const JAPAN_TAX_PERCENT = 10;

export function withTax(amount: number): number {
  return Math.round((amount * (100 + JAPAN_TAX_PERCENT)) / 100);
}

export function money(amount: number, currency: "usd" | "jpy"): string {
  return (currency === "usd" ? "$" : "¥") + amount.toLocaleString("en-US");
}

const en = {
  lang: "en" as Lang,
  currency: "usd" as "usd" | "jpy",
  home: "/",
  other: { href: "/ja/", label: "日本語" },
  title: "backhaul: Zoom presenters on your venue's outputs",
  description: "A Windows app for live events: every remote presenter on its own monitor output and audio channel, and your programme back to Zoom.",
  preview: "Preview. Payments on this site run in Stripe's test mode and nothing is charged. Sales open after Zoom has reviewed the app.",
  nav: { how: "How it works", pricing: "Pricing", guide: "Guide", trial: "Start free trial" },
  hero: {
    badge: "For Windows 10 and 11",
    lead: "backhaul puts every remote presenter on its own monitor output and audio channel, and returns your programme to Zoom.",
    pricing: "See pricing",
    onAir: "On air",
    shot: "/screen-en.png",
    shotAlt: "The backhaul operation screen with four presenters, slides and the return",
  },
  how: {
    title: "Built for the people behind the switcher",
    lead: "Each PC takes one job, or one PC takes them all.",
    cells: [
      { title: "One presenter, one output", text: "Full screen on the monitor you choose, up to 1080p. Name cards cover a camera that is off." },
      { title: "One voice, one channel", text: "ASIO or WASAPI, with level, delay and VST 3 plug-ins per person." },
      { title: "Pick a role per PC", text: "Small shows run on one PC. Large ones split the work." },
      { title: "Send the room back", text: "Your programme and mix-minus reach the presenters as a camera or a shared screen." },
      { title: "One licence, up to 20 PCs", text: "Sign in with Zoom once. Every PC on the licence is covered." },
    ],
    people: ["Alex Sample", "Hana Example", "Mina Specimen"],
    roles: ["All in one", "Camera", "Two-shot", "Slides", "Audio", "Return"],
    switcher: "Switcher out",
  },
  pricing: {
    title: "Pricing",
    note: "US dollars, excluding tax.",
    otherCurrency: { text: "Paying in yen?", link: "Prices in Japanese yen", href: "/ja/#pricing" },
    monthly: "Monthly",
    yearly: "Yearly, 2 months free",
    perMonth: "per month",
    perYear: "per year",
    orYear: (price: string) => `or ${price} per year`,
    orMonth: (price: string) => `or ${price} per month`,
    taxIn: (_price: string) => "",
    pcs: (n: number) => `${n} PCs`,
    plans: ["Standard", "Team", "Rental house"],
    buy: (name: string) => `Buy ${name}`,
    opening: "Opening Stripe…",
    trialTitle: "Try it with 3 PCs for 14 days",
    trialText: "No card needed. The key arrives by email.",
    fine: "Your licence key is shown right after payment and sent by email. Cancel any time; the key works until the end of the paid period.",
    errors: {
      not_configured: "Payments are not switched on yet. Please try again later.",
      slow_down: "Too many attempts. Please wait ten minutes and try again.",
      stripe_failed: "Stripe did not answer. Nothing was charged; please try again.",
      network: "Could not reach the server. Check your connection and try again.",
    },
  },
  trial: {
    title: "Free trial",
    heading: "Try backhaul for 14 days",
    lead: "3 PCs, every feature, no card. We send the licence key to your email address.",
    label: "Email address",
    send: "Send my trial key",
    sending: "Sending…",
    done: (email: string) => `The key is on its way to ${email}. It usually arrives within a minute; check the spam folder if it does not.`,
    next: "How to get started",
    privacy: "We use the address once, to send the key, and keep only a one-way hash of it so that each address gets one trial.",
    errors: {
      bad_email: "That does not look like an email address.",
      already_used: "This address has already had a trial. Write to backhaul@seventh-well.com if you need more time.",
      slow_down: "Too many requests. Please try again tomorrow.",
      mail_failed: "The email could not be sent. Check the address and try again.",
      not_configured: "Trials are not switched on yet. Please try again later.",
      network: "Could not reach the server. Check your connection and try again.",
    },
  },
  thanks: {
    title: "Your licence key",
    waiting: "Confirming your payment…",
    heading: "Thank you. This is your licence key.",
    headingShown: "Thank you for subscribing.",
    once: "It is shown here once. Copy it now; the same key is in your email.",
    copy: "Copy key",
    copied: "Copied",
    seats: (n: number) => `Valid on up to ${n} PCs. Enter the same key on each PC.`,
    mailed: (email: string) => `Also sent to ${email}.`,
    notMailed: "The email is delayed and will follow. Copy the key now.",
    shown: (tail: string, email: string) => `The key (ending in ${tail}) was already shown once. You will find it in the email sent to ${email}.`,
    next: "How to get started",
    slow: "Stripe has not confirmed the payment yet. Leave this page open, or come back through the link in a minute. If you were charged and no key arrives, write to backhaul@seventh-well.com.",
    missing: "This page needs the link Stripe sends you to after paying.",
    errors: {
      unknown_order: "This order was not found.",
      slow_down: "Too many attempts. Please wait ten minutes and reload.",
      not_configured: "Payments are not switched on yet.",
      stripe_failed: "Stripe did not answer. Reload the page in a moment.",
      network: "Could not reach the server. Check your connection and reload.",
    },
  },
  start: {
    title: "Getting started",
    heading: "Getting started",
    lead: "From the key in your inbox to presenters on your outputs.",
    steps: [
      { title: "Install backhaul", text: "Download the installer on each Windows 10 or 11 PC (64-bit) and run it." },
      { title: "Enter your licence key", text: "Start backhaul, open Settings > Zoom connection, and type the key under Licence key. Use the same key on every PC of your plan." },
      { title: "Sign in with Zoom, once", text: "On any one PC, press Sign in with Zoom under Zoom account and allow the request in your browser. This covers every PC on the licence, and is needed to join meetings hosted by other accounts." },
      { title: "Pick a role and join", text: "Choose what this PC does (All in one, Camera, Two-shot, Slides, Audio or Return), enter the meeting ID and passcode, and join. Ask the host to allow backhaul to receive video and audio." },
    ],
    download: "Download for Windows",
    noDownload: "The installer will be available here when sales open.",
    downloadNote: "Windows 10 and 11, 64-bit. Windows may ask you to confirm the first time you open the installer.",
    guide: "The full guide",
    billing: "Change plan or card, cancel, receipts",
  },
  faq: {
    title: "Questions",
    items: [
      { q: "What do I need to run it?", a: "A PC with Windows 10 or 11 (64-bit) and a Zoom account. There is no version for macOS." },
      { q: "How are PCs counted?", a: "Each PC you enter the licence key on takes one place, up to the number in your plan. Press “Remove this PC” in backhaul to free a place for another PC." },
      { q: "What happens when the trial ends?", a: "The trial key stops working after 14 days and nothing is charged. To keep using backhaul, choose a plan; you get a new key." },
      { q: "How do I cancel or change my plan?", a: "On the subscriber page linked in the e-mail that brought your key, at any time. After you cancel, the key works until the end of the period already paid. If you move to a plan with fewer PCs, the PCs that were unused the longest lose their place." },
      { q: "Can I get a refund?", a: "Once a licence key has been issued, fees are not refundable unless the law gives you that right. If backhaul does not work as described and we cannot fix it in a reasonable time, write to us and we refund the unused period." },
      { q: "How do I pay, and do I get a receipt?", a: "By credit card, through Stripe. The e-mail with your key has a link to the receipt, and past receipts are on the subscriber page." },
      { q: "Does the meeting host have to do anything?", a: "Yes. Zoom asks the host to allow backhaul to receive video and audio. Ask the host before the event." },
    ],
  },
  descriptions: {
    trial: "Try backhaul for 14 days on 3 PCs. No card needed; the licence key arrives by e-mail.",
    start: "Install backhaul, enter your licence key, sign in with Zoom and join your first meeting.",
  },
  notFound: { title: "Page not found", text: "There is nothing at this address.", home: "Back to the start page" },
  footer: { privacy: "Privacy", terms: "Terms", support: "Support", billing: "Manage subscription", legal: "特定商取引法に基づく表記", zoom: "Zoom is a trademark of Zoom Video Communications, Inc. backhaul is not made or endorsed by Zoom." },
};

type Texts = typeof en;

const ja: Texts = {
  lang: "ja",
  currency: "jpy",
  home: "/ja/",
  other: { href: "/", label: "English" },
  title: "backhaul：Zoomの演者を、会場の出力へ",
  description: "リアル開催イベント向けのWindowsアプリ。Zoomの演者を1人ずつモニター出力と音声チャンネルに出し、会場の映像と音をZoomへ返します。",
  preview: "準備中のページです。このサイトの支払いはStripeのテスト環境で動いており、請求は発生しません。販売はZoomの審査が済んでから始めます。",
  nav: { how: "しくみ", pricing: "料金", guide: "はじめかた", trial: "無料で試す" },
  hero: {
    badge: "Windows 10・11 対応",
    lead: "Zoomの演者を1人ずつ、モニター出力と音声チャンネルへ。会場の映像と音は、Zoomへ返します。",
    pricing: "料金を見る",
    onAir: "本番中",
    shot: "/screen-ja.png",
    shotAlt: "演者4人とスライド、返しが並んだ backhaul の操作画面",
  },
  how: {
    title: "スイッチャーの後ろにいる人のために",
    lead: "PC1台に役割を1つ。小さな現場なら1台でぜんぶ。",
    cells: [
      { title: "演者1人に、出力1つ", text: "選んだモニターに全画面で、1080pまで。カメラがオフのあいだは名前カードを出します。" },
      { title: "声1つに、チャンネル1つ", text: "ASIOでもWASAPIでも。レベル、ディレイ、VST 3プラグインを1人ずつ。" },
      { title: "PCごとに役割を選ぶ", text: "小さな現場は1台で。大きな現場は役割を分けて。" },
      { title: "会場を演者へ返す", text: "本線の映像とマイナスワンの音を、カメラか画面共有としてZoomへ。" },
      { title: "1つのライセンスで20台まで", text: "Zoomへのサインインは1回だけ。同じライセンスのPCは、すべてサインインした状態になります。" },
    ],
    people: ["見本 太郎", "例 花子", "標本 美奈"],
    roles: ["1台でぜんぶ", "カメラ", "2ショット", "スライド", "音声", "返し"],
    switcher: "本線出力",
  },
  pricing: {
    title: "料金",
    note: "日本円・税別です（消費税10%が加わります）。",
    otherCurrency: { text: "ドルで支払う場合は", link: "米ドルの料金", href: "/#pricing" },
    monthly: "月額",
    yearly: "年額（2か月分お得）",
    perMonth: "／月",
    perYear: "／年",
    orYear: (price: string) => `年額なら ${price}`,
    orMonth: (price: string) => `月額なら ${price}`,
    taxIn: (price: string) => `税込 ${price}`,
    pcs: (n: number) => `PC ${n}台`,
    plans: ["スタンダード", "チーム", "レンタルハウス"],
    buy: (name: string) => `${name}を申し込む`,
    opening: "Stripeを開いています…",
    trialTitle: "PC 3台で14日間、無料で試せます",
    trialText: "カードは要りません。キーはメールで届きます。",
    fine: "ライセンスキーは、支払いのすぐあとに画面に表示し、メールでもお送りします。解約はいつでもでき、支払い済みの期間の終わりまで使えます。",
    errors: {
      not_configured: "支払いの受け付けは、まだ始まっていません。時間をおいてお試しください。",
      slow_down: "操作が続きすぎました。10分ほど待ってからお試しください。",
      stripe_failed: "Stripeから返事がありませんでした。請求は発生していません。もう一度お試しください。",
      network: "サーバーに届きませんでした。通信を確かめて、もう一度お試しください。",
    },
  },
  trial: {
    title: "無料体験",
    heading: "14日間、無料で試す",
    lead: "PC 3台まで、機能の制限なし、カード不要。ライセンスキーをメールでお送りします。",
    label: "メールアドレス",
    send: "体験用のキーを送る",
    sending: "送っています…",
    done: (email: string) => `${email} へキーを送りました。ふつう1分ほどで届きます。届かないときは迷惑メールもご確認ください。`,
    next: "はじめかた",
    privacy: "アドレスはキーを送るために1回だけ使います。同じアドレスに体験を1回だけにするため、元に戻せない形（ハッシュ）だけを残します。",
    errors: {
      bad_email: "メールアドレスの形になっていません。",
      already_used: "このアドレスは体験を利用済みです。延長のご相談は backhaul@seventh-well.com へどうぞ。",
      slow_down: "申し込みが続きすぎました。明日もう一度お試しください。",
      mail_failed: "メールを送れませんでした。アドレスを確かめて、もう一度お試しください。",
      not_configured: "体験の受け付けは、まだ始まっていません。時間をおいてお試しください。",
      network: "サーバーに届きませんでした。通信を確かめて、もう一度お試しください。",
    },
  },
  thanks: {
    title: "ライセンスキー",
    waiting: "支払いを確かめています…",
    heading: "ライセンスキーをお届けします",
    headingShown: "ご契約ありがとうございます。",
    once: "ここに表示するのは1回だけです。いまコピーしてください。同じキーをメールでもお送りしています。",
    copy: "キーをコピー",
    copied: "コピーしました",
    seats: (n: number) => `PC ${n}台まで使えます。各PCに同じキーを入れてください。`,
    mailed: (email: string) => `${email} にもお送りしました。`,
    notMailed: "メールは遅れて届きます。いまキーをコピーしておいてください。",
    shown: (tail: string, email: string) => `キー（末尾 ${tail}）は、すでに1回表示しました。${email} へお送りしたメールをご覧ください。`,
    next: "はじめかた",
    slow: "Stripeでの支払いの確認が、まだ済んでいません。このページを開いたままにするか、1分ほどして同じリンクを開き直してください。請求があったのにキーが届かないときは backhaul@seventh-well.com へご連絡ください。",
    missing: "このページは、支払いのあとにStripeから移ってくるリンクで開いてください。",
    errors: {
      unknown_order: "この注文は見つかりませんでした。",
      slow_down: "操作が続きすぎました。10分ほど待ってから開き直してください。",
      not_configured: "支払いの受け付けは、まだ始まっていません。",
      stripe_failed: "Stripeから返事がありませんでした。少し待って開き直してください。",
      network: "サーバーに届きませんでした。通信を確かめて、開き直してください。",
    },
  },
  start: {
    title: "はじめかた",
    heading: "はじめかた",
    lead: "届いたライセンスキーを入力して、演者をモニター出力と音声チャンネルに出すまでの手順です。",
    steps: [
      { title: "backhaul をインストールする", text: "backhaul を使う Windows 10・11（64ビット）のPCそれぞれに、インストーラーをダウンロードして実行します。" },
      { title: "ライセンスキーを入力する", text: "backhaul を起動し、設定の「Zoom接続」にある「ライセンスキー」に入力します。同じキーを、プランの台数までのPCに入力してください。" },
      { title: "Zoomにサインインする", text: "どれか1台で「Zoomアカウント」の「Zoomにサインイン」を押し、開いたブラウザで許可します。サインインは1回だけで、同じライセンスのPCはすべてサインインした状態になります。このサインインは、ほかのアカウントが開いた会議に入るために必要です。" },
      { title: "役割を選んで会議に参加する", text: "このPCの役割（1台でぜんぶ・カメラ・2ショット・スライド・音声・返し）を選び、ミーティングIDとパスコードを入力して参加します。映像と音を受け取る許可をホストにお願いしてください。" },
    ],
    download: "Windows用をダウンロード",
    noDownload: "インストーラーは、販売の開始に合わせてここに置きます。",
    downloadNote: "Windows 10・11（64ビット）用です。インストーラーを初めて開くときに、Windows が確認を求めることがあります。",
    guide: "くわしいガイド（英語）",
    billing: "プラン・カードの変更、解約、領収書",
  },
  faq: {
    title: "よくある質問",
    items: [
      { q: "使うのに必要なものは何ですか。", a: "Windows 10 または 11（64ビット）のPCと、Zoom のアカウントが必要です。macOS 用はありません。" },
      { q: "PCの台数はどう数えますか。", a: "ライセンスキーを入力したPC 1台ごとに、プランの台数の枠を1つ使います。backhaul の「このPCを外す」を押すと枠が空き、別のPCで使えます。" },
      { q: "無料体験が終わるとどうなりますか。", a: "体験用のキーは14日で使えなくなり、料金は発生しません。続けて使う場合はプランを申し込んでください。新しいキーをお送りします。" },
      { q: "解約やプランの変更はどうすればよいですか。", a: "キーをお送りしたメールにある「ご契約者ページ」から、いつでも手続きできます。解約したあとも、支払い済みの期間の終わりまで使えます。台数の少ないプランに変えたときは、長く使っていないPCから順に登録が外れます。" },
      { q: "返金はできますか。", a: "ソフトウェアの性質上、ライセンスキーの発行後は、お客様のご都合による返金はお受けしていません。説明どおりに動かず、相当の期間内に直せない場合は、未使用の期間分を返金します。" },
      { q: "支払い方法と領収書を教えてください。", a: "クレジットカードで、決済は Stripe が行います。キーをお送りするメールに領収書へのリンクがあり、過去の領収書は「ご契約者ページ」で確認できます。" },
      { q: "会議のホストに頼むことはありますか。", a: "あります。backhaul が映像と音を受け取るには、Zoom の決まりでホストの許可が必要です。本番の前にホストへ依頼してください。" },
    ],
  },
  descriptions: {
    trial: "backhaul を PC 3台・14日間、無料で試せます。カードは不要で、ライセンスキーはメールで届きます。",
    start: "backhaul をインストールし、ライセンスキーを入力して、Zoom にサインインし、最初の会議に参加するまでの手順です。",
  },
  notFound: { title: "ページが見つかりません", text: "このアドレスには何もありません。", home: "最初のページへ" },
  footer: { privacy: "プライバシー", terms: "利用規約", support: "サポート", billing: "契約の管理", legal: "特定商取引法に基づく表記", zoom: "Zoom は Zoom Video Communications, Inc. の商標です。backhaul は Zoom が作ったものでも、Zoom が推奨するものでもありません。" },
};

export const texts: Record<Lang, Texts> = { en, ja };
export type { Texts };
