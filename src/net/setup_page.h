#pragma once
// La página de badge.local. Va entera aquí (HTML, estilo y JavaScript) porque
// el badge la sirve tal cual, sin internet. Los datos los pide a /api/profile.
#include <Arduino.h>

static const char SETUP_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Open Source Badge</title>
<style>
  :root{--bg:#0c0e13;--card:#151922;--line:#262c3a;--fg:#eef0f4;--dim:#8a93a6;--or:#f7931a;--gold:#ffc93c}
  *{box-sizing:border-box}
  body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.4 system-ui,-apple-system,sans-serif}
  main{max-width:460px;margin:0 auto;padding:20px 16px 40px}
  h1{font-size:22px;margin:0 0 2px;color:var(--or)}
  .sub{color:var(--dim);margin:0 0 20px}
  h2{font-family:ui-monospace,"Cascadia Mono",Menlo,Consolas,monospace;font-size:13px;letter-spacing:.08em;text-transform:uppercase;color:var(--dim);margin:24px 0 8px}
  .roles{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
  .role{background:var(--card);border:2px solid var(--line);border-radius:10px;padding:8px 4px 6px;color:var(--fg);
        font:600 12px system-ui;display:flex;flex-direction:column;align-items:center;gap:4px;cursor:pointer}
  .role img{width:64px;height:64px;image-rendering:pixelated}
  .role[aria-pressed=true]{border-color:var(--c);box-shadow:0 0 0 2px var(--c) inset}
  label{display:block;margin:12px 0 4px;font-size:14px;color:var(--dim)}
  input{width:100%;padding:12px;border-radius:8px;border:1px solid var(--line);background:var(--card);color:var(--fg);font-size:16px}
  input:focus{outline:2px solid var(--or);border-color:transparent}
  .hint{font-size:12px;color:var(--dim);margin-top:4px}
  p.hint{font-size:13px;margin:0 0 4px}
  button.save{margin-top:24px;width:100%;padding:14px;border:0;border-radius:10px;background:var(--or);color:#0c0e13;font:700 17px system-ui}
  #msg{text-align:center;margin-top:14px;min-height:22px}
  .px{font-family:ui-monospace,"Cascadia Mono",Menlo,Consolas,monospace;letter-spacing:.04em}
  /* La vista previa imita la pantalla del badge: tarjeta con trama, marco del color del rol */
  .badge{margin:0 auto 8px;width:200px;background:var(--bg);border:3px solid #2a2f3b;border-radius:14px;padding:12px 10px 14px;text-align:center;
         box-shadow:0 0 0 6px #111318,0 10px 30px #0008}
  .badge .top{display:flex;justify-content:space-between;font-size:11px;color:var(--dim);border-bottom:1px solid var(--line);padding-bottom:6px;margin-bottom:12px}
  .badge .top b{color:var(--or);font-weight:600}
  .frame{width:112px;height:112px;margin:0 auto;border-radius:3px;background:var(--c,#9ca3af);padding:4px}
  .frame div{width:100%;height:100%;background:#1b2030 radial-gradient(#20263a 1px,transparent 1.5px) 0 0/4px 4px;display:grid;place-items:center}
  .frame img{width:96px;height:96px;image-rendering:pixelated}
  #pName{font-size:20px;font-weight:700;margin:10px 0 0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
  #pHandle{color:var(--dim);font-size:14px;min-height:20px}
  .chip{display:inline-block;margin-top:6px;background:var(--c,#9ca3af);color:var(--bg);font-size:10px;font-weight:700;padding:2px 6px;border-radius:2px}
  #pLine{color:var(--dim);font-size:13px;margin-top:10px;min-height:18px}
  .ok{color:#57d38c}.err{color:#f87171}
  /* Contactos */
  .peer{background:var(--card);border:1px solid var(--line);border-left:4px solid var(--c);border-radius:8px;padding:10px 12px;margin:8px 0}
  .peer b{font-size:16px}
  .peer .meta{font-size:12px;color:var(--dim);font-family:ui-monospace,"Cascadia Mono",Menlo,Consolas,monospace}
  .peer .row{font-size:13px;margin-top:3px;overflow-wrap:anywhere}
  .peer .row span{color:var(--dim)}
  .peer a{color:var(--or)}
  .dl{display:flex;gap:8px;margin-top:10px}
  .dl a{flex:1;text-align:center;padding:11px;border-radius:10px;border:1px solid var(--or);color:var(--or);text-decoration:none;font-weight:600}
  #empty{color:var(--dim);font-size:14px}
  /* Las WiFi que ve el badge */
  .nets{display:flex;flex-direction:column;gap:6px;margin-top:8px}
  .net{display:flex;align-items:center;gap:10px;width:100%;padding:11px 12px;border-radius:8px;border:1px solid var(--line);
       background:var(--card);color:var(--fg);font:15px system-ui;text-align:left;cursor:pointer}
  .net[aria-pressed=true]{border-color:var(--or);box-shadow:0 0 0 1px var(--or) inset}
  .net .name{flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
  .net .meta{color:var(--dim);font-size:12px;font-family:ui-monospace,"Cascadia Mono",Menlo,Consolas,monospace}
  .bars{display:inline-flex;align-items:flex-end;gap:2px;height:14px}
  .bars i{width:3px;background:var(--line);border-radius:1px}
  .bars i.on{background:var(--fg)}
  .rescan{margin-top:8px;background:none;border:0;color:var(--or);font:600 14px system-ui;padding:4px 0;cursor:pointer}
</style></head>
<body><main>
  <h1>Open Source Badge</h1>
  <p class="sub">Proof of Peer &middot; Bitcoin Amsterdam 2026</p>

  <div class="badge" id="preview" aria-label="How your badge will look">
    <div class="top px"><b id="num">BADGE</b><span>&#9889; 100%</span></div>
    <div class="frame"><div><img id="pImg" alt=""></div></div>
    <div id="pName"></div>
    <div id="pHandle"></div>
    <span class="chip px" id="pRole"></span>
    <div id="pLine"></div>
  </div>

  <h2>Pick your character</h2>
  <div class="roles" id="roles"></div>

  <h2>About you</h2>
  <label for="name">Name on the badge</label>
  <input id="name" maxlength="20" autocomplete="nickname" required>
  <label for="handle">X (Twitter)</label>
  <input id="handle" maxlength="20" placeholder="@satoshi" autocapitalize="off">
  <label for="npub">Nostr public key</label>
  <input id="npub" maxlength="63" placeholder="npub1..." autocapitalize="off" spellcheck="false">
  <label for="ln">Lightning address</label>
  <input id="ln" maxlength="60" placeholder="you@wallet.com" autocapitalize="off" spellcheck="false">
  <div class="hint" id="lnhint">People can zap you by scanning the QR on your badge.</div>
  <label for="tagline">Your line</label>
  <input id="tagline" maxlength="40" placeholder="Stay humble, stack sats">
  <label for="telegram">Telegram</label>
  <input id="telegram" maxlength="40" placeholder="@satoshi" autocapitalize="off" spellcheck="false">
  <label for="pgp">PGP fingerprint</label>
  <input id="pgp" maxlength="60" placeholder="40 hex digits, spaces are fine" autocapitalize="characters" spellcheck="false">
  <div class="hint">Whoever you handshake with gets all of this, so they can check your PGP key later.</div>

  <h2>Catch your zaps</h2>
  <p class="hint">Your badge lights up when someone zaps your npub on Nostr. It needs internet for that:
    your phone hotspot or the venue WiFi. The sats go to your own wallet; the badge only listens.</p>
  <label>WiFi networks your badge can see</label>
  <div class="nets" id="nets"><div class="hint">Looking for networks...</div></div>
  <button type="button" class="rescan" id="rescan">Search again</button>
  <label for="wifi">WiFi name</label>
  <input id="wifi" maxlength="32" autocapitalize="off" spellcheck="false" placeholder="Tap one above or type it">
  <label for="wifipass">WiFi password</label>
  <input id="wifipass" type="password" maxlength="63" autocomplete="off">

  <h2>Mine Bitcoin</h2>
  <p class="hint">Your badge solo-mines like a NerdMiner (LAB menu) on the Bitronics pool.</p>
  <label for="maddr">Your Bitcoin address</label>
  <input id="maddr" maxlength="90" autocapitalize="off" spellcheck="false">
  <div class="hint">The grey one is the Bitronics address your badge mines to by default. Put yours here
    and, if your badge ever finds a block, the reward is all yours.</div>
  <label for="mpool">Pool</label>
  <input id="mpool" maxlength="80" placeholder="pool.bitronics.store:3334" autocapitalize="off" spellcheck="false">

  <button class="save" id="save">Save to my badge</button>
  <div id="msg" role="status"></div>

  <h2>People you met</h2>
  <p class="hint">Everyone you did a handshake with, and what they shared. Download them before you leave.</p>
  <div id="peers"><div id="empty">Loading...</div></div>
  <div class="dl" id="dl" hidden>
    <a href="/contacts.vcf" download>Contacts (vCard)</a>
    <a href="/contacts.csv" download>Spreadsheet (CSV)</a>
  </div>
</main>
<script>
const $ = id => document.getElementById(id);
let role = "ANON";

let colors = {};

function pick(r) {
  role = r;
  document.querySelectorAll(".role").forEach(b => b.setAttribute("aria-pressed", b.dataset.role === r));
  $("preview").style.setProperty("--c", colors[r]);
  $("pImg").src = "/avatar.svg?r=" + r;
  $("pRole").textContent = r;
}

// La vista previa sigue a lo que escribes
function refresh() {
  $("pName").textContent = $("name").value || "Your name";
  const h = $("handle").value.replace(/[@ ]/g, "");
  $("pHandle").textContent = h ? "@" + h : "";
  $("pLine").textContent = $("tagline").value ? "“" + $("tagline").value + "”" : "";
}

fetch("/api/profile").then(r => r.json()).then(p => {
  $("num").textContent = "BADGE #" + String(p.number).padStart(3, "0");
  p.roles.forEach(([r, color]) => {
    colors[r] = color;
    const b = document.createElement("button");
    b.type = "button"; b.className = "role"; b.dataset.role = r; b.style.setProperty("--c", color);
    b.innerHTML = '<img alt="" src="/avatar.svg?r=' + r + '">' + r;
    b.onclick = () => pick(r);
    $("roles").append(b);
  });
  ["name", "handle", "npub", "ln", "tagline", "telegram", "pgp"].forEach(k => { $(k).value = p[k] || ""; $(k).oninput = refresh; });
  $("wifi").value = p.wifi || "";
  $("maddr").value = p.maddr || "";
  $("maddr").placeholder = p.maddrDefault || "";
  $("mpool").value = p.mpool || "";
  if (p.wallet) {
    $("ln").placeholder = p.wallet;
    $("lnhint").textContent = "Leave it empty to use your badge wallet (" + p.wallet + "). It lights up with every payment.";
  }
  $("wifipass").placeholder = p.wifiSaved ? "Saved (leave empty to keep it)" : "";
  pick(p.role);
  refresh();
});

// Las redes que ve el badge, de la más fuerte a la más débil. Al tocar una se
// rellena el nombre y se pasa a la contraseña (si la pide). La búsqueda tarda
// unos segundos, así que se pregunta varias veces.
function bars(rssi) {
  const n = rssi > -55 ? 4 : rssi > -67 ? 3 : rssi > -78 ? 2 : 1;
  return '<span class="bars">' + [4, 7, 10, 14].map((h, i) => '<i style="height:' + h + 'px"' + (i < n ? ' class="on"' : "") + "></i>").join("") + "</span>";
}
function showNetworks(list) {
  list.sort((a, b) => b.r - a.r);
  $("nets").innerHTML = "";
  list.slice(0, 8).forEach(n => {
    const b = document.createElement("button");
    b.type = "button"; b.className = "net";
    b.setAttribute("aria-pressed", n.s === $("wifi").value);
    b.innerHTML = bars(n.r) + '<span class="name"></span><span class="meta">' + (n.o ? "open" : "&#128274;") + "</span>";
    b.querySelector(".name").textContent = n.s;
    b.onclick = () => {
      $("wifi").value = n.s;
      document.querySelectorAll(".net").forEach(x => x.setAttribute("aria-pressed", x === b));
      if (!n.o) $("wifipass").focus();
    };
    $("nets").append(b);
  });
}
function loadNetworks(tries, rescan) {
  fetch("/api/networks" + (rescan ? "?rescan=1" : "")).then(r => r.json()).then(list => {
    if (!list.length && tries > 0) return setTimeout(() => loadNetworks(tries - 1), 2000);
    if (!list.length) { $("nets").innerHTML = '<div class="hint">No networks found. Type the name below.</div>'; return; }
    showNetworks(list);
  }).catch(() => {});
}
loadNetworks(6);
$("rescan").onclick = () => {
  $("nets").innerHTML = '<div class="hint">Looking for networks...</div>';
  fetch("/api/networks?rescan=1").catch(() => {});
  setTimeout(() => loadNetworks(5), 3000);
};

$("save").onclick = () => {
  const npub = $("npub").value.trim();
  const pgp = $("pgp").value.replace(/[^0-9a-f]/gi, "");
  if (pgp && pgp.length !== 40) { show("A PGP fingerprint has 40 hex digits", "err"); return; }
  if (npub && !npub.startsWith("npub1")) { show("That Nostr key should start with npub1", "err"); return; }
  const body = new URLSearchParams({
    role, name: $("name").value, handle: $("handle").value, npub, ln: $("ln").value, tagline: $("tagline").value,
    telegram: $("telegram").value, pgp: $("pgp").value,
    wifi: $("wifi").value, wifipass: $("wifipass").value,
    maddr: $("maddr").value, mpool: $("mpool").value,
    now: Math.floor(Date.now() / 1000), tz: -new Date().getTimezoneOffset()
  });
  fetch("/api/profile", {method: "POST", body}).then(r => r.ok ? show("Saved! Look at your badge.", "ok")
                                                         : show("Could not save, try again.", "err"))
    .catch(() => show("Could not save, try again.", "err"));
};
function show(t, c) { $("msg").textContent = t; $("msg").className = c; }

// La gente que has conocido
function esc(t) { const d = document.createElement("div"); d.textContent = t; return d.innerHTML; }
fetch("/api/peers").then(r => r.json()).then(list => {
  if (!list.length) { $("empty").textContent = "Nobody yet. Press PEER next to another badge!"; return; }
  $("peers").innerHTML = list.map(p => {
    const rows = [];
    if (p.x) rows.push('<span>X</span> <a href="https://x.com/' + esc(p.x.replace("@", "")) + '">' + esc(p.x) + "</a>");
    if (p.telegram) rows.push('<span>Telegram</span> <a href="https://t.me/' + esc(p.telegram.replace("@", "")) + '">' + esc(p.telegram) + "</a>");
    if (p.npub) rows.push("<span>Nostr</span> " + esc(p.npub));
    if (p.ln) rows.push("<span>Lightning</span> " + esc(p.ln));
    if (p.pgp) rows.push("<span>PGP</span> " + esc(p.pgp.match(/.{4}/g).join(" ")));
    return '<div class="peer" style="--c:' + p.color + '"><b>' + esc(p.name) + "</b>" +
      '<div class="meta">#' + String(p.number).padStart(3, "0") + " · " + p.role + " · block " + p.block + (p.met ? " · " + p.met : "") + "</div>" +
      rows.map(r => '<div class="row">' + r + "</div>").join("") + "</div>";
  }).join("");
  $("dl").hidden = false;
}).catch(() => { $("empty").textContent = "Could not load them."; });
</script>
</body></html>
)HTML";
