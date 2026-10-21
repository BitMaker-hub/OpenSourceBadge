#include "display.h"
#include "../config.h"
#include "../hardware/power.h"
#include <qrcoded.h>

namespace display {

static TFT_eSPI tft;
static TFT_eSprite lienzo(&tft);

void begin() {
  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.fillScreen(TFT_BLACK);

  lienzo.setColorDepth(16);
  if (!lienzo.createSprite(SCREEN_W, SCREEN_H)) {
    Serial.println("[display] no hay memoria para el lienzo");
  }
}

void push()            { lienzo.pushSprite(0, 0); }
TFT_eSprite& canvas()  { return lienzo; }

// ---------------------------------------------------------------- formas
void clear(uint16_t color) { lienzo.fillSprite(color); }

void rect(int x, int y, int w, int h, uint16_t color) {
  if (w > 0 && h > 0) lienzo.fillRect(x, y, w, h, color);
}

void roundRect(int x, int y, int w, int h, uint16_t color) {
  rect(x + 1, y, w - 2, h, color);
  rect(x, y + 1, w, h - 2, color);
}

// ---------------------------------------------------------------- texto
static void useFont(Font f) {
  lienzo.setTextSize(1);
  switch (f) {
    case Font::Label: lienzo.setTextFont(1); break;   // GLCD 6x8
    case Font::LabelBig: lienzo.setTextFont(1); lienzo.setTextSize(2); break;
    case Font::Body:  lienzo.setTextFont(2); break;   // 16 px
    case Font::Title: lienzo.setTextFont(4); break;   // 26 px
  }
}

void text(const String& s, int x, int y, Font f, uint16_t color, Align a) {
  useFont(f);

  // Red de seguridad: si un texto se sale de la pantalla, avisamos por el puerto serie
  const int w = lienzo.textWidth(s);
  const int left = a == Align::Left ? x : a == Align::Center ? x - w / 2 : x - w;
  if (left < 0 || left + w > SCREEN_W) {
    static String lastWarning;
    if (s != lastWarning) Serial.printf("[pantalla] no cabe (%d px): \"%s\"\n", w, s.c_str());
    lastWarning = s;
  }
  lienzo.setTextColor(color);   // sin color de fondo: el texto es transparente
  lienzo.setTextDatum(a == Align::Left ? TL_DATUM : a == Align::Center ? TC_DATUM : TR_DATUM);
  lienzo.drawString(s, x, y);
}

int textWidth(const String& s, Font f) {
  useFont(f);
  return lienzo.textWidth(s);
}

String fit(const String& s, int maxWidth, Font f) {
  if (textWidth(s, f) <= maxWidth) return s;
  String cut = s;
  while (cut.length() > 1 && textWidth(cut + "..", f) > maxWidth) cut.remove(cut.length() - 1);
  return cut + "..";
}

Font name(const String& s, int centerX, int y, int maxWidth, uint16_t color) {
  Font f = textWidth(s, Font::Title) <= maxWidth ? Font::Title : Font::Body;
  // La letra normal es más baja: la bajamos un poco para que quede centrada en el hueco
  text(fit(s, maxWidth, f), centerX, f == Font::Title ? y : y + 5, f, color, Align::Center);
  return f;
}

int wrap(const String& s, int maxWidth, Font f, String* lines, int maxLines) {
  int n = 0;
  String line;
  int start = 0;
  while (start <= (int)s.length() && n < maxLines) {
    int space = s.indexOf(' ', start);
    if (space < 0) space = s.length();
    String word = s.substring(start, space);
    start = space + 1;

    String tryLine = line.length() ? line + " " + word : word;
    if (textWidth(tryLine, f) <= maxWidth) { line = tryLine; continue; }

    // No cabe: cerramos la línea que llevábamos
    if (line.length()) { lines[n++] = line; line = ""; }
    // Una palabra sola que no cabe (una dirección): se corta por letras
    for (unsigned i = 0; i < word.length() && n < maxLines; i++) {
      if (textWidth(line + word[i], f) > maxWidth) { lines[n++] = line; line = ""; }
      line += word[i];
    }
  }
  if (line.length() && n < maxLines) lines[n++] = line;
  return n;
}

// ---------------------------------------------------------------- piezas
// La pila. Cargando (con USB) se va llenando en dorado una y otra vez, como en
// un móvil: con el USB puesto la tensión de la batería no dice cuánto le queda.
static void batteryIcon(int x, int y, uint8_t pct, power::Charge charge) {
  rect(x, y, 15, 7, ink::Dim);
  rect(x + 1, y + 1, 13, 5, ink::Bg);
  rect(x + 15, y + 2, 1, 3, ink::Dim);
  switch (charge) {
    case power::Charge::Charging:  rect(x + 2, y + 2, 11 * ((millis() / 450) % 5) / 4, 3, ink::Gold); break;
    case power::Charge::Full:      rect(x + 2, y + 2, 11, 3, ink::Ok); break;
    case power::Charge::NoBattery: break;                                  // vacía: no hay batería que enseñar
    case power::Charge::OnBattery: rect(x + 2, y + 2, 11 * pct / 100, 3, pct > 20 ? ink::Ok : ink::Red); break;
  }
}

// Un rayito de 5 x 9 delante de la pila: está cargando
static void chargingBolt(int x, int y) {
  static const char* const BOLT[] = {"..###", ".###.", ".##..", "#####", "..##.", ".##..", ".#...", "#....", "....."};
  for (int j = 0; j < 9; j++)
    for (int i = 0; i < 5; i++)
      if (BOLT[j][i] == '#') lienzo.drawPixel(x + i, y + j, ink::Gold);
}

void header(const String& title, uint16_t titleColor, uint8_t batteryPercent, uint16_t dotColor) {
  rect(0, 0, SCREEN_W, 17, ink::Card);
  rect(0, 17, SCREEN_W, 1, ink::Line);
  text(title, 6, 1, Font::Body, titleColor);
  int right = 143;                                   // lo que va a la izquierda de la pila
  const power::Charge charge = power::chargeState();
  if (charge == power::Charge::Charging || charge == power::Charge::Full) {
    chargingBolt(right - 5, 4);
    right -= 7;
  } else if (charge == power::Charge::NoBattery) {    // interruptor en OFF (no carga) o sin batería
    text("NO BAT", right, 5, Font::Label, ink::Gold, Align::Right);
    right -= textWidth("NO BAT", Font::Label);
  } else {
    const String pct = String(batteryPercent) + "%";
    text(pct, right, 1, Font::Body, ink::Dim, Align::Right);
    right -= textWidth(pct, Font::Body);
  }
  if (dotColor) lienzo.fillCircle(right - 7, 8, 3, dotColor);
  batteryIcon(148, 5, batteryPercent, charge);
}

int chip(const String& label, int centerX, int y, uint16_t color) {
  const int w = textWidth(label, Font::Label) + 8;
  roundRect(centerX - w / 2, y, w, 11, color);
  text(label, centerX, y + 2, Font::Label, ink::Bg, Align::Center);
  return w;
}

void pageDots(int current, int count) {
  const int x0 = SCREEN_W / 2 - count * 10 / 2;
  for (int i = 0; i < count; i++) roundRect(x0 + i * 10 + 2, 308, 6, 4, i == current ? ink::Orange : ink::Line);
}

void bitmap(const char* const* rows, int nRows, int x, int y, int scale, uint16_t color) {
  for (int j = 0; j < nRows; j++)
    for (int i = 0; rows[j][i]; i++)
      if (rows[j][i] == '#') rect(x + i * scale, y + j * scale, scale, scale, color);
}

// Iconos de 8 px de alto, dibujados con '#'
static const char* const ICON_PEERS[] = {"...##.....##...", "..####...####..", "..####...####..", "...##.....##...",
                                         "...............", ".######.######.", "###############", "###############"};
static const char* const ICON_BOLT[]  = {"......####", ".....####.", "....####..", "...#######",
                                         ".......##.", "......##..", ".....##...", "....#....."};
static const char* const ICON_BARS[]  = {".........##", ".........##", "......#####", "......#####",
                                         "...########", "...########", "###########", "###########"};
static const char* const ICON_CHAIN[] = {"..##....##..", ".#..#..#..#.", ".#..####..#.", "..##....##.."};

void icon(Icon which, int x, int y, uint16_t color) {
  switch (which) {
    case Icon::Peers: bitmap(ICON_PEERS, 8, x, y, 1, color); break;
    case Icon::Bolt:  bitmap(ICON_BOLT, 8, x, y, 1, color); break;
    case Icon::Bars:  bitmap(ICON_BARS, 8, x, y, 1, color); break;
    case Icon::Chain: bitmap(ICON_CHAIN, 4, x, y + 2, 1, color); break;
  }
}

// ---------------------------------------------------------------- logos
uint16_t blend(uint16_t a, uint16_t b, uint8_t t) {
  const int r = ((a >> 11) & 31) + ((((b >> 11) & 31) - ((a >> 11) & 31)) * t) / 255;
  const int g = ((a >> 5) & 63) + ((((b >> 5) & 63) - ((a >> 5) & 63)) * t) / 255;
  const int bl = (a & 31) + (((b & 31) - (a & 31)) * t) / 255;
  return r << 11 | g << 5 | bl;
}

void logo(const Logo& l, int centerX, int y, uint8_t fade) {
  if (fade == 0) return;
  const int x = centerX - l.width / 2;
  // Píxel a píxel (y no pushImage) para no liarnos con el orden de bytes del lienzo.
  // Mientras aparece, cada píxel va a medio camino entre el fondo y su color.
  for (int j = 0; j < l.height; j++) {
    for (int i = 0; i < l.width; i++) {
      const uint16_t c = l.pixels[j * l.width + i];
      lienzo.drawPixel(x + i, y + j, fade == 255 ? c : blend(ink::Bg, c, fade));
    }
  }
}

void logo(const AlphaLogo& l, int x, int y) {
  for (int j = 0; j < l.height; j++) {
    for (int i = 0; i < l.width; i++) {
      const uint8_t a = l.alpha[j * l.width + i];
      if (a == 0) continue;
      const uint16_t c = l.pixels[j * l.width + i];
      lienzo.drawPixel(x + i, y + j, a == 255 ? c : blend(lienzo.readPixel(x + i, y + j), c, a));
    }
  }
}

// ---------------------------------------------------------------- QR
int qr(const String& data, int centerX, int y, int maxSize) {
  static uint8_t buffer[700];   // cabe hasta la versión 13
  QRCode code;

  // La versión más pequeña en la que cabe el texto: módulos más grandes, más fácil de leer
  int version = 1;
  while (version <= 13 && !qrcode_fitsBytes(version, ECC_MEDIUM, data.length())) version++;
  if (version > 13) return 0;
  qrcode_initText(&code, buffer, version, ECC_MEDIUM, data.c_str());

  const int modules = code.size + 8;                 // 4 módulos de margen blanco a cada lado, como pide el estándar
  const int scale = max(1, maxSize / modules);
  const int side = modules * scale;
  const int x = centerX - side / 2;

  roundRect(x, y, side, side, ink::White);
  for (int r = 0; r < code.size; r++)
    for (int c = 0; c < code.size; c++)
      if (qrcode_getModule(&code, c, r)) rect(x + (c + 4) * scale, y + (r + 4) * scale, scale, scale, ink::Bg);
  return side;
}

void screenshotToSerial() {
  Serial.printf("SHOT %d %d\n", SCREEN_W, SCREEN_H);
  uint8_t row[SCREEN_W * 2];
  for (int y = 0; y < SCREEN_H; y++) {
    for (int x = 0; x < SCREEN_W; x++) {
      const uint16_t c = lienzo.readPixel(x, y);
      row[x * 2] = c >> 8;
      row[x * 2 + 1] = c & 0xFF;
    }
    Serial.write(row, sizeof row);
  }
  Serial.println("END");
}

void qrToSerial(const String& data) {
  static uint8_t buffer[700];
  QRCode code;
  int version = 1;
  while (version <= 13 && !qrcode_fitsBytes(version, ECC_MEDIUM, data.length())) version++;
  if (version > 13) { Serial.println("QR: no cabe"); return; }
  qrcode_initText(&code, buffer, version, ECC_MEDIUM, data.c_str());
  Serial.printf("QR v%d (%d modulos): %s\n", version, code.size, data.c_str());
  for (int r = 0; r < code.size; r++) {
    for (int c = 0; c < code.size; c++) Serial.print(qrcode_getModule(&code, c, r) ? '#' : '.');
    Serial.println();
  }
  Serial.println("QR fin");
}

}  // namespace display
