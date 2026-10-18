#include "miner.h"
#include "radio.h"
#include <Preferences.h>

namespace miner {

static Preferences prefs;
static String   poolHost, btcAddress;
static uint16_t poolPort = DEFAULT_PORT;
static bool     on = false;

static String get(const char* key, const char* def) { return prefs.isKey(key) ? prefs.getString(key) : def; }

void begin() {
  prefs.begin("badge", false);
  poolHost   = get("m_pool", DEFAULT_POOL);
  poolPort   = prefs.getUShort("m_port", DEFAULT_PORT);
  btcAddress = get("m_addr", "");
  if (prefs.getBool("m_on", false)) start();       // estaba minando: seguimos
}

void setConfig(const String& pool, uint16_t port, const String& address) {
  poolHost = pool.length() ? pool : DEFAULT_POOL;
  poolPort = port ? port : DEFAULT_PORT;
  btcAddress = address;
  prefs.putString("m_pool", poolHost);
  prefs.putUShort("m_port", poolPort);
  prefs.putString("m_addr", btcAddress);
  if (on) { stop(); start(); }                      // con lo nuevo
}

String   pool()    { return poolHost; }
uint16_t port()    { return poolPort; }
String   address() { return btcAddress; }
bool     enabled() { return on; }

bool start() {
  if (btcAddress.isEmpty()) return false;
  nerdminer::start({poolHost, poolPort, btcAddress, "x"}, radio::online);
  on = true;
  prefs.putBool("m_on", true);
  Serial.printf("[miner] minando en %s:%u para %s\n", poolHost.c_str(), poolPort, btcAddress.c_str());
  return true;
}

void stop() {
  nerdminer::stop();
  on = false;
  prefs.putBool("m_on", false);
  Serial.println("[miner] parado");
}

nerdminer::Stats stats() { return nerdminer::stats(); }

}  // namespace miner
