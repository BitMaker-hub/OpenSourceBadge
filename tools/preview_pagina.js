// Monta tools/preview/setup.html: la página de badge.local con datos falsos,
// para verla en el navegador del ordenador sin tener el badge al lado.
//   node tools/preview_pagina.js
const fs = require("fs");
const path = require("path");
const root = path.join(__dirname, "..");

const page = fs.readFileSync(path.join(root, "src/net/setup_page.h"), "utf8").split('R"HTML(')[1].split(')HTML"')[0];
const data = fs.readFileSync(path.join(root, "src/ui/avatars_data.h"), "utf8");

// Cada personaje a SVG, igual que hace el badge (avatars::svg)
function svg(name) {
  const rows = data.split(`AVATAR_${name}[32] = {`)[1].split("};")[0].match(/"(.*)"/g).map(r => r.slice(1, -1));
  const pal = {};
  data.split(`AVATAR_${name}_PALETTE[] = {`)[1].split("};")[0].replace(/\{'(.)', 0x([0-9A-F]{4})\}/g, (_, l, h) => {
    const c = parseInt(h, 16);
    pal[l] = "#" + [(c >> 8) & 0xF8, (c >> 3) & 0xFC, (c << 3) & 0xF8].map(v => v.toString(16).padStart(2, "0")).join("");
  });
  let out = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32' shape-rendering='crispEdges'>";
  rows.forEach((r, j) => { for (let i = 0; i < 32;) { let n = 1; while (i + n < 32 && r[i + n] === r[i]) n++;
    if (r[i] !== " ") out += `<rect x='${i}' y='${j}' width='${n}' height='1' fill='${pal[r[i]]}'/>`; i += n; } });
  return out + "</svg>";
}
const roles = [["DEV", "#38BCF8"], ["MINER", "#F8CC10"], ["PLEB", "#F09018"], ["WHALE", "#E0E4E8"], ["NOSTRICH", "#A088F8"], ["ANON", "#989CA8"]];
const avatars = Object.fromEntries(roles.map(([r]) => [r, "data:image/svg+xml," + encodeURIComponent(svg(r))]));
const mock = `<script>
const AVATARS = ${JSON.stringify(avatars)};
const _fetch = window.fetch;
window.fetch = (url, opt) => {
  if (url === "/api/profile" && (!opt || opt.method !== "POST"))
    return Promise.resolve({json: () => ({number: 42, name: "satoshi.friends", handle: "@satoshi", npub: "", ln: "",
      tagline: "Stay humble, stack sats", role: "DEV", roles: ${JSON.stringify(roles)}})});
  if (url === "/api/profile") return new Promise(r => setTimeout(() => r({ok: true}), 400));
  return _fetch(url, opt);
};
new MutationObserver(() => document.querySelectorAll("img[src^='/avatar.svg']").forEach(i => i.src = AVATARS[i.src.split("r=")[1]]))
  .observe(document.documentElement, {subtree: true, childList: true, attributes: true, attributeFilter: ['src']});
</script>`;
fs.writeFileSync(path.join(__dirname, "preview", "setup.html"), page.replace("<script>", mock + "\n<script>"));
console.log("escrito tools/preview/setup.html");
