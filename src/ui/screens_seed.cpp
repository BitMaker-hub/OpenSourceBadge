// Pantallas del Seed Lab. La lógica está en app/seed_lab.cpp; aquí solo se dibuja.
//
// Todas comparten el mismo esqueleto:
//   - cabecera con "SEED LAB" y la pastilla roja RADIO OFF
//   - barra de 4 pasos (método, palabras, aleatoriedad, semilla)
//   - abajo, los botones como teclas: [PEER] next  [MODE] go ...
// La moneda y el dado son pixel art, como los personajes.
#include "screens.h"
#include "screen_parts.h"
#include "display.h"
#include "../config.h"
#include "../net/miner.h"
#include "lab_logos_data.h"

namespace screens {

static const int CX = SCREEN_W / 2;

namespace pal {
  constexpr uint16_t Gold      = rgb(0xF5C542);
  constexpr uint16_t GoldLight = rgb(0xFFE38A);
  constexpr uint16_t GoldDark  = rgb(0xB8860B);
  constexpr uint16_t GoldEdge  = rgb(0x7A5600);
  constexpr uint16_t Outline   = rgb(0x07080C);
  constexpr uint16_t Brown     = rgb(0x5A3A00);
  constexpr uint16_t DieFace   = rgb(0xF2F3F6);
  constexpr uint16_t DieShade  = rgb(0xAEB5C2);
  constexpr uint16_t DiePip    = rgb(0x1B1F2A);
  // El Seed Lab va en el verde de la Seeder: de semilla, como su logotipo
  constexpr uint16_t Seed      = rgb(0x5CD68A);
  constexpr uint16_t SeedDark  = rgb(0x2E6B45);
  constexpr uint16_t SelBg     = rgb(0x0F2A1B);
  constexpr uint16_t RadioOff  = rgb(0xE5484D);
}

// ============================================================ piezas comunes

// Cabecera del Seed Lab: título y, a la derecha, la pastilla RADIO OFF
static void seedHeader(const String& title) {
  display::rect(0, 0, SCREEN_W, 17, ink::Card);
  display::rect(0, 17, SCREEN_W, 1, ink::Line);
  display::logo(SEED_SPROUT_MINI, 5, 1);                      // el brote de la Seeder: estás en su modo
  display::text(title, 18, 1, Font::Body, pal::Seed);
  const int w = display::textWidth("RADIO OFF", Font::Label) + 10;
  display::roundRect(SCREEN_W - 6 - w, 3, w, 12, pal::RadioOff);
  display::text("RADIO OFF", SCREEN_W - 6 - w / 2, 5, Font::Label, ink::White, Align::Center);
}

// Los cuatro pasos: el actual en naranja, los hechos en naranja apagado
static void steps(int current, const char* name) {
  static const int N = 4, GAP = 3;
  const int w = (SCREEN_W - 12 - GAP * (N - 1)) / N;
  for (int i = 0; i < N; i++) {
    const uint16_t c = i + 1 == current ? pal::Seed : i + 1 < current ? pal::SeedDark : ink::Line;
    display::rect(6 + i * (w + GAP), 22, w, 3, c);
  }
  display::text(String("STEP ") + current + "/4 - " + name, 6, 29, Font::Label, ink::Dim);
}

// Una tecla con su acción al lado: [PEER] next. Devuelve el ancho.
static int keyWidth(const char* key, const char* action) {
  return display::textWidth(key, Font::Label) + 8 + 4 + display::textWidth(action, Font::Label);
}

static void key(int x, int y, const char* keyName, const char* action) {
  const int kw = display::textWidth(keyName, Font::Label) + 8;
  display::roundRect(x, y, kw, 12, ink::Soft);
  display::rect(x + 1, y + 10, kw - 2, 2, ink::Dim);                  // el canto de la tecla
  display::text(keyName, x + kw / 2, y + 2, Font::Label, ink::Bg, Align::Center);
  display::text(action, x + kw + 4, y + 2, Font::Label, ink::Soft);
}

// Las teclas centradas. Si no caben en una fila, la última baja a una segunda fila.
static void keyRow(int y, const char* const* ks, const char* const* as, int n) {
  int total = -8;
  for (int i = 0; i < n; i++) total += keyWidth(ks[i], as[i]) + 8;
  int x = CX - total / 2;
  for (int i = 0; i < n; i++) {
    key(x, y, ks[i], as[i]);
    x += keyWidth(ks[i], as[i]) + 8;
  }
}

static void keys(int y, const char* k1, const char* a1, const char* k2 = nullptr, const char* a2 = nullptr,
                 const char* k3 = nullptr, const char* a3 = nullptr) {
  const char* ks[] = {k1, k2, k3};
  const char* as[] = {a1, a2, a3};
  int n = 0, total = -8;
  while (n < 3 && ks[n]) { total += keyWidth(ks[n], as[n]) + 8; n++; }
  if (total <= SCREEN_W - 8 || n == 1) {
    keyRow(y, ks, as, n);
  } else {
    keyRow(y - 16, ks, as, n - 1);
    keyRow(y, ks + n - 1, as + n - 1, 1);
  }
}

// Una tarjeta de opción: con borde naranja si está elegida
static void optionCard(int x, int y, int w, int h, bool selected) {
  display::roundRect(x, y, w, h, selected ? pal::Seed : ink::Line);
  display::roundRect(x + 2, y + 2, w - 4, h - 4, selected ? pal::SelBg : ink::Card);
}

// ============================================================ pixel art

// Una "celda" del pixel art
static void cell(int x0, int y0, int s, int i, int j, uint16_t c) { display::rect(x0 + i * s, y0 + j * s, s, s, c); }

// Moneda de 17 x 17 celdas. Cara (1) con la ₿; cruz (0) con un 0.
static void pixelCoin(int cx, int y, int s, bool heads) {
  static const char* const B_GLYPH[] = {
    ".#.#..",
    "#####.",
    ".#..##",
    ".#..##",
    ".####.",
    ".#..##",
    ".#..##",
    "#####.",
    ".#.#..",
  };
  static const char* const ZERO_GLYPH[] = {
    ".###.",
    "##.##",
    "##.##",
    "##.##",
    "##.##",
    "##.##",
    ".###.",
  };
  const int N = 17, x0 = cx - N * s / 2;
  for (int j = 0; j < N; j++) {
    for (int i = 0; i < N; i++) {
      const float dx = i - 8, dy = j - 8, d = sqrtf(dx * dx + dy * dy);
      if (d > 8.4f) continue;
      uint16_t c;
      if (d > 7.5f)      c = pal::Outline;                                   // contorno
      else if (d > 6.3f) c = dx + dy < 0 ? pal::GoldLight : pal::GoldEdge;   // canto: luz arriba a la izquierda
      else if (d > 5.4f) c = pal::GoldDark;                                  // surco del canto
      else               c = dx + dy < -5 ? pal::GoldLight : pal::Gold;      // cara, con un brillo
      cell(x0, y, s, i, j, c);
    }
  }
  // El dibujo del centro
  if (heads) {
    for (int j = 0; j < 9; j++)
      for (int i = 0; B_GLYPH[j][i]; i++)
        if (B_GLYPH[j][i] == '#') cell(x0, y, s, 6 + i, 4 + j, pal::Brown);
  } else {
    for (int j = 0; j < 7; j++)
      for (int i = 0; ZERO_GLYPH[j][i]; i++)
        if (ZERO_GLYPH[j][i] == '#') cell(x0, y, s, 6 + i, 5 + j, pal::Brown);
  }
}

// Dado de 16 x 16 celdas, con luz arriba y sombra a la derecha y abajo
static void pixelDie(int x0, int y, int s, uint8_t value) {
  static const uint16_t FACE[6] = {0x010, 0x101, 0x111, 0x145, 0x155, 0x16D};   // 3x3: qué puntos van
  const int N = 16;
  for (int j = 0; j < N; j++) {
    for (int i = 0; i < N; i++) {
      const bool corner = (i < 2 || i > 13) && (j < 2 || j > 13) && !((i == 1 || i == 14) && (j == 1 || j == 14));
      if ((i == 0 || i == 15) && (j == 0 || j == 15)) continue;          // esquinas redondeadas
      if (corner) { cell(x0, y, s, i, j, pal::Outline); continue; }
      uint16_t c = pal::DieFace;
      if (i == 0 || j == 0 || i == 15 || j == 15) c = pal::Outline;
      else if (i >= 13 || j >= 13) c = pal::DieShade;                   // sombra
      cell(x0, y, s, i, j, c);
    }
  }
  if (value < 1 || value > 6) return;
  const uint16_t face = FACE[value - 1];
  static const int POS[3] = {3, 7, 10};
  for (int k = 0; k < 9; k++)
    if (face & (1 << k))
      for (int dj = 0; dj < 2; dj++)
        for (int di = 0; di < 2; di++) cell(x0, y, s, POS[k % 3] + di, POS[k / 3] + dj, pal::DiePip);
}

// ============================================================ LAB (el menú de SEED)

// Cada opción lleva su logotipo y su color: el verde de la Seeder (de semilla)
// y el amarillo del NerdMiner. El logotipo ya dice qué es; debajo, una línea.
namespace lablogo { constexpr uint16_t Seeder = rgb(0x5CD68A), Nerd = rgb(0xFFE600); }

static void labCard(int y, bool selected, uint16_t accent) {
  display::roundRect(6, y, 158, 104, selected ? accent : ink::Line);
  display::roundRect(8, y + 2, 154, 100, selected ? display::blend(ink::Card, accent, 28) : ink::Card);
}

void lab(int selected, bool mining) {
  header("LAB", ink::Orange);
  display::text("What do you want to try?", CX, 26, Font::Body, ink::Soft, Align::Center);

  // Seeder: el brote a la izquierda, la palabra SEEDER a la derecha
  int y = 50;
  labCard(y, selected == 0, lablogo::Seeder);
  display::logo(LAB_SPROUT, 30, y + 24);
  display::logo(LAB_SEEDER_WORD, 108 - LAB_SEEDER_WORD.width / 2, y + 36);
  display::text("MAKE A SEED", 108, y + 60, Font::Label, selected == 0 ? lablogo::Seeder : ink::Dim, Align::Center);

  // NerdMiner: su logotipo tal cual, y si está minando
  y = 162;
  labCard(y, selected == 1, lablogo::Nerd);
  display::logo(LAB_NERDMINER, CX - LAB_NERDMINER.width / 2, y + 12);
  display::text(mining ? "MINING NOW" : "SOLO MINING", CX, y + 84, Font::Label,
                mining ? ink::Ok : (selected == 1 ? lablogo::Nerd : ink::Dim), Align::Center);

  keys(300, "PEER", "next", "MODE", "go", "SEED", "exit");
}

// ============================================================ paso 1: método

void seedMenu(bool coinSelected) {
  seedHeader("SEED LAB");
  steps(1, "METHOD");

  // Dos tarjetas grandes, una al lado de la otra
  for (int i = 0; i < 2; i++) {
    const bool coin = i == 1, on = coin == coinSelected;
    const int x = 6 + i * 82, w = 76, cx = x + w / 2;
    optionCard(x, 42, w, 132, on);
    // Dibujos a escala 3 (no 4) para que no toquen el borde de la tarjeta
    if (coin) pixelCoin(cx, 56, 3, true);
    else      pixelDie(cx - 24, 57, 3, 5);
    display::text(coin ? "Coin" : "Dice", cx, 116, Font::Title, on ? pal::Seed : ink::Fg, Align::Center);
    display::text(coin ? "128 OR 256" : "50 OR 99", cx, 146, Font::Label, ink::Dim, Align::Center);
    display::text(coin ? "FLIPS" : "ROLLS", cx, 156, Font::Label, ink::Dim, Align::Center);
  }

  centeredLines("You make the randomness. The badge turns it into a Bitcoin seed.", 186, 154, 4, 16, ink::Soft);
  keys(300, "PEER", "next", "MODE", "go", "SEED", "exit");
}

// ============================================================ paso 2: palabras

void seedWords(uint8_t nWords, bool coin) {
  seedHeader("SEED LAB");
  steps(2, "WORDS");
  display::text("How many words?", CX, 44, Font::Body, ink::Soft, Align::Center);

  for (int i = 0; i < 2; i++) {
    const bool twelve = i == 0, on = twelve == (nWords == 12);
    const int x = 6 + i * 82, w = 76, cx = x + w / 2;
    optionCard(x, 66, w, 90, on);
    display::text(twelve ? "12" : "24", cx, 76, Font::Title, on ? pal::Seed : ink::Fg, Align::Center);
    display::text("WORDS", cx, 106, Font::Label, ink::Dim, Align::Center);
    display::text(twelve ? "128 BITS" : "256 BITS", cx, 128, Font::Label, on ? ink::Gold : ink::Dim, Align::Center);
    display::text(coin ? (twelve ? "128 FLIPS" : "256 FLIPS") : (twelve ? "50 ROLLS" : "99 ROLLS"),
                  cx, 140, Font::Label, ink::Dim, Align::Center);
  }

  centeredLines(coin ? "Every flip is one bit: heads 1, tails 0." : "Every roll gives about 2.6 bits.",
                170, 154, 2, 16, ink::Soft);
  centeredLines("12 words is already plenty. 24 is for the extra careful.", 212, 154, 3, 16, ink::Dim);
  keys(300, "PEER", "change", "MODE", "start", "SEED", "back");
}

// ============================================================ paso 3: aleatoriedad

void seedCoin(uint16_t done, uint16_t total, const uint8_t* entropy) {
  seedHeader("FLIP A COIN");
  steps(3, "RANDOMNESS");

  // La moneda enseña la última tirada (cara si aún no hay ninguna)
  const bool lastHeads = done == 0 || ((entropy[(done - 1) / 8] >> (7 - (done - 1) % 8)) & 1);
  pixelCoin(CX, 48, 3, lastHeads);
  keys(126, "PEER", "heads = 1", "MODE", "tails = 0");     // no caben en una fila: salen en dos (110 y 126)
  display::text(String(done) + " / " + total, CX, 144, Font::Title, ink::Gold, Align::Center);

  // Todos los bits: naranja = 1, gris = 0, oscuro = por tirar. Un hueco cada 8 (un byte).
  for (int i = 0; i < (int)total; i++) {
    const int col = i % 16, row = i / 16;
    const int x = 19 + col * 8 + (col >= 8 ? 2 : 0), y = 178 + row * 7;
    uint16_t c = ink::Line;
    if (i < done) c = (entropy[i / 8] >> (7 - i % 8)) & 1 ? pal::Seed : ink::Soft;
    display::rect(x, y, 6, 5, c);
  }
  keys(300, "SEED", "hold to quit");
}

void seedDice(uint8_t done, uint8_t total, uint8_t value, const uint8_t* lastRolls) {
  seedHeader("ROLL A DIE");
  steps(3, "RANDOMNESS");

  pixelDie(CX - 40, 52, 5, value);
  keys(144, "PEER", "change", "MODE", "save");
  display::text(String(done) + " / " + total, CX, 164, Font::Title, ink::Gold, Align::Center);
  display::rect(14, 198, 142, 5, ink::Line);
  display::rect(14, 198, 142 * done / total, 5, pal::Seed);

  if (done > 0) {
    display::text("LAST ROLLS", CX, 218, Font::Label, ink::Dim, Align::Center);
    for (int i = 0; i < 3; i++) if (lastRolls[i]) pixelDie(CX - 54 + i * 38, 232, 2, lastRolls[i]);
  }
  keys(300, "SEED", "hold to quit");
}

void seedGenerating() {
  seedHeader("SEED LAB");
  steps(4, "SEED");
  pixelCoin(CX, 80, 4, true);
  display::text("Turning your bits", CX, 166, Font::Body, pal::Seed, Align::Center);
  display::text("into words...", CX, 184, Font::Body, pal::Seed, Align::Center);
  display::text("BIP39", CX, 214, Font::Label, ink::Dim, Align::Center);
}

// ============================================================ paso 4: la semilla

static void resultHeader(const char* name, int page, int pages) {
  seedHeader("YOUR SEED");
  steps(4, name);
  // Puntitos de página a la derecha de la barra de pasos
  for (int i = 0; i < pages; i++)
    display::roundRect(SCREEN_W - 6 - (pages - i) * 8, 29, 6, 6, i + 1 == page ? pal::Seed : ink::Line);
}

void seedMnemonic(const String& mnemonic, int from, int nWords, int page, int pages) {
  resultHeader("WORDS", page, pages);

  // Las palabras van separadas por espacios: nos saltamos las `from` primeras
  int start = 0;
  for (int skipped = 0; skipped < from; skipped++) start = mnemonic.indexOf(' ', start) + 1;
  for (int i = 0; i < 12 && from + i < nWords; i++) {
    int end = mnemonic.indexOf(' ', start);
    if (end < 0) end = mnemonic.length();
    const int y = 42 + i * 20;
    if (i % 2 == 0) display::roundRect(6, y - 1, 158, 19, ink::Card);
    display::text(String(from + i + 1), 32, y + 1, Font::Body, ink::Dim, Align::Right);
    display::text(mnemonic.substring(start, end), 42, y + 1, Font::Body, ink::Fg);
    start = end + 1;
  }
  keys(296, "PEER", "next", "MODE", "back");
}

void seedAddress(const String& address, int page, int pages) {
  resultHeader("ADDRESS", page, pages);
  display::text("Your first address", CX, 42, Font::Body, ink::Soft, Align::Center);
  const int side = display::qr(address, CX, 62, 132);
  centeredLines(address, 66 + side, 156, 3, 16, ink::Fg);
  display::text("m/84'/0'/0'/0/0", CX, 270, Font::Label, ink::Dim, Align::Center);
  keys(296, "PEER", "next", "MODE", "back");
}

void seedEntropy(const String& hex, bool fromDice, int page, int pages) {
  resultHeader("ENTROPY", page, pages);
  display::text("The bits you made", CX, 42, Font::Body, ink::Soft, Align::Center);

  // En grupos de 8 letras, dos grupos por línea, dentro de una tarjeta
  const int lines = (hex.length() + 15) / 16;
  display::roundRect(6, 62, 158, lines * 18 + 8, ink::Card);
  for (int line = 0; line < lines; line++) {
    const unsigned i = line * 16;
    String part = hex.substring(i, min<unsigned>(i + 8, hex.length()));
    if (i + 8 < hex.length()) part += " " + hex.substring(i + 8, min<unsigned>(i + 16, hex.length()));
    display::text(part, CX, 66 + line * 18, Font::Body, ink::Gold, Align::Center);
  }
  centeredLines(fromDice ? "Your rolls hashed with SHA-256, like Coldcard. Same rolls, same words."
                         : "Every coin flip is one of these bits. Same flips, same words.",
                220, 154, 4, 16, ink::Dim);
  keys(296, "PEER", "next", "MODE", "back");
}

// Un aviso en una pastilla de color, centrado
static void pill(const char* text, int y, uint16_t color) {
  const int w = display::textWidth(text, Font::Label) + 12;
  display::roundRect(CX - w / 2, y, w, 13, color);
  display::text(text, CX, y + 3, Font::Label, ink::Bg, Align::Center);
}

void seedExportQr(const String& mnemonic, int nWords, int page, int pages) {
  resultHeader("EXPORT", page, pages);
  display::text(String(nWords) + " seed words", CX, 42, Font::Body, ink::Soft, Align::Center);
  pill("CAN SPEND - NO PHONES", 62, pal::RadioOff);
  display::qr(mnemonic, CX, 82, 150);
  centeredLines("Scan only with an offline wallet.", 240, 154, 2, 16, ink::Dim);
  keys(296, "PEER", "next", "MODE", "back");
}

void seedWatchOnly(const String& zpub, const String& fingerprint, int page, int pages) {
  resultHeader("WATCH", page, pages);
  display::text("Account zpub", CX, 42, Font::Body, ink::Soft, Align::Center);
  pill("VIEW ONLY - CAN'T SPEND", 62, ink::Ok);
  display::qr(zpub, CX, 82, 150);
  display::text("FINGERPRINT", CX, 240, Font::Label, ink::Dim, Align::Center);
  display::text(fingerprint, CX, 252, Font::Body, ink::Gold, Align::Center);
  keys(296, "PEER", "next", "MODE", "back");
}

void seedDone(int page, int pages) {
  resultHeader("DONE", page, pages);

  // Un escudo con un tic: la radio no se ha encendido en ningún momento
  static const char* const SHIELD[] = {
    "..##########..",
    ".############.",
    "##############",
    "##########.###",
    "#########..###",
    "###.####..####",
    "###..##..#####",
    "####....######",
    ".####..######.",
    ".############.",
    "..##########..",
    "...########...",
    ".....####.....",
  };
  display::bitmap(SHIELD, 13, CX - 21, 48, 3, ink::Ok);

  display::text("Radio was off", CX, 96, Font::Title, ink::Ok, Align::Center);
  display::text("the whole time", CX, 122, Font::Title, ink::Ok, Align::Center);
  centeredLines("Nothing left this badge. Your seed lived only in its memory.", 160, 154, 3, 16, ink::Soft);
  centeredLines("This is a learning lab. For real savings, use the Seeder.", 216, 154, 3, 16, ink::Dim);
  keys(282, "SEED", "hold: wipe + exit");
  keys(300, "MODE", "back");
}

}  // namespace screens
