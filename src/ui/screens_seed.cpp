// Pantallas del Seed Lab. La lógica está en app/seed_lab.cpp; aquí solo se dibuja.
#include "screens.h"
#include "screen_parts.h"
#include "display.h"
#include "../config.h"

namespace screens {

static const int CX = SCREEN_W / 2;

static void footer(const char* line1, const char* line2) {
  display::text(line1, CX, 286, Font::Body, ink::Dim, Align::Center);
  display::text(line2, CX, 302, Font::Body, ink::Dim, Align::Center);
}

static void pageHeader(const char* title, int page, int pages) {
  header(String(title) + " " + page + "/" + pages, ink::Ok);
}

// Un dado con sus puntos (el mismo dibujo que el Seeder)
static void die(int x, int y, int s, uint8_t value, uint16_t color) {
  static const uint16_t FACE[6] = {0x010, 0x101, 0x111, 0x145, 0x155, 0x16D};
  if (value < 1 || value > 6) return;
  TFT_eSprite& c = display::canvas();
  const int r = max(2, s / 8), pip = max(1, s / 10);
  c.fillRoundRect(x, y, s, s, r, ink::Bg);
  c.drawRoundRect(x, y, s, s, r, color);
  if (s >= 32) c.drawRoundRect(x + 1, y + 1, s - 2, s - 2, r - 1, color);
  const uint16_t face = FACE[value - 1];
  for (int i = 0; i < 9; i++)
    if (face & (1 << i)) c.fillCircle(x + s * (22 + 28 * (i % 3)) / 100, y + s * (22 + 28 * (i / 3)) / 100, pip, color);
}

// Una moneda: círculo dorado con una B
static void coin(int cx, int cy, int r) {
  TFT_eSprite& c = display::canvas();
  c.fillCircle(cx, cy, r, ink::Gold);
  c.drawCircle(cx, cy, r - 3, rgb(0xB8860B));
  display::text("B", cx + 1, cy - 7, Font::LabelBig, rgb(0x7A5A00), Align::Center);
}

// ============================================================ menú y palabras

void seedMenu(bool coinSelected) {
  header("SEED LAB", ink::Red);
  display::roundRect(6, 22, 158, 36, rgb(0x2A1416));
  display::text("Radio off", 14, 24, Font::Body, ink::Red);
  display::text("Offline, nothing is sent", 14, 40, Font::Body, rgb(0xE6A5A5));

  // Las dos formas de hacer entropía
  for (int i = 0; i < 2; i++) {
    const bool on = (i == 1) == coinSelected;
    const int y = 68 + i * 62;
    display::roundRect(6, y, 158, 56, on ? rgb(0x2B1C08) : ink::Card);
    if (on) display::rect(6, y + 2, 3, 52, ink::Orange);
    if (i == 0) die(16, y + 10, 36, 5, on ? ink::Orange : ink::Soft);
    else        coin(34, y + 28, 18);
    display::text(i == 0 ? "Dice" : "Coin", 62, y + 6, Font::Title, on ? ink::Orange : ink::Fg);
    display::text(i == 0 ? "50 or 99 rolls" : "128/256 flips", 62, y + 34, Font::Body, ink::Dim);
  }

  centeredLines("Learn how a Bitcoin seed is born, step by step.", 200, 150, 3, 16, ink::Soft);
  footer("PEER next - MODE go", "SEED exit");
}

void seedWords(uint8_t nWords, bool coin) {
  header("SEED LAB", ink::Red);
  display::text("How many words?", CX, 26, Font::Body, ink::Soft, Align::Center);

  for (int i = 0; i < 2; i++) {
    const bool on = (i == 0) == (nWords == 12);
    const int x = 14 + i * 76;
    display::roundRect(x, 50, 66, 60, on ? ink::Orange : ink::Card);
    display::roundRect(x + 2, 52, 62, 56, on ? rgb(0x2B1C08) : ink::Card);
    display::text(i == 0 ? "12" : "24", x + 33, 60, Font::Title, on ? ink::Orange : ink::Fg, Align::Center);
    display::text("WORDS", x + 33, 92, Font::Label, ink::Dim, Align::Center);
  }

  if (coin) {
    centeredLines("12 words need 128 coin flips. 24 words need 256.", 128, 150, 3, 16, ink::Soft);
    centeredLines("Every flip is one bit: heads 1, tails 0.", 196, 150, 3, 16, ink::Dim);
  } else {
    centeredLines("12 words need 50 dice rolls. 24 words need 99.", 128, 150, 3, 16, ink::Soft);
    centeredLines("Every roll gives about 2.6 bits of randomness.", 196, 150, 3, 16, ink::Dim);
  }
  footer("PEER change - MODE start", "SEED back");
}

// ============================================================ captura

void seedCoin(uint16_t done, uint16_t total, const uint8_t* entropy) {
  header("FLIP A COIN", ink::Gold);
  display::text("Toss a real coin", CX, 22, Font::Body, ink::Soft, Align::Center);
  coin(36, 60, 15);
  display::text("Heads = PEER", 60, 44, Font::Body, ink::Fg);
  display::text("Tails = MODE", 60, 62, Font::Body, ink::Fg);
  display::text(String(done) + " / " + total, CX, 86, Font::Title, ink::Gold, Align::Center);

  // Rejilla con todos los bits: naranja = 1, gris = 0, oscuro = por tirar
  for (int i = 0; i < (int)total; i++) {
    const int x = 21 + (i % 16) * 8, y = 120 + (i / 16) * 8;
    uint16_t c = ink::Line;
    if (i < done) c = (entropy[i / 8] >> (7 - i % 8)) & 1 ? ink::Orange : ink::Soft;
    display::rect(x, y, 7, 7, c);
  }
  footer("Every flip is one bit", "Hold SEED to quit");
}

void seedDice(uint8_t done, uint8_t total, uint8_t value, const uint8_t* lastRolls) {
  header("ROLL A DIE", ink::Gold);
  display::text("Roll a real die and", CX, 22, Font::Body, ink::Soft, Align::Center);
  display::text("mark what you got", CX, 38, Font::Body, ink::Soft, Align::Center);

  die(CX - 40, 62, 80, value, ink::Fg);
  display::text("PEER change - MODE save", CX, 150, Font::Body, ink::Dim, Align::Center);

  display::text(String(done) + " / " + total, CX, 174, Font::Title, ink::Gold, Align::Center);
  display::rect(14, 208, 142, 5, ink::Line);
  display::rect(14, 208, 142 * done / total, 5, ink::Orange);

  if (done > 0) {
    display::text("LAST ROLLS", CX, 224, Font::Label, ink::Dim, Align::Center);
    for (int i = 0; i < 3; i++) if (lastRolls[i]) die(CX - 46 + i * 32, 238, 28, lastRolls[i], ink::Soft);
  }
  display::text("Hold SEED to quit", CX, 302, Font::Body, ink::Dim, Align::Center);
}

void seedGenerating() {
  header("SEED LAB", ink::Red);
  display::text("Turning your", CX, 120, Font::Title, ink::Orange, Align::Center);
  display::text("bits into words", CX, 148, Font::Title, ink::Orange, Align::Center);
  display::text("BIP39 - one moment", CX, 190, Font::Body, ink::Dim, Align::Center);
}

// ============================================================ resultado

void seedMnemonic(const String& mnemonic, int from, int nWords, int page, int pages) {
  pageHeader("YOUR WORDS", page, pages);

  // Las palabras van separadas por espacios: nos saltamos las `from` primeras
  int start = 0;
  for (int skipped = 0; skipped < from; skipped++) start = mnemonic.indexOf(' ', start) + 1;
  for (int i = 0; i < 12 && from + i < nWords; i++) {
    int end = mnemonic.indexOf(' ', start);
    if (end < 0) end = mnemonic.length();
    const int y = 24 + i * 21;
    display::text(String(from + i + 1) + ".", 40, y, Font::Body, ink::Dim, Align::Right);
    display::text(mnemonic.substring(start, end), 48, y, Font::Body, ink::Fg);
    start = end + 1;
  }
  footer("PEER next - MODE back", "Write them down");
}

void seedAddress(const String& address, int page, int pages) {
  pageHeader("ADDRESS", page, pages);
  display::text("Your first address", CX, 22, Font::Body, ink::Soft, Align::Center);
  const int side = display::qr(address, CX, 42, 140);
  centeredLines(address, 48 + side, 156, 3, 16, ink::Fg);
  display::text("m/84'/0'/0'/0/0", CX, 260, Font::Body, ink::Dim, Align::Center);
  footer("PEER next - MODE back", "");
}

void seedEntropy(const String& hex, bool fromDice, int page, int pages) {
  pageHeader("ENTROPY", page, pages);
  display::text("The bits you made:", CX, 22, Font::Body, ink::Soft, Align::Center);

  // En grupos de 8 letras, dos grupos por línea
  for (unsigned i = 0, line = 0; i < hex.length(); i += 16, line++) {
    String part = hex.substring(i, min<unsigned>(i + 8, hex.length()));
    if (i + 8 < hex.length()) part += " " + hex.substring(i + 8, min<unsigned>(i + 16, hex.length()));
    display::text(part, CX, 44 + line * 18, Font::Body, ink::Gold, Align::Center);
  }
  centeredLines(fromDice ? "Your rolls hashed with SHA-256, like Coldcard does. Same rolls, same words."
                         : "Every coin flip is one of these bits. Same flips, same words.",
                200, 150, 4, 16, ink::Dim);
  footer("PEER next - MODE back", "");
}

void seedDone(int page, int pages) {
  pageHeader("DONE", page, pages);
  display::text("That's how", CX, 26, Font::Title, ink::Orange, Align::Center);
  display::text("a seed is born", CX, 54, Font::Title, ink::Orange, Align::Center);

  display::roundRect(6, 96, 158, 124, rgb(0x2A1416));
  centeredLines("This badge has a radio. Don't use these words for real money.", 102, 146, 4, 16, ink::Red);
  centeredLines("For your savings, use an offline device like the Seeder.", 168, 146, 3, 16, rgb(0xE6A5A5));

  display::text("Hold SEED to wipe", CX, 236, Font::Body, ink::Fg, Align::Center);
  display::text("everything and exit", CX, 252, Font::Body, ink::Fg, Align::Center);
  footer("MODE back", "");
}

}  // namespace screens
