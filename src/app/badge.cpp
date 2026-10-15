#include "badge.h"
#include "../ui/display.h"

static const RoleInfo ROLES[] = {
  {"DEV",      rgb(0x38BDF8)},
  {"MINER",    rgb(0xFACC15)},
  {"PLEB",     rgb(0xF7931A)},
  {"WHALE",    rgb(0xE5E7EB)},
  {"NOSTRICH", rgb(0xA78BFA)},
  {"ANON",     rgb(0x9CA3AF)},
};

const RoleInfo& roleInfo(Role r) { return ROLES[(int)r]; }

namespace badge {

static Profile  profile;
static Peer     peers[MAX_PEERS];
static int      nPeers = 0;
static uint32_t block = 42;
static uint32_t sats = 12600;

void loadDemo() {
  profile.number    = 42;
  profile.name      = "satoshi.friends";
  profile.handle    = "@satoshi";
  profile.npub      = "npub1sg6plzptd64u62a878hep2kev88swjh3tw00gjsfl8f237lmu63q0uf63m";
  profile.lightning = "badge042@zaps.bitronics.store";
  profile.tagline   = "Stay humble, stack sats";
  profile.role      = Role::Dev;

  // Del más antiguo al más reciente
  const Peer demo[] = {
    {"stacker.sam", Role::Pleb,     36, "12:22"},
    {"anon_2140",   Role::Anon,     37, "12:40"},
    {"whale.watch", Role::Whale,    38, "12:55"},
    {"rustacean",   Role::Dev,      39, "13:10"},
    {"nostr.nina",  Role::Nostrich, 40, "13:31"},
    {"asic.andy",   Role::Miner,    41, "13:48"},
    {"hodl.queen",  Role::Pleb,     42, "14:02"},
  };
  nPeers = 0;
  for (const Peer& p : demo) addPeer(p);
}

Profile& me()         { return profile; }
int peerCount()       { return nPeers; }
uint32_t blockHeight(){ return block; }
uint32_t satsReceived(){ return sats; }
void addSats(uint32_t a) { sats += a; }

// Los peers se guardan del más antiguo al más reciente; peer(0) da el último
const Peer& peer(int i) { return peers[nPeers - 1 - i]; }

void addPeer(const Peer& p) {
  if (nPeers == MAX_PEERS) {                          // lleno: se olvida el más antiguo
    for (int i = 1; i < MAX_PEERS; i++) peers[i - 1] = peers[i];
    nPeers--;
  }
  peers[nPeers++] = p;
  block = max(block, p.block);
}

bool metRole(Role r) {
  for (int i = 0; i < nPeers; i++) if (peers[i].role == r) return true;
  return false;
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
  if (profile.npub.length())
    out[n++] = {QrKind::Nostr, "NOSTR", "NOSTR", "nostr:" + profile.npub, "FOLLOW ON NOSTR",
                profile.npub.substring(0, 10) + ".." + profile.npub.substring(profile.npub.length() - 6), ink::Nostr};
  if (profile.handle.length()) {
    String user = profile.handle;
    user.replace("@", "");
    out[n++] = {QrKind::X, "X", "X", "https://x.com/" + user, "FOLLOW ON X", profile.handle, ink::X};
  }
  if (n == 0)
    out[n++] = {QrKind::Setup, "SETUP", "SETUP", "http://badge.local", "SET ME UP", "hold MODE", ink::Orange};
  return n;
}

Peer randomDemoPeer() {
  static const Peer POOL[] = {
    {"lisa.btc", Role::Dev, 0, ""},       {"blockrunner", Role::Miner, 0, ""},
    {"orange.pill", Role::Pleb, 0, ""},   {"zap.zoe", Role::Nostrich, 0, ""},
    {"sats.sage", Role::Whale, 0, ""},    {"cypherpunk", Role::Anon, 0, ""},
  };
  Peer p = POOL[random(6)];
  p.block = block + 1;
  const uint32_t minutes = millis() / 60000;          // sin reloj todavía: tiempo desde que se encendió
  char hhmm[6];
  snprintf(hhmm, sizeof hhmm, "%02lu:%02lu", (unsigned long)(minutes / 60 % 24), (unsigned long)(minutes % 60));
  p.time = hhmm;
  return p;
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

}  // namespace badge
