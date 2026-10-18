#include "badge.h"
#include "storage.h"
#include "../ui/display.h"
#include <esp_system.h>

static const RoleInfo ROLES[] = {
  {"DEV",      rgb(0x38BDF8)},
  {"MINER",    rgb(0xFACC15)},
  {"PLEB",     rgb(0xF7931A)},
  {"WHALE",    rgb(0xE5E7EB)},
  {"NOSTRICH", rgb(0xA78BFA)},
  {"ANON",     rgb(0x9CA3AF)},
};

const RoleInfo& roleInfo(Role r) { return ROLES[(int)r < (int)Role::Count ? (int)r : (int)Role::Anon]; }

Role roleFromName(const String& name) {
  for (int r = 0; r < (int)Role::Count; r++)
    if (name.equalsIgnoreCase(ROLES[r].name)) return (Role)r;
  return Role::Anon;
}

namespace badge {

static uint16_t serial = 0;
static Profile  profile;
static bool     configured = false;
static Peer     peers[MAX_PEERS];     // del más antiguo al más reciente
static int      nPeers = 0;
static uint32_t block = 0;
static uint32_t sats = 0;
static bool     demo = false;         // con datos de demo no se guarda nada

// ============================================================ arranque

static void defaultProfile() {
  profile = Profile();
  profile.name = "Badge #" + String(serial);
  profile.tagline = "Hold MODE to set me up";
  profile.role = Role::Anon;
}

void begin() {
  storage::begin();

  serial = storage::loadNumber();
  if (serial == 0) {
    // Sin número de fábrica: sacamos uno de la MAC, para que al menos no salgan todos iguales
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    serial = (mac[4] << 8 | mac[5]) % 1000;
  }

  configured = storage::loadProfile(profile);
  if (!configured) defaultProfile();

  nPeers = storage::loadPeers(peers, MAX_PEERS);
  block = storage::loadBlockHeight();
  sats = storage::loadSats();
  setTimezone(storage::loadTimezone());
  Serial.printf("[badge] #%03u %s, %d peers, bloque %lu\n", serial, profile.name.c_str(), nPeers, (unsigned long)block);
}

// ============================================================ yo

uint16_t number()          { return serial; }
void setNumber(uint16_t n) { serial = n; storage::saveNumber(n); }
Profile& me()              { return profile; }
bool isConfigured()        { return configured; }

void saveMe() {
  configured = true;
  if (!demo) storage::saveProfile(profile);
}

String wifiName() {
  char ssid[16];
  snprintf(ssid, sizeof ssid, "BADGE-%03u", serial);
  return ssid;
}

// ============================================================ peers

int peerCount() { return nPeers; }
const Peer& peer(int i) { return peers[nPeers - 1 - i]; }

static bool sameBadge(const Peer& a, const Peer& b) {
  static const uint8_t ZERO[6] = {};
  return memcmp(a.mac, ZERO, 6) != 0 && memcmp(a.mac, b.mac, 6) == 0;
}

void addPeer(const Peer& p) {
  // Si ya lo conocíamos, lo quitamos de donde estaba: vuelve a ser el más reciente
  for (int i = 0; i < nPeers; i++) {
    if (sameBadge(peers[i], p)) {
      for (int j = i + 1; j < nPeers; j++) peers[j - 1] = peers[j];
      nPeers--;
      break;
    }
  }
  if (nPeers == MAX_PEERS) {                          // lleno: se olvida el más antiguo
    for (int i = 1; i < MAX_PEERS; i++) peers[i - 1] = peers[i];
    nPeers--;
  }
  peers[nPeers++] = p;
  setBlockHeight(max(block, p.block));
  if (!demo) storage::savePeers(peers, nPeers);
}

bool knows(const uint8_t* mac) {
  for (int i = 0; i < nPeers; i++) if (memcmp(peers[i].mac, mac, 6) == 0) return true;
  return false;
}

bool metRole(Role r) {
  for (int i = 0; i < nPeers; i++) if (peers[i].role == r) return true;
  return false;
}

// ============================================================ números

uint32_t blockHeight()    { return block; }
uint32_t satsReceived()   { return sats; }

void setBlockHeight(uint32_t h) {
  block = h;
  if (!demo) storage::saveBlockHeight(h);
}

void addSats(uint32_t a) {
  sats += a;
  if (!demo) storage::saveSats(sats);
}

void resetSats() {
  sats = 0;
  if (!demo) storage::saveSats(0);
}

uint8_t decentralization() {
  int roles = 0;
  for (int r = 0; r < (int)Role::Count; r++) if (metRole((Role)r)) roles++;
  return min(99, 60 + roles * 4 + (int)round(log2(block + 1) * 2));
}

String socialHash() { return String(block * 0.033f, 1); }

int qrItems(QrItem* out) {
  int n = 0;
  if (profile.lightning.length())
    out[n++] = {QrKind::Lightning, "LN", "LIGHTNING", "lightning:" + profile.lightning,
                "ZAP ME", profile.lightning, ink::Gold};
  if (profile.npub.length() > 16)
    out[n++] = {QrKind::Nostr, "NOSTR", "NOSTR", "nostr:" + profile.npub, "FOLLOW ON NOSTR",
                profile.npub.substring(0, 10) + ".." + profile.npub.substring(profile.npub.length() - 6), ink::Nostr};
  if (profile.handle.length()) {
    String user = profile.handle;
    user.replace("@", "");
    out[n++] = {QrKind::X, "X", "X", "https://x.com/" + user, "FOLLOW ON X", "@" + user, ink::X};
  }
  if (n == 0)
    out[n++] = {QrKind::Setup, "SETUP", "SETUP", "WIFI:S:" + wifiName() + ";T:nopass;;",
                "SET ME UP", "Hold MODE", ink::Orange};
  return n;
}

// ============================================================ hora

static uint32_t timeBase = 0;         // hora Unix que nos dieron...
static uint32_t timeBaseMillis = 0;   // ...y el millis() de aquel momento
static int32_t  tzSeconds = 0;

bool hasTime() { return timeBase != 0; }

uint32_t now() {
  return timeBase ? timeBase + (millis() - timeBaseMillis) / 1000 : 0;
}

void setTime(uint32_t unixSeconds) {
  timeBase = unixSeconds;
  timeBaseMillis = millis();
}

void setTimezone(int16_t minutes) {
  tzSeconds = minutes * 60;
  storage::saveTimezone(minutes);
}

String clock(uint32_t t) {
  if (t == 0) return "--:--";
  const uint32_t local = t + tzSeconds;
  char hhmm[6];
  snprintf(hhmm, sizeof hhmm, "%02lu:%02lu", (unsigned long)(local / 3600 % 24), (unsigned long)(local / 60 % 60));
  return hhmm;
}

// ============================================================ texto

String clean(const String& s, int max) {
  // Las letras con acento llegan en UTF-8 como dos bytes, el primero 0xC3
  static const char FROM[] = "\xA0\xA1\xA8\xA9\xAD\xB2\xB3\xBA\xBC\xB1\xA7\x81\x89\x8D\x93\x9A\x91\x87";
  static const char TO[]   = "aaeeioouuncAEIOUNC";
  String out;
  bool lastSpace = true;                                // así no empieza por espacio
  for (unsigned i = 0; i < s.length() && (int)out.length() < max; i++) {
    uint8_t c = s[i];
    if (c == 0xC3 && i + 1 < s.length()) {
      const char* hit = strchr(FROM, s[++i]);
      if (hit) { out += TO[hit - FROM]; lastSpace = false; }
      continue;
    }
    if (c >= 0x80) continue;                            // resto de UTF-8 (emojis...): fuera
    if (c < 32) c = ' ';
    if (c == ' ' && lastSpace) continue;                // sin espacios dobles
    out += (char)c;
    lastSpace = c == ' ';
  }
  out.trim();
  return out;
}

// ============================================================ demos

void loadDemo() {
  demo = true;
  serial = 42;
  profile.name      = "satoshi.friends";
  profile.handle    = "@satoshi";
  profile.npub      = "npub1sg6plzptd64u62a878hep2kev88swjh3tw00gjsfl8f237lmu63q0uf63m";
  profile.lightning = "badge042@zaps.bitronics.store";
  profile.tagline   = "Stay humble, stack sats";
  profile.role      = Role::Dev;
  configured = true;

  struct { const char* name; Role role; uint32_t block; } const DEMO[] = {
    {"stacker.sam", Role::Pleb, 36}, {"anon_2140", Role::Anon, 37},    {"whale.watch", Role::Whale, 38},
    {"rustacean", Role::Dev, 39},    {"nostr.nina", Role::Nostrich, 40}, {"asic.andy", Role::Miner, 41},
    {"hodl.queen", Role::Pleb, 42}};
  nPeers = 0;
  for (auto& d : DEMO) {
    Peer p;
    p.name = d.name;  p.role = d.role;  p.block = d.block;  p.mac[5] = d.block;
    addPeer(p);
  }
  sats = 12600;
}

Zap nextDemoZap() {
  static const Zap ZAPS[] = {
    {2100,  "@lisa.btc",  "Nostr",     "Great talk on self-custody!"},
    {500,   "hodl.queen", "Lightning", ""},
    {21000, "@asic.andy", "Nostr",     "For the nerdminer chat"},
  };
  static int next = 0;
  return ZAPS[next++ % 3];
}

void factoryReset() {
  storage::factoryReset();
  nPeers = 0;
  block = 0;
  sats = 0;
  configured = false;
  demo = false;
  defaultProfile();
}

}  // namespace badge
