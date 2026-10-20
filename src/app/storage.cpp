#include "storage.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <unistd.h>

namespace storage {

static Preferences prefs;
static const char* PEERS_FILE    = "/peers2.bin";   // con Telegram y PGP
static const char* PEERS_FILE_V1 = "/peers.bin";    // el de antes: se lee una vez y se pasa al nuevo

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
  char     telegram[TELEGRAM_LEN + 1];
  char     pgp[PGP_LEN + 1];
};
// LittleFS.exists() de esta versión se queja por el puerto si no está; access() no
static bool fileExists(const char* name) { return access((String("/littlefs") + name).c_str(), F_OK) == 0; }

// La parte de delante es igual que la versión 1: un registro viejo es el principio de uno nuevo
static const size_t RECORD_V1_SIZE = offsetof(PeerRecord, telegram);

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
  p.telegram  = prefs.isKey("tg") ? prefs.getString("tg") : "";    // isKey: si no, Preferences se queja
  p.pgp       = prefs.isKey("pgp") ? prefs.getString("pgp") : "";
  p.role      = (Role)prefs.getUChar("role", (uint8_t)Role::Anon);
  return true;
}

void saveProfile(const Profile& p) {
  prefs.putString("name", p.name);
  prefs.putString("handle", p.handle);
  prefs.putString("npub", p.npub);
  prefs.putString("ln", p.lightning);
  prefs.putString("tagline", p.tagline);
  prefs.putString("tg", p.telegram);
  prefs.putString("pgp", p.pgp);
  prefs.putUChar("role", (uint8_t)p.role);
}

uint16_t loadNumber()            { return prefs.getUShort("number", 0); }
void     saveNumber(uint16_t n)  { prefs.putUShort("number", n); }
int16_t  loadTimezone()          { return prefs.getShort("tz", 0); }
void     saveTimezone(int16_t m) { prefs.putShort("tz", m); }

void loadNetwork(String& ssid, String& password) {
  // isKey antes de leer: si no existe, Preferences llena el puerto serie de errores
  ssid = prefs.isKey("net_ssid") ? prefs.getString("net_ssid") : "";
  password = prefs.isKey("net_pass") ? prefs.getString("net_pass") : "";
}

void saveNetwork(const String& ssid, const String& password) {
  prefs.putString("net_ssid", ssid);
  prefs.putString("net_pass", password);
}

uint32_t loadBlockHeight()       { return prefs.getULong("block", 0); }
void     saveBlockHeight(uint32_t h) { prefs.putULong("block", h); }
uint32_t loadSats()              { return prefs.getULong("sats", 0); }
void     saveSats(uint32_t s)    { prefs.putULong("sats", s); }

int loadPeers(Peer* out, int max) {
  // Si solo está el fichero de antes, se lee con su tamaño de registro (sin Telegram ni PGP)
  const bool v1 = !fileExists(PEERS_FILE) && fileExists(PEERS_FILE_V1);
  File f = LittleFS.open(v1 ? PEERS_FILE_V1 : PEERS_FILE, "r");
  if (!f) return 0;
  const size_t size = v1 ? RECORD_V1_SIZE : sizeof(PeerRecord);
  int n = 0;
  PeerRecord r;
  while (n < max && (memset(&r, 0, sizeof r), f.read((uint8_t*)&r, size) == size)) {
    Peer& p = out[n++];
    p.name = r.name;  p.handle = r.handle;  p.npub = r.npub;  p.lightning = r.lightning;
    p.role = (Role)r.role;  memcpy(p.mac, r.mac, 6);
    p.number = r.number;  p.block = r.block;  p.when = r.when;
    p.telegram = r.telegram;  p.pgp = r.pgp;
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
    copy(r.telegram, p.telegram, sizeof r.telegram);
    copy(r.pgp, p.pgp, sizeof r.pgp);
    f.write((const uint8_t*)&r, sizeof r);
  }
  f.close();
  if (fileExists(PEERS_FILE_V1)) LittleFS.remove(PEERS_FILE_V1);   // ya está todo en el nuevo
}

void factoryReset() {
  // El número de badge no se borra: es de fábrica
  const uint16_t number = prefs.getUShort("number", 0);
  prefs.clear();
  if (number) prefs.putUShort("number", number);
  if (fileExists(PEERS_FILE)) LittleFS.remove(PEERS_FILE);
  if (fileExists(PEERS_FILE_V1)) LittleFS.remove(PEERS_FILE_V1);
}

}  // namespace storage
