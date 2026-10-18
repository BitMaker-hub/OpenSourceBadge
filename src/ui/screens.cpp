#include "screens.h"
#include "screen_parts.h"
#include "display.h"
#include "avatars.h"
#include "logos_data.h"
#include "../config.h"
#include "../hardware/leds.h"
#include "../hardware/power.h"
#include "../net/setup_portal.h"
#include "../net/nostr_zaps.h"
#include "../net/lnbits.h"
#include "../net/miner.h"

namespace screens {

static const int CX = SCREEN_W / 2;   // centro horizontal, 85

// "12600" -> "12,600"
static String thousands(uint32_t n) {
  String s = String(n);
  for (int i = s.length() - 3; i > 0; i -= 3) s = s.substring(0, i) + "," + s.substring(i);
  return s;
}

// Cabecera de todas las pantallas. El punto dice cómo va Nostr:
// morado = escuchando zaps, gris = conectando, sin punto = sin configurar o en pausa
void header(const String& title, uint16_t color) {
  uint16_t dot = 0;
  switch (nostrzaps::status()) {
    case nostrzaps::Status::Listening:       dot = ink::Nostr; break;
    case nostrzaps::Status::ConnectingWifi:
    case nostrzaps::Status::ConnectingRelay: dot = ink::Dim;   break;
    default:                                 break;
  }
  display::header(title, color, power::batteryPercent(), dot);
}

// Texto partido en líneas y centrado
void centeredLines(const String& s, int y, int maxWidth, int maxLines, int lineHeight, uint16_t color) {
  String lines[4];
  const int n = display::wrap(s, maxWidth, Font::Body, lines, min(maxLines, 4));
  for (int i = 0; i < n; i++) display::text(lines[i], CX, y + i * lineHeight, Font::Body, color, Align::Center);
}

// ============================================================ PÁGINAS DE INICIO

void profile() {
  const Profile& me = badge::me();
  char title[16];
  snprintf(title, sizeof title, "BADGE #%03u", badge::number());
  header(title, ink::Orange);

  avatars::card(me.role, 37, 26, 3);
  display::name(me.name, CX, 132, 158);
  // Debajo del nombre: su X, o si no tiene, el principio del npub, o nada
  const String sub = me.handle.length() ? me.handle : me.npub.length() ? me.npub.substring(0, 12) + ".." : "";
  if (sub.length()) display::text(display::fit(sub, 158), CX, 158, Font::Body, ink::Dim, Align::Center);
  display::chip(roleInfo(me.role).name, CX, 177, roleInfo(me.role).color);

  // Tarjeta con los tres números del badge
  display::roundRect(6, 194, 158, 64, ink::Card);
  struct Row { Icon icon; uint16_t color; const char* label; String value; };
  const Row rows[] = {
    {Icon::Peers, ink::Orange, "Peers",            String(badge::peerCount())},
    {Icon::Bolt,  ink::Gold,   "Social hash",      badge::socialHash() + " TH"},
    {Icon::Bars,  ink::Ok,     "Decentralized",    String(badge::decentralization()) + "%"},
  };
  for (int i = 0; i < 3; i++) {
    const int y = 198 + i * 20;
    if (i) display::rect(12, y - 2, 146, 1, ink::Line);
    display::icon(rows[i].icon, 13, y + 4, rows[i].color);
    display::text(rows[i].label, 32, y, Font::Body, ink::Soft);
    display::text(rows[i].value, 158, y, Font::Body, ink::Fg, Align::Right);
  }

  centeredLines("\"" + me.tagline + "\"", 266, 150, 2, 16, ink::Dim);
  display::pageDots(0, HOME_PAGES);
}

void qrCodes(const QrItem* items, int count, int current) {
  const QrItem& q = items[current];
  header(q.header, q.color);

  const int side = display::qr(q.data, CX, 26, 140);
  const int below = 26 + side;

  // Título con su icono delante
  const int titleW = display::textWidth(q.title, Font::Label);
  display::icon(q.kind == QrKind::Lightning ? Icon::Bolt : Icon::Chain, CX - titleW / 2 - 16, below + 10, q.color);
  display::text(q.title, CX + 2, below + 11, Font::Label, q.color, Align::Center);

  // La dirección, partida por la @ si no cabe
  String sub = q.subtitle;
  sub.replace("@", " @");
  centeredLines(sub, below + 24, 156, 2, 16, ink::Soft);

  // Pestañas LN / NOSTR / X
  int total = -4;
  for (int i = 0; i < count; i++) total += display::textWidth(items[i].tab, Font::Label) + 14;
  int x = CX - total / 2;
  for (int i = 0; i < count; i++) {
    const int w = display::textWidth(items[i].tab, Font::Label) + 10;
    const bool on = i == current;
    display::roundRect(x, 266, w, 13, on ? items[i].color : ink::Card);
    display::text(items[i].tab, x + w / 2, 269, Font::Label, on ? ink::Bg : ink::Dim, Align::Center);
    x += w + 4;
  }
  display::text("Changes every 6 s", CX, 286, Font::Body, ink::Dim, Align::Center);
  display::pageDots(1, HOME_PAGES);
}

void peers() {
  header("PEERS - " + String(badge::peerCount()), ink::Orange);

  const int shown = min(6, badge::peerCount());
  for (int i = 0; i < shown; i++) {
    const Peer& p = badge::peer(i);
    const RoleInfo& r = roleInfo(p.role);
    const int y = 22 + i * 46;

    display::roundRect(6, y, 158, 42, i == 0 ? ink::Card2 : ink::Card);
    display::roundRect(9, y + 3, 36, 36, r.color);
    display::rect(11, y + 5, 32, 32, ink::Pattern);
    avatars::draw(p.role, 11, y + 5, 1);

    display::text(display::fit(p.name, 74), 52, y + 3);
    display::chip(r.name, 52 + display::textWidth(r.name, Font::Label) / 2 + 4, y + 25, r.color);
    display::text("#" + String(p.block), 158, y + 3, Font::Body, ink::Orange, Align::Right);
    display::text(badge::clock(p.when), 158, y + 22, Font::Body, ink::Dim, Align::Right);
  }
  if (shown == 0) display::text("Nobody yet. Press PEER!", CX, 140, Font::Body, ink::Dim, Align::Center);
  display::pageDots(2, HOME_PAGES);
}

void stats() {
  header("SOCIAL MINING", ink::Orange);

  // Altura de bloque y una gráfica de peers por hora (de adorno por ahora)
  display::roundRect(6, 22, 158, 52, ink::Card);
  display::text("Block height", 14, 26, Font::Body, ink::Dim);
  display::text(String(badge::blockHeight()), 14, 44, Font::Title);
  static const uint8_t HIST[] = {1, 3, 2, 5, 8, 6, 4, 7, 9, 6};
  for (int i = 0; i < 10; i++) {
    const int h = HIST[i] * 3;
    display::rect(92 + i * 7, 68 - h, 5, h, i == 9 ? ink::Orange : rgb(0x6A4312));
  }

  // Los seis roles: en color los que ya conoces
  display::roundRect(6, 80, 158, 92, ink::Card);
  display::text("Roles met", 14, 84, Font::Body, ink::Dim);
  int met = 0;
  for (int r = 0; r < (int)Role::Count; r++) if (badge::metRole((Role)r)) met++;
  display::text(String(met) + " / 6", 158, 84, Font::Body, ink::Fg, Align::Right);
  for (int r = 0; r < (int)Role::Count; r++) {
    const int x = 21 + (r % 3) * 48;
    const int y = 104 + (r / 3) * 34;
    const bool known = badge::metRole((Role)r);
    display::roundRect(x - 2, y - 1, 36, 34, known ? roleInfo((Role)r).color : ink::Line);
    display::rect(x, y + 1, 32, 30, ink::Pattern);
    avatars::draw((Role)r, x, y, 1, !known);
  }

  // Descentralización
  const uint8_t d = badge::decentralization();
  display::roundRect(6, 178, 158, 40, ink::Card);
  display::text("Decentralization", 14, 182, Font::Body, ink::Dim);
  display::text(String(d) + "%", 158, 182, Font::Body, ink::Ok, Align::Right);
  display::rect(14, 204, 142, 6, ink::Line);
  display::rect(14, 204, 142 * d / 100, 6, ink::Ok);

  // Resumen
  display::roundRect(6, 224, 158, 58, ink::Card);
  display::text("Social hash", 14, 228, Font::Body, ink::Dim);
  display::text(badge::socialHash() + " TH", 158, 228, Font::Body, ink::Gold, Align::Right);
  // Con monedero LNbits se enseña lo que hay de verdad; si no, lo recibido por zaps
  const int32_t walletSats = lnbits::balanceSats();
  display::text(walletSats >= 0 ? "Wallet" : "Zaps", 14, 245, Font::Body, ink::Dim);
  display::text(thousands(walletSats >= 0 ? walletSats : badge::satsReceived()) + " sats", 158, 245, Font::Body, ink::Gold, Align::Right);
  if (walletSats >= 0) display::text("HOLD PEER TO WITHDRAW", CX, 290, Font::Label, ink::Gold, Align::Center);
  display::text("Battery", 14, 262, Font::Body, ink::Dim);
  display::text(power::usbConnected() ? String("USB") : String(power::batteryPercent()) + "%",
                158, 262, Font::Body, ink::Fg, Align::Right);
  display::pageDots(3, HOME_PAGES);
}

// ============================================================ ARRANQUE

// De 0 a 255 entre `start` y `start + duration` ms (para hacer aparecer cosas)
static uint8_t fadeIn(uint32_t t, uint32_t start, uint32_t duration) {
  if (t <= start) return 0;
  if (t >= start + duration) return 255;
  return (t - start) * 255 / duration;
}

// Aparece, y en los últimos 300 ms de la escena se va
static uint8_t sceneFade(uint32_t t, uint32_t start, uint32_t duration) {
  const uint8_t in = fadeIn(t, start, duration);
  const uint8_t out = t > BOOT_SCENE_MS - 300 ? 255 - fadeIn(t, BOOT_SCENE_MS - 300, 300) : 255;
  return min(in, out);
}

static void fadedText(const String& s, int y, Font f, uint16_t color, uint8_t fade) {
  if (fade) display::text(s, CX, y, f, display::blend(ink::Bg, color, fade), Align::Center);
}

void bootAmsterdam(uint32_t t) {
  display::logo(LOGO_BAMS_MEDALLION, CX, 65, sceneFade(t, 0, 500));
  display::logo(LOGO_BAMS_WORDMARK, CX, 169, sceneFade(t, 300, 500));
  fadedText("2026", 239, Font::LabelBig, ink::Orange, sceneFade(t, 700, 400));
}

void bootMakers(uint32_t t) {
  fadedText("DESIGNED BY", 72, Font::Label, ink::Dim, sceneFade(t, 0, 300));
  display::logo(LOGO_BITMAKER, CX, 90, sceneFade(t, 100, 500));
  fadedText("BUILT BY", 183, Font::Label, ink::Dim, sceneFade(t, 600, 300));
  display::logo(LOGO_BITRONICS, CX, 201, sceneFade(t, 700, 500));
  fadedText("bitronics.store", 230, Font::Body, ink::Dim, sceneFade(t, 1000, 400));
}


// Los 18 LEDs de la ₿ en coordenadas del dibujo (las mismas del prototipo)
static const float B_POINTS[18][2] = {
  {0, 18}, {0, 13.5}, {0, 9}, {0, 4.5}, {0, 0}, {4.5, 0}, {8.2, 1.2}, {9.6, 4.5}, {8.2, 7.8},
  {4.5, 9}, {10.2, 10.5}, {11, 13.5}, {9.5, 16.8}, {4.5, 18}, {5.6, 21.6}, {1.4, 21.6}, {1.4, -3.6}, {5.6, -3.6}};
static const uint8_t B_EDGES[][2] = {
  {0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {6, 7}, {7, 8}, {8, 9}, {9, 2}, {9, 10},
  {10, 11}, {11, 12}, {12, 13}, {13, 0}, {13, 14}, {14, 15}, {15, 0}, {4, 16}, {16, 17}, {17, 5}};

void bootBadge(uint32_t t) {
  // La ₿ se dibuja punto a punto, a la vez que los LEDs de verdad
  const int n = min<int>(18, t / 70);
  auto px = [](int i) { return 61 + (int)(B_POINTS[i][0] * 5); };
  auto py = [](int i) { return 66 + (int)(B_POINTS[i][1] * 5); };
  for (auto& e : B_EDGES)
    if (e[0] < n && e[1] < n) display::canvas().drawLine(px(e[0]), py(e[0]), px(e[1]), py(e[1]), rgb(0x5A3A12));
  for (int i = 0; i < n; i++) display::rect(px(i) - 3, py(i) - 3, 7, 7, ink::Orange);

  if (t > 1400) {
    display::text("OPEN SOURCE", CX, 202, Font::LabelBig, ink::Fg, Align::Center);
    display::text("BADGE", CX, 222, Font::LabelBig, ink::Fg, Align::Center);
  }
  if (t > 1900) {
    display::text("Proof of Peer", CX, 248, Font::Body, ink::Orange, Align::Center);
    display::text("Bitcoin Amsterdam 2026", CX, 264, Font::Body, ink::Dim, Align::Center);
  }
  if (t > 2300) {
    display::rect(35, 292, 100, 3, ink::Line);
    display::rect(35, 292, min<int>(100, (t - 2300) / 8), 3, ink::Orange);
  }
}

// ============================================================ OTRAS PANTALLAS

void qrFullscreen(const QrItem& q) {
  display::clear(ink::White);
  const int side = display::qr(q.data, CX, 30, 166);
  const int below = 30 + side;
  display::text(q.title, CX, below + 14, Font::LabelBig, ink::Bg, Align::Center);
  String sub = q.subtitle;
  sub.replace("@", " @");
  centeredLines(sub, below + 40, 158, 2, 16, rgb(0x3A3F4B));
  display::text("PEER - Back", CX, 298, Font::Body, ink::Dim, Align::Center);
}

void setup(uint32_t t, int phonesConnected) {
  header("SETUP", ink::Orange);

  display::text("Hello, pleb!", CX, 22, Font::Title, ink::Orange, Align::Center);
  display::text("1. Scan to join my WiFi", CX, 52, Font::Body, ink::Soft, Align::Center);
  const int side = display::qr(portal::wifiQr(), CX, 72, 124);
  display::text(badge::wifiName(), CX, 76 + side, Font::Body, ink::Gold, Align::Center);
  display::text("Open WiFi, no password", CX, 92 + side, Font::Body, ink::Dim, Align::Center);

  display::text("2. Then open", CX, 222, Font::Body, ink::Soft, Align::Center);
  display::text("badge.local", CX, 238, Font::Title, ink::Gold, Align::Center);

  if (phonesConnected > 0) display::text("Phone connected!", CX, 270, Font::Body, ink::Ok, Align::Center);

  const int left = max<int>(0, 299 - t / 1000);
  char closes[24];
  snprintf(closes, sizeof closes, "MODE exits - %d:%02d", left / 60, left % 60);
  display::text(closes, CX, 300, Font::Body, ink::Dim, Align::Center);
}

void setupSaved() {
  const Profile& me = badge::me();
  header("SETUP", ink::Ok);
  display::text("Saved!", CX, 30, Font::Title, ink::Ok, Align::Center);
  avatars::card(me.role, 37, 74, 3);
  display::name(me.name, CX, 184, 158);
  display::chip(roleInfo(me.role).name, CX, 214, roleInfo(me.role).color);
  display::text("Welcome to the network", CX, 250, Font::Body, ink::Dim, Align::Center);
}

static const char* const BOLT[] = {
  "......######", ".....######.", "....######..", "...######...", "..########..", ".#########..",
  "......####..", ".....####...", "....####....", "...####.....", "..###.......", ".##........."};

void zap(const Zap& z, uint32_t t) {
  if (t < 1200 && (t / 120) % 2) display::clear(rgb(0x1A1606));   // destello al llegar
  display::bitmap(BOLT, 12, CX - 36, 30, 6, ink::Gold);
  display::text("ZAP!", CX, 110, Font::LabelBig, ink::Gold, Align::Center);
  display::text("+" + thousands(z.amount) + " sats", CX, 132, Font::Title, ink::Fg, Align::Center);
  display::text("From " + z.from, CX, 170, Font::Body, ink::Soft, Align::Center);
  display::text("Via " + z.via, CX, 188, Font::Body, z.via == "Nostr" ? ink::Nostr : ink::Gold, Align::Center);
  if (z.message.length()) {
    display::roundRect(10, 212, 150, 42, ink::Card);
    centeredLines("\"" + z.message + "\"", 216, 140, 2, 16, ink::Soft);
  }
  // Con monedero LNbits, lo que hay en él; si no, lo recibido por zaps
  const int32_t wallet = lnbits::balanceSats();
  display::text((wallet >= 0 ? "Wallet " + thousands(wallet) : "Total " + thousands(badge::satsReceived())) + " sats",
                CX, 284, Font::Body, ink::Dim, Align::Center);
}

// ============================================================ HANDSHAKE

void handshakeSearching(uint32_t t) {
  header("PEER", ink::Nostr);
  display::text("Looking for peers nearby", CX, 26, Font::Body, ink::Soft, Align::Center);
  // Tres ondas que salen del centro y se apagan
  for (int k = 0; k < 3; k++) {
    const float phase = fmodf(t / 900.0f + k / 3.0f, 1.0f);
    const uint16_t c = rgb(leds::blend(0xA78BFA, 0x0C0E13, phase * 255));
    display::canvas().drawCircle(CX, 158, 26 + phase * 60, c);
  }
  avatars::card(badge::me().role, 69, 142, 1);
  display::text("Hold badges close", CX, 262, Font::Body, ink::Dim, Align::Center);
}

void handshakeFound(const Peer& other, uint32_t t) {
  header("PEER", ink::Nostr);
  const float k = min(1.0f, t / 500.0f);               // el otro baja desde arriba
  avatars::card(other.role, 53, 30 - (int)(24 * (1 - k)), 2);
  avatars::card(badge::me().role, 53, 222, 2);
  for (int y = 104; y < 214; y += 6) {                 // "datos" pasando de uno a otro
    const bool on = ((y / 6) + (t / 80)) % 3 == 0;
    display::rect(84, y, 2, 3, on ? ink::Nostr : ink::Line);
  }
  const RoleInfo& r = roleInfo(other.role);
  display::text("Found", 112, 140, Font::Body, ink::Dim);
  display::text(display::fit(other.name, 54), 112, 156);
  display::chip(r.name, 112 + display::textWidth(r.name, Font::Label) / 2 + 4, 176, r.color);
}

void handshakeMining(const String* hashes, int count, uint8_t progress, uint32_t nonce) {
  header("PEER", ink::Nostr);
  display::text("Mining handshake", CX, 22, Font::Body, ink::Orange, Align::Center);
  for (int i = 0; i < count; i++)
    display::text((i ? "  " : "> ") + hashes[i], 14, 42 + i * 14, Font::Body, i ? ink::Dim : ink::Fg);
  display::rect(14, 252, 142, 5, ink::Line);
  display::rect(14, 252, 142 * progress / 255, 5, ink::Orange);
  display::text("Nonce " + String(nonce), CX, 264, Font::Body, ink::Dim, Align::Center);
}

void handshakeNotFound() {
  header("PEER", ink::Nostr);
  avatars::card(badge::me().role, 53, 80, 2);
  display::text("No badge", CX, 170, Font::Title, ink::Fg, Align::Center);
  display::text("nearby", CX, 196, Font::Title, ink::Fg, Align::Center);
  centeredLines("Both press PEER at the same time and hold the badges close.", 232, 150, 3, 16, ink::Dim);
}

void handshakeMined(const Peer& other) {
  header("PEER", ink::Nostr);
  display::text("Block #" + String(other.block), CX, 20, Font::Title, ink::Ok, Align::Center);
  display::text("MINED!", CX, 48, Font::LabelBig, ink::Ok, Align::Center);
  avatars::card(other.role, 37, 74, 3);
  display::name(other.name, CX, 180, 158);
  display::chip(roleInfo(other.role).name, CX, 208, roleInfo(other.role).color);
  display::text("+1 peer", CX, 224, Font::Title, ink::Orange, Align::Center);
  display::text("Contact saved", CX, 254, Font::Body, ink::Dim, Align::Center);
  display::text("Nostr - X - Zaps", CX, 270, Font::Body, ink::Dim, Align::Center);
}

// ============================================================ NERDMINER

// 1234567 -> "1.23M", como el NerdMiner
static String shortNumber(double v) {
  if (v < 1) return String(v, v < 0.001 ? 6 : 4);                 // las pequeñas, con decimales
  static const char* const SUFFIX[] = {"", "K", "M", "G", "T", "P"};
  int k = 0;
  while (v >= 1000 && k < 5) { v /= 1000; k++; }
  return String(v, v < 10 ? 2 : v < 100 ? 1 : 0) + SUFFIX[k];
}

void miner(const nerdminer::Stats& s, const float* history, int historyNext, bool online) {
  header("NERDMINER", ink::Orange);

  // Estado en una pastilla
  const char* state = !s.running ? "STOPPED" : !online ? "NO INTERNET" : s.connected ? "MINING" : "CONNECTING";
  const uint16_t stateColor = !s.running ? ink::Dim : !online ? ink::Red : s.connected ? ink::Ok : ink::Gold;
  const int pw = display::textWidth(state, Font::Label) + 12;
  display::roundRect(CX - pw / 2, 24, pw, 13, stateColor);
  display::text(state, CX, 27, Font::Label, ink::Bg, Align::Center);

  // Hashrate grande y su gráfica
  display::text(String(s.khs, 1), CX, 46, Font::Title, ink::Fg, Align::Center);
  display::text("KH/S", CX, 74, Font::Label, ink::Dim, Align::Center);
  float top = 1;
  for (int i = 0; i < 24; i++) top = max(top, history[i]);
  for (int i = 0; i < 24; i++) {
    const float v = history[(historyNext + i) % 24];
    const int h = (int)(28 * v / top);
    display::rect(14 + i * 6, 116 - h, 4, h, i == 23 ? ink::Orange : rgb(0x6A4312));
  }

  // Números: los shares que ha aceptado el pool (los mismos que enseña su web),
  // la mejor dificultad del badge frente a la que pide el pool, y cuánto lleva
  const uint32_t min = s.minedMs / 60000;
  const String time = min >= 60 ? String(min / 60) + "h " + String(min % 60) + "m" : String(min) + " min";
  display::roundRect(6, 122, 158, 112, ink::Card);
  display::text("Shares", 14, 126, Font::Body, ink::Dim);
  display::text(String(s.accepted), 158, 126, Font::Body, ink::Fg, Align::Right);
  display::text("Best diff", 14, 144, Font::Body, ink::Dim);
  display::text(shortNumber(s.bestDiff), 158, 144, Font::Body, ink::Gold, Align::Right);
  display::text("Pool diff", 14, 162, Font::Body, ink::Dim);
  display::text(s.poolDiff > 0 ? shortNumber(s.poolDiff) : String("--"), 158, 162, Font::Body, ink::Fg, Align::Right);
  display::text("Time", 14, 180, Font::Body, ink::Dim);
  display::text(time, 158, 180, Font::Body, ink::Fg, Align::Right);
  display::text("Blocks", 14, 198, Font::Body, ink::Dim);
  display::text(String(s.blocks), 158, 198, Font::Body, s.blocks ? ink::Ok : ink::Fg, Align::Right);
  display::text(display::fit(miner::pool(), 150, Font::Label), CX, 220, Font::Label, ink::Dim, Align::Center);

  // A quién va el premio, sin engañar: de fábrica es para Bitronics
  if (miner::defaultAddress())
    centeredLines("A block now goes to Bitronics. Set your address in badge.local.", 242, 154, 3, 16, ink::Soft);
  else
    centeredLines("Solo mining: a lottery. If you find a block, it is all yours.", 242, 154, 3, 16, ink::Soft);
  display::text(s.running ? "PEER stop - MODE back" : "PEER start - MODE back", CX, 300, Font::Body, ink::Dim, Align::Center);
}

// ============================================================ WITHDRAW

void withdraw(const String& accessUrl, int32_t balanceSats, bool showQr) {
  header("WITHDRAW", ink::Gold);
  display::text(balanceSats >= 0 ? thousands(balanceSats) + " sats" : String("-- sats"), CX, 24, Font::Title, ink::Gold, Align::Center);
  if (!showQr) {
    display::roundRect(6, 64, 158, 120, rgb(0x2A1416));
    centeredLines("This QR opens your badge wallet.", 70, 146, 2, 16, ink::Red);
    centeredLines("Anyone who scans it can spend your sats. Scan it only with your own phone.",
                  106, 146, 5, 16, rgb(0xE6A5A5));
    display::roundRect(18, 204, 134, 24, ink::Gold);
    display::text("PEER  show QR", CX, 208, Font::Body, ink::Bg, Align::Center);
    display::text("MODE  back", CX, 240, Font::Body, ink::Dim, Align::Center);
    return;
  }
  display::qr(accessUrl, CX, 60, 166);
  centeredLines("Open it on your phone and send the sats to your own wallet.", 236, 154, 3, 16, ink::Soft);
  display::text("PEER hide - MODE back", CX, 296, Font::Body, ink::Dim, Align::Center);
}

// ============================================================ PEER CERCANO

void nearbyBanner(const nearby::Neighbor& n, bool alreadyPeer) {
  const uint16_t accent = alreadyPeer ? ink::Ok : ink::Nostr;
  display::roundRect(4, 262, 162, 44, accent);
  display::roundRect(5, 263, 160, 42, ink::Card2);
  display::rect(5, 265, 3, 38, accent);
  display::rect(14, 268, 32, 32, ink::Pattern);
  avatars::draw(n.role, 14, 268, 1);
  display::text("NEARBY", 52, 267, Font::Label, ink::Dim);
  display::text(display::fit(n.name, 108), 52, 276, Font::Body, ink::Fg);
  display::text(alreadyPeer ? "ALREADY PEERS" : "PEER TO CONNECT", 52, 294, Font::Label, accent);
}

void invite(const nearby::Neighbor& from, bool alreadyPeer, uint32_t t) {
  header("INVITE", ink::Nostr);
  // Ondas que salen del otro, como en la búsqueda
  for (int k = 0; k < 3; k++) {
    const float phase = fmodf(t / 900.0f + k / 3.0f, 1.0f);
    display::canvas().drawCircle(CX, 92, 40 + phase * 50, rgb(leds::blend(0xA78BFA, 0x0C0E13, phase * 255)));
  }
  avatars::card(from.role, 53, 56, 2);
  display::name(from.name, CX, 140, 158);
  display::chip(roleInfo(from.role).name, CX, 170, roleInfo(from.role).color);
  centeredLines(alreadyPeer ? "Wants to say hi again" : "Wants to be your peer", 192, 154, 2, 16, ink::Soft);
  display::roundRect(18, 236, 134, 24, ink::Nostr);
  display::text("PEER  accept", CX, 240, Font::Body, ink::Bg, Align::Center);
  display::text("MODE  ignore", CX, 272, Font::Body, ink::Dim, Align::Center);
}

// ============================================================ PRUEBA DE PLACA

// Posición de cada LED en la placa (mm, de BTCBadge.kicad_pcb), para dibujar el mapa
static const float LED_MM[LED_COUNT][2] = {
  {126.50, 145.00}, {126.50, 139.38}, {126.50, 133.75}, {126.50, 128.12}, {126.50, 122.50},
  {132.12, 122.50}, {136.75, 124.00}, {138.50, 128.12}, {136.75, 132.25}, {132.12, 133.75},
  {139.25, 135.62}, {140.25, 139.38}, {138.38, 143.50}, {132.12, 145.00}, {133.50, 149.50},
  {128.25, 149.50}, {128.25, 118.00}, {133.50, 118.00}, {152.50, 110.50}, {157.00, 110.50}};

static void buttonBox(const char* name, bool down, int x, int y) {
  display::roundRect(x, y, 30, 18, down ? ink::Orange : ink::Card);
  display::text(name, x + 15, y + 5, Font::Label, down ? ink::Bg : ink::Dim, Align::Center);
}

void hardwareTest(const HardwareTestInfo& info) {
  header("TEST PLACA", ink::Orange);

  // Botones: se encienden mientras están pulsados
  buttonBox("PEER", buttons::isDown(Button::Peer), 4, 24);
  buttonBox("MODE", buttons::isDown(Button::Mode), 37, 24);
  buttonBox("SEED", buttons::isDown(Button::Seed), 70, 24);
  buttonBox("BOOT", buttons::isBootDown(), 103, 24);
  buttonBox("KEY", buttons::isKeyDown(), 136, 24);
  display::text(String("Evento: ") + buttons::eventName(info.lastEvent), 6, 46, Font::Body, ink::Soft);

  // Mapa de los LEDs con el color que tienen ahora mismo
  display::roundRect(6, 66, 158, 140, ink::Card);
  for (int i = 0; i < LED_COUNT; i++) {
    const int x = 30 + (int)((LED_MM[i][0] - 126.5f) * 3.4f);
    const int y = 78 + (int)((LED_MM[i][1] - 110.5f) * 2.5f);
    const Color c = leds::get(i);
    display::canvas().drawCircle(x, y, 5, ink::Line);
    if (c) display::canvas().fillCircle(x, y, 4, rgb(c));
  }
  display::text("TX  RX", 126, 87, Font::Label, ink::Dim, Align::Center);
  display::text(info.ledStep, 14, 186, Font::Body, info.paused ? ink::Orange : ink::Fg);

  // Medidas
  display::roundRect(6, 212, 158, 72, ink::Card);
  char line[40];
  snprintf(line, sizeof line, "USB      %u.%02u V", info.usbMv / 1000, info.usbMv % 1000 / 10);
  display::text(line, 14, 216, Font::Body, ink::Soft);
  snprintf(line, sizeof line, "Bateria  %u.%02u V", info.batteryMv / 1000, info.batteryMv % 1000 / 10);
  display::text(line, 14, 232, Font::Body, ink::Soft);
  snprintf(line, sizeof line, "LEDs     ~%u mA", info.ledMa);
  display::text(line, 14, 248, Font::Body, ink::Soft);
  String q = "Qwiic   ";
  if (info.i2cCount == 0) q += " nada";
  for (int i = 0; i < info.i2cCount && i < 3; i++) {
    char a[6];
    snprintf(a, sizeof a, " %02X", info.i2c[i]);
    q += a;
  }
  display::text(q, 14, 264, Font::Body, ink::Soft);

  display::text(info.paused ? "PEER sigue - MODE paso" : "PEER pausa", CX, 288, Font::Body, ink::Dim, Align::Center);
  display::text("SEED larga: Salir", CX, 304, Font::Body, ink::Dim, Align::Center);
}

}  // namespace screens
