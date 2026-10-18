// Las dos páginas del NerdMiner. La lógica (cuándo mina, los avisos) está en
// app/state_machine.cpp; los datos de la red, en net/bitcoin_info.
//
//   1. MINANDO: el logotipo arriba a la izquierda y el hashrate a la derecha,
//      la gráfica, los números del pool y los avisos de "trabajo nuevo" o
//      "share aceptado".
//   2. BITCOIN: el precio, el bloque, la comisión, la dificultad y el hashrate
//      de la red, el tuyo, y una barra vertical con lo que falta para el halving.
#include "screens.h"
#include "screen_parts.h"
#include "display.h"
#include "miner_logo_data.h"
#include "../config.h"
#include "../net/miner.h"

namespace screens {

static const int CX = SCREEN_W / 2;

namespace nerd {
  constexpr uint16_t Yellow = rgb(0xFFE600);   // el amarillo del logotipo del NerdMiner
  constexpr uint16_t YellowDark = rgb(0x5C5310);
}

// Los dos puntitos de abajo: en qué página estamos
static void pageDots(int page) {
  for (int i = 0; i < 2; i++)
    display::roundRect(CX - 9 + i * 12, 304, 6, 6, i == page ? nerd::Yellow : ink::Line);
}

// La fila de arriba de las dos páginas: la cabecera (con el estado o el nombre
// de la página como título), el logotipo del NerdMiner a la izquierda (así
// siempre se sabe en qué modo estás) y una raya debajo
static void topRow(const String& title, uint16_t color) {
  header(title, color);
  display::logo(MINER_LOGO, 6, 25);
  display::rect(6, 74, 158, 1, ink::Line);
}

// Un aviso de un momento, abajo
static void toast(const MinerView& v) {
  if (v.toast.isEmpty()) return;
  display::roundRect(14, 266, 142, 24, v.toastColor);
  display::roundRect(16, 268, 138, 20, ink::Card2);
  display::text(v.toast, CX, 270, Font::Body, v.toastColor, Align::Center);
}

// ============================================================ 1. MINANDO

// La gráfica del hashrate: una línea con el área de debajo rellena. La de la
// derecha del todo es la medida de ahora (con un puntito).
static void hashGraph(const MinerView& v, int x0, int y0, int w, int h) {
  float val[24], top = 1;
  for (int i = 0; i < 24; i++) {
    val[i] = v.history[(v.historyNext + i) % 24];
    top = max(top, val[i]);
  }
  top *= 1.15f;                                             // un poco de aire por arriba
  const uint16_t area = display::blend(ink::Bg, nerd::Yellow, 45);
  display::rect(x0, y0 + h, w, 1, ink::Line);               // el suelo
  int prevY = -1, y = y0 + h;
  for (int x = 0; x < w; x++) {
    const float pos = x * 23.0f / (w - 1);                  // entre qué dos medidas cae esta columna
    const int i = min((int)pos, 22);
    const float k = val[i] + (val[i + 1] - val[i]) * (pos - i);
    y = y0 + h - 1 - (int)((h - 2) * k / top);
    display::rect(x0 + x, y, 1, y0 + h - y, area);
    const int from = prevY < 0 ? y : min(prevY, y), to = prevY < 0 ? y : max(prevY, y);
    display::rect(x0 + x, from, 1, to - from + 2, nerd::Yellow);   // la línea, de 2 px
    prevY = y;
  }
  display::roundRect(x0 + w - 3, y - 2, 5, 5, nerd::Yellow);
}

void minerMining(const MinerView& v) {
  const nerdminer::Stats& s = v.stats;

  // El estado va de título en la cabecera
  const char* state = !s.running ? "STARTING" : !v.online ? "NO INTERNET" : s.connected ? "MINING" : "CONNECTING";
  const uint16_t stateColor = !v.online ? ink::Red : s.connected ? ink::Ok : ink::Gold;
  topRow(state, stateColor);

  // A la derecha del logotipo: el hashrate, con su nombre encima y los KH/S al lado
  display::text("HASHRATE", 164, 34, Font::Label, ink::Dim, Align::Right);
  const int unitW = display::textWidth("KH/S", Font::Label);
  display::text(String(s.khs, 1), 164 - unitW - 4, 46, Font::Title, ink::Fg, Align::Right);
  display::text("KH/S", 164, 59, Font::Label, ink::Dim, Align::Right);

  hashGraph(v, 10, 84, 150, 38);

  // Los números: los shares son los que acepta el pool (los mismos que enseña su web)
  const uint32_t min = s.minedMs / 60000;
  const String time = min >= 60 ? String(min / 60) + "h " + String(min % 60) + "m" : String(min) + " min";
  display::roundRect(6, 134, 158, 120, ink::Card);
  display::text("Shares", 14, 138, Font::Body, ink::Dim);
  display::text(String(s.accepted), 156, 138, Font::Body, ink::Fg, Align::Right);
  display::text("Best diff", 14, 156, Font::Body, ink::Dim);
  display::text(shortNumber(s.bestDiff), 156, 156, Font::Body, ink::Gold, Align::Right);
  display::text("Pool diff", 14, 174, Font::Body, ink::Dim);
  display::text(s.poolDiff > 0 ? shortNumber(s.poolDiff) : String("--"), 156, 174, Font::Body, ink::Fg, Align::Right);
  display::text("Time", 14, 192, Font::Body, ink::Dim);
  display::text(time, 156, 192, Font::Body, ink::Fg, Align::Right);
  display::text("Blocks", 14, 210, Font::Body, s.blocks ? ink::Ok : ink::Dim);
  display::text(String(s.blocks), 156, 210, Font::Body, s.blocks ? ink::Ok : ink::Fg, Align::Right);
  display::text(display::fit(miner::pool(), 146, Font::Label), CX, 234, Font::Label, ink::Dim, Align::Center);

  toast(v);
  pageDots(0);
}

// ============================================================ 2. BITCOIN

// Una fila de datos: la etiqueta pequeña y el valor debajo
static void datum(int y, const char* label, const String& value, uint16_t color = ink::Fg) {
  display::text(label, 10, y, Font::Label, ink::Dim);
  display::text(value, 10, y + 10, Font::Body, color);
}

void minerBitcoin(const MinerView& v, const btcinfo::Info& info) {
  topRow("BITCOIN", ink::Orange);

  if (!info.valid) {
    centeredLines(v.online ? "Asking mempool.space..." : "No internet: set a WiFi in badge.local.", 130, 150, 3, 16, ink::Soft);
    toast(v);
    pageDots(1);
    return;
  }

  // El precio, a la derecha del logotipo (si no cabe en grande, en la letra normal)
  const String price = "$" + thousands(info.priceUsd);
  const Font priceFont = display::textWidth(price, Font::Title) <= 94 ? Font::Title : Font::Body;
  display::text("BTC / USD", 164, 34, Font::Label, ink::Dim, Align::Right);
  display::text(price, 164, priceFont == Font::Title ? 46 : 52, priceFont, ink::Orange, Align::Right);

  // Los datos, a la izquierda
  datum(82,  "BLOCK", thousands(info.height));
  const float f = info.feeFast;                             // "2" o "0.6": hay comisiones de menos de 1
  datum(110, "NEXT BLOCK FEE", (f == (int)f ? String((int)f) : String(f, 1)) + " sat/vB");
  datum(138, "DIFFICULTY", shortNumber(info.difficulty));
  String net = shortNumber(info.hashrate);                 // "1.02Z" -> "1.02 ZH/s"
  if (isAlpha(net[net.length() - 1])) net = net.substring(0, net.length() - 1) + " " + net[net.length() - 1];
  datum(166, "NETWORK", net + "H/s");
  datum(194, "YOUR BADGE", String(v.stats.khs, 1) + " kH/s", nerd::Yellow);

  // El halving, a la derecha: una barra vertical que se llena de abajo arriba
  const float done = btcinfo::halvingProgress(info.height);
  const int bx = 136, by = 94, bw = 24, bh = 120;
  display::text(String((int)(done * 100)) + "%", bx + bw / 2, 82, Font::Label, ink::Orange, Align::Center);
  display::roundRect(bx, by, bw, bh, ink::Line);
  display::roundRect(bx + 2, by + 2, bw - 4, bh - 4, ink::Card);
  const int fill = (int)((bh - 6) * done);
  display::rect(bx + 3, by + bh - 3 - fill, bw - 6, fill, ink::Orange);

  // Cuánto falta, a 10 minutos por bloque
  const uint32_t blocks = btcinfo::blocksToHalving(info.height);
  const uint32_t days = blocks * 10 / 1440;
  display::text("Halving: " + thousands(blocks) + " blocks", CX, 228, Font::Body, ink::Soft, Align::Center);
  display::text(days > 0 ? "about " + String(days) + " days" : String("any moment now!"), CX, 244, Font::Body, ink::Soft, Align::Center);

  toast(v);
  pageDots(1);
}

}  // namespace screens
