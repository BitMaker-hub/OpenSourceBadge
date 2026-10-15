#include "storage.h"
#include <Preferences.h>
#include <LittleFS.h>

namespace storage {

static Preferences prefs;
static const char* PEERS_FILE = "/peers.bin";

// Cómo va un peer en el fichero: tamaño fijo, para leer y escribir de golpe
struct PeerRecord {
  char     name[NAME_LEN + 1];
  char     handle[HANDLE_LEN + 1];
  char     npub[NPUB_LEN + 1];
  char     lightning[LIGHTNING_LEN + 1];
  uint8_t  role;
  uint8_t  mac[6];
  uint16_t number;
  uint32_t block;
  uint32_t when;
};

static void copy(char* dst, const String& src, size_t size) {
  strncpy(dst, src.c_str(), size - 1);
  dst[size - 1] = 0;
}

void begin() {
  prefs.begin("badge", false);
  if (!LittleFS.begin(true)) Serial.println("[storage] no se pudo montar LittleFS");
}

bool loadProfile(Profile& p) {
  if (!prefs.isKey("name")) return false;
  p.name      = prefs.getString("name");
  p.handle    = prefs.getString("handle");
  p.npub      = prefs.getString("npub");
  p.lightning = prefs.getString("ln");
  p.tagline   = prefs.getString("tagline");
  p.role      = (Role)prefs.getUChar("role", (uint8_t)Role::Anon);
  return true;
}

void saveProfile(const Profile& p) {
  prefs.putString("name", p.name);
  prefs.putString("handle", p.handle);
  prefs.putString("npub", p.npub);
  prefs.putString("ln", p.lightning);
  prefs.putString("tagline", p.tagline);
  prefs.putUChar("role", (uint8_t)p.role);
}

uint16_t loadNumber()            { return prefs.getUShort("number", 0); }
void     saveNumber(uint16_t n)  { prefs.putUShort("number", n); }
int16_t  loadTimezone()          { return prefs.getShort("tz", 0); }
void     saveTimezone(int16_t m) { prefs.putShort("tz", m); }

uint32_t loadBlockHeight()       { return prefs.getULong("block", 0); }
void     saveBlockHeight(uint32_t h) { prefs.putULong("block", h); }
uint32_t loadSats()              { return prefs.getULong("sats", 0); }
void     saveSats(uint32_t s)    { prefs.putULong("sats", s); }

int loadPeers(Peer* out, int max) {
  File f = LittleFS.open(PEERS_FILE, "r");
  if (!f) return 0;
  int n = 0;
  PeerRecord r;
  while (n < max && f.read((uint8_t*)&r, sizeof r) == sizeof r) {
    Peer& p = out[n++];
    p.name = r.name;  p.handle = r.handle;  p.npub = r.npub;  p.lightning = r.lightning;
    p.role = (Role)r.role;  memcpy(p.mac, r.mac, 6);
    p.number = r.number;  p.block = r.block;  p.when = r.when;
  }
  f.close();
  return n;
}

void savePeers(const Peer* peers, int count) {
  File f = LittleFS.open(PEERS_FILE, "w");
  if (!f) return;
  for (int i = 0; i < count; i++) {
    PeerRecord r = {};
    const Peer& p = peers[i];
    copy(r.name, p.name, sizeof r.name);
    copy(r.handle, p.handle, sizeof r.handle);
    copy(r.npub, p.npub, sizeof r.npub);
    copy(r.lightning, p.lightning, sizeof r.lightning);
    r.role = (uint8_t)p.role;  memcpy(r.mac, p.mac, 6);
    r.number = p.number;  r.block = p.block;  r.when = p.when;
    f.write((const uint8_t*)&r, sizeof r);
  }
  f.close();
}

void factoryReset() {
  // El número de badge no se borra: es de fábrica
  const uint16_t number = prefs.getUShort("number", 0);
  prefs.clear();
  if (number) prefs.putUShort("number", number);
  LittleFS.remove(PEERS_FILE);
}

}  // namespace storage
