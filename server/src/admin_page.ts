// The owner's page for licences: issue, look, stop. One file, no build step, made for a phone.
// It holds nothing secret; every request it makes carries the admin token the owner types in.
// Whatever comes from the server is put on the page as text, never as HTML.

export function adminPage(): string {
  return `<!doctype html>
<html lang="ja">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="robots" content="noindex">
<title>backhaul ライセンス</title>
<style>
:root{--bg:#0a0b0c;--panel:#141618;--line:#2a2d31;--ink:#f2f2f0;--mute:#9a9fa6;--control:#1e2125;--ok:#3ddc84;--warn:#ffc83d;--bad:#ff4d5e;
--sans:system-ui,-apple-system,"Hiragino Sans","Noto Sans JP",sans-serif;--mono:ui-monospace,"SF Mono",Menlo,Consolas,monospace}
*{box-sizing:border-box}
html{background:var(--bg);color:var(--ink);font:15px/1.6 var(--sans);-webkit-text-size-adjust:100%}
body{margin:0;padding:env(safe-area-inset-top) 16px calc(env(safe-area-inset-bottom) + 40px)}
main{max-width:640px;margin:0 auto}
header{display:flex;align-items:baseline;gap:12px;padding:18px 0 14px;border-bottom:1px solid var(--line)}
header b{font:700 13px var(--mono);letter-spacing:.08em}
header span{font-weight:700}
header button{margin-left:auto}
h2{font-size:15px;margin:26px 0 10px}
p{margin:0 0 10px;color:var(--mute);font-size:13px}
label{display:block;font-size:12px;color:var(--mute);margin:12px 0 4px}
input{width:100%;height:44px;padding:0 12px;background:#0f1113;color:var(--ink);border:1px solid var(--line);border-radius:2px;font:16px var(--sans)}
input:focus-visible,button:focus-visible{outline:none;border-color:var(--ink)}
button{height:44px;padding:0 16px;background:transparent;color:var(--ink);border:1px solid var(--line);border-radius:2px;font:13px var(--sans);cursor:pointer;white-space:nowrap}
button.strong{background:var(--ink);color:var(--bg);border-color:var(--ink);font-weight:700}
button.small{height:36px;padding:0 12px}
button.danger{color:var(--bad);border-color:var(--bad)}
button:disabled{opacity:.4;cursor:default}
.row{display:flex;gap:8px;align-items:end}
.row>*{flex:1}
.row>button{flex:none}
.choices{display:flex;gap:0;border:1px solid var(--line);border-radius:2px}
.choices button{flex:1;border:0;border-radius:0;padding:0 6px}
.choices button[aria-pressed=true]{background:var(--ink);color:var(--bg);font-weight:700}
.note{min-height:20px;margin-top:10px;font-size:13px}
.note.bad{color:var(--bad)}
.note.ok{color:var(--ok)}
.issued{margin-top:14px;padding:14px;border:1px solid var(--warn)}
.issued code{display:block;font:700 17px var(--mono);letter-spacing:.04em;word-break:break-all;margin:6px 0 10px;user-select:all}
.issued p{color:var(--warn);margin:0}
ul{list-style:none;margin:0;padding:0;border-top:1px solid var(--line)}
li{padding:14px 0;border-bottom:1px solid var(--line)}
.top{display:flex;gap:10px;align-items:center}
.top .name{flex:1;min-width:0;font-weight:700;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.tag{font:700 12px var(--sans);padding:2px 8px;color:var(--bg);background:var(--ok)}
.tag.revoked{background:var(--bad)}
.tag.expired{background:var(--warn)}
.facts{margin-top:4px;color:var(--mute);font:13px var(--mono)}
.pcs{margin-top:6px;color:var(--mute);font-size:13px}
.empty{padding:18px 0;color:var(--mute)}
[hidden]{display:none!important}
</style>
</head>
<body>
<main>
<header><b>backhaul</b><span>ライセンス</span><button class="small" id="leave" hidden>鍵を忘れる</button></header>

<form id="door" autocomplete="on">
<h2>管理用の合言葉</h2>
<p>Cloudflare に入れた ADMIN_TOKEN です。この端末のパスワード保存に入れておくと、次から自動で入ります。</p>
<input type="text" name="username" value="backhaul-admin" autocomplete="username" hidden>
<div class="row">
<input id="token" name="password" type="password" autocomplete="current-password" aria-label="管理用の合言葉" required minlength="20">
<button class="strong" type="submit">開く</button>
</div>
<div class="note bad" id="doorNote" role="status"></div>
</form>

<section id="inside" hidden>
<h2>新しいキーを出す</h2>
<form id="issue">
<label for="note">だれ向けか（自分用のメモ。相手には見えません）</label>
<input id="note" maxlength="200" autocomplete="off">
<div class="row">
<div><label for="seats">同時に使えるPCの台数</label><input id="seats" type="number" inputmode="numeric" min="1" max="100" value="5" required></div>
<div><label id="daysLabel">期限</label>
<div class="choices" role="group" aria-labelledby="daysLabel">
<button type="button" data-days="30" aria-pressed="false">30日</button>
<button type="button" data-days="365" aria-pressed="true">1年</button>
<button type="button" data-days="" aria-pressed="false">なし</button>
</div></div>
</div>
<div class="row" style="margin-top:14px"><span></span><button class="strong" type="submit" id="issueButton">キーを出す</button></div>
<div class="note" id="issueNote" role="status"></div>
</form>
<div class="issued" id="issued" hidden>
<b id="issuedNote"></b>
<div class="facts" id="issuedFacts"></div>
<code id="issuedKey"></code>
<div class="row"><p>このキーは今だけ表示されます。どこにも保存されないので、すぐ控えてください。</p><button class="small" id="copy" type="button">コピー</button></div>
</div>

<h2>出したキー</h2>
<div class="note" id="listNote" role="status"></div>
<ul id="list"></ul>
</section>
</main>
<script>
(function(){
"use strict";
var token="";
var days="365";
function $(id){return document.getElementById(id)}
function el(tag,cls,text){var e=document.createElement(tag);if(cls)e.className=cls;if(text!==undefined)e.textContent=text;return e}
function say(id,text,kind){var n=$(id);n.textContent=text||"";n.className="note"+(kind?" "+kind:"")}
function day(seconds){if(seconds===null||seconds===undefined)return "期限なし";var d=new Date(seconds*1000);return d.getFullYear()+"-"+String(d.getMonth()+1).padStart(2,"0")+"-"+String(d.getDate()).padStart(2,"0")}
function reason(status,body){
  if(status===403)return "合言葉が違います";
  if(status===503)return "サーバーに ADMIN_TOKEN がまだ入っていません";
  if(status===400)return "入力を確かめてください";
  if(status===0)return "サーバーに届きません。ネットを確かめてください";
  return "うまくいきませんでした（"+status+(body&&body.error?" "+body.error:"")+"）";
}
function call(method,path,body){
  return fetch(path,{method:method,headers:{"authorization":"Bearer "+token,"content-type":"application/json"},body:body===undefined?undefined:JSON.stringify(body),cache:"no-store"})
    .then(function(r){return r.json().catch(function(){return null}).then(function(j){return {status:r.status,body:j}})})
    .catch(function(){return {status:0,body:null}});
}
function draw(licences){
  var list=$("list");list.textContent="";
  if(licences.length===0){list.appendChild(el("li","empty","まだ1つも出していません"));return}
  licences.slice().reverse().forEach(function(l){
    var li=el("li");
    var top=el("div","top");
    var state=l.problem==="revoked"?"停止中":l.problem==="expired"?"期限切れ":"有効";
    top.appendChild(el("span","tag"+(l.problem?" "+l.problem:""),state));
    top.appendChild(el("span","name",l.note||"（メモなし）"));
    var b=el("button","small"+(l.revoked?"":" danger"),l.revoked?"戻す":"止める");
    b.type="button";
    b.addEventListener("click",function(){
      // Stopping asks twice, on the button itself (no pop-up).
      if(!l.revoked&&b.dataset.sure!=="1"){b.dataset.sure="1";b.textContent="本当に止める";setTimeout(function(){b.dataset.sure="";b.textContent="止める"},4000);return}
      b.disabled=true;
      call("POST","/admin/licences/"+l.id+"/"+(l.revoked?"restore":"revoke")).then(function(r){
        if(r.status!==200){say("listNote",reason(r.status,r.body),"bad");b.disabled=false;return}
        load();
      });
    });
    top.appendChild(b);
    li.appendChild(top);
    li.appendChild(el("div","facts","#"+l.id+"  …"+l.keyTail+"  PC "+l.devices.length+"/"+l.seats+"  "+(l.expiresAt===null?"期限なし":day(l.expiresAt)+" まで")));
    if(l.devices.length>0)li.appendChild(el("div","pcs",l.devices.map(function(d){return d.name+"（最後 "+day(d.lastSeenAt)+"）"}).join("、")));
    list.appendChild(li);
  });
}
function load(){
  return call("GET","/admin/licences").then(function(r){
    if(r.status!==200||!r.body||!Array.isArray(r.body.licences)){return r}
    say("listNote","");draw(r.body.licences);return r;
  });
}
$("door").addEventListener("submit",function(e){
  e.preventDefault();
  token=$("token").value.trim();
  say("doorNote","確かめています…");
  load().then(function(r){
    if(r.status!==200){token="";say("doorNote",reason(r.status,r.body),"bad");return}
    say("doorNote","");$("door").hidden=true;$("inside").hidden=false;$("leave").hidden=false;
  });
});
$("leave").addEventListener("click",function(){
  token="";$("token").value="";$("list").textContent="";$("issued").hidden=true;$("issuedKey").textContent="";
  $("inside").hidden=true;$("leave").hidden=true;$("door").hidden=false;
});
Array.prototype.forEach.call(document.querySelectorAll(".choices button"),function(b){
  b.addEventListener("click",function(){
    days=b.dataset.days;
    Array.prototype.forEach.call(document.querySelectorAll(".choices button"),function(o){o.setAttribute("aria-pressed",o===b?"true":"false")});
  });
});
$("issue").addEventListener("submit",function(e){
  e.preventDefault();
  var seats=parseInt($("seats").value,10);
  if(!(seats>=1&&seats<=100)){say("issueNote","台数は1〜100で入れてください","bad");return}
  $("issueButton").disabled=true;say("issueNote","出しています…");
  call("POST","/admin/licences",{seats:seats,days:days===""?null:parseInt(days,10),note:$("note").value.trim()}).then(function(r){
    $("issueButton").disabled=false;
    if(r.status!==200||!r.body||typeof r.body.key!=="string"){say("issueNote",reason(r.status,r.body),"bad");return}
    say("issueNote","");
    $("issuedNote").textContent=r.body.note||"（メモなし）";
    $("issuedFacts").textContent="PC "+r.body.seats+"台  "+(r.body.expiresAt===null?"期限なし":day(r.body.expiresAt)+" まで");
    $("issuedKey").textContent=r.body.key;$("issued").hidden=false;$("copy").textContent="コピー";
    $("note").value="";load();
  });
});
$("copy").addEventListener("click",function(){
  var key=$("issuedKey").textContent;
  if(!navigator.clipboard){$("copy").textContent="長押しでコピー";return}
  navigator.clipboard.writeText(key).then(function(){$("copy").textContent="コピーしました"},function(){$("copy").textContent="長押しでコピー"});
});
})();
</script>
</body>
</html>`;
}
