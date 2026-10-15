// Saca los personajes del prototipo de pantallas (pantallas_v2.html) y los
// escribe como dibujos ASCII en src/ui/avatars_data.h.
//
//   node tools/exportar_sprites.js ../PCBs/BTCAmsterdamBadge/firmware-diseno/pantallas_v2.html
//
// Cada personaje queda como 32 líneas de 32 letras. Cada letra es un color de
// su paleta; el espacio es transparente y '#' es el contorno.
const fs = require("fs");
const path = require("path");

const html = fs.readFileSync(process.argv[2], "utf8");
const start = html.indexOf('const OUT="#07080c";');
const end = html.indexOf("const ROLES=");
let code = html.slice(start, end);
// En el prototipo paint() devuelve un canvas; aquí queremos los píxeles.
code = code.replace(/const c=document\.createElement[\s\S]*?return c;/, "return out;");
const DRAW_ROLE = new Function(code + "; return {paint, DRAW_ROLE};")();

const ORDER = ["dev", "miner", "pleb", "whale", "nostrich", "anon"];
const LETTERS = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

function rgb565(hex) {
  const v = parseInt(hex.slice(1), 16);
  const r = (v >> 16) & 255, g = (v >> 8) & 255, b = v & 255;
  return "0x" + (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)).toString(16).toUpperCase().padStart(4, "0");
}

let out = `#pragma once
// GENERADO por tools/exportar_sprites.js a partir del prototipo de pantallas.
// No se edita a mano: se cambia el prototipo y se vuelve a exportar.
//
// Cada personaje mide 32 x 32. ' ' = transparente, '#' = contorno, y cada
// letra es un color de la paleta que va justo debajo del dibujo.
#include <stdint.h>

struct PaletteEntry { char letter; uint16_t color; };

`;
for (const role of ORDER) {
  const px = DRAW_ROLE.paint(DRAW_ROLE.DRAW_ROLE[role]);
  const pal = new Map();
  pal.set("#07080c", "#");
  const rows = [];
  for (let y = 0; y < 32; y++) {
    let s = "";
    for (let x = 0; x < 32; x++) {
      const c = px[y * 32 + x];
      if (!c) { s += " "; continue; }
      if (!pal.has(c)) pal.set(c, LETTERS[pal.size - 1]);
      s += pal.get(c);
    }
    rows.push(s);
  }
  const name = role.toUpperCase();
  out += `static const char* const AVATAR_${name}[32] = {\n`;
  out += rows.map(r => `  "${r}",`).join("\n") + "\n};\n";
  out += `static const PaletteEntry AVATAR_${name}_PALETTE[] = {\n`;
  out += [...pal].map(([hex, l]) => `  {'${l}', ${rgb565(hex)}},   // ${hex}`).join("\n") + "\n  {0, 0}\n};\n\n";
}
const dest = path.join(__dirname, "..", "src", "ui", "avatars_data.h");
fs.mkdirSync(path.dirname(dest), { recursive: true });
fs.writeFileSync(dest, out);
console.log("escrito", dest);
