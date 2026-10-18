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
  if (poolHost == "public-pool.io" && poolPort == 21496) {   // el de fábrica de antes: al nuevo
    poolHost = DEFAULT_POOL;
    poolPort = DEFAULT_PORT;
  }
}

void setConfig(const String& pool, uint16_t port, const String& address) {
  poolHost = pool.length() ? pool : DEFAULT_POOL;
  poolPort = port ? port : DEFAULT_PORT;
  btcAddress = address;
  prefs.putString("m_pool", poolHost);
  prefs.putUShort("m_port", poolPort);
  prefs.putString("m_addr", btcAddress);
  if (on) { run(false); run(true); }                // con lo nuevo
}

String   pool()    { return poolHost; }
uint16_t port()    { return poolPort; }
String   ownAddress()     { return btcAddress; }
bool     defaultAddress() { return btcAddress.isEmpty(); }
String   address()        { return btcAddress.isEmpty() ? String(DEFAULT_ADDRESS) : btcAddress; }

// Lo que va en el usuario de stratum: dirección.worker (si ya trae worker, se respeta)
static String stratumUser() {
  const String a = address();
  return a.indexOf('.') >= 0 ? a : a + "." + WORKER;
}

void run(bool want) {
  if (want == on) return;
  on = want;
  if (on) {
    nerdminer::start({poolHost, poolPort, stratumUser(), "x"}, radio::online,
                     [](bool open) { radio::busy(radio::User::Miner, open); });
    Serial.printf("[miner] minando en %s:%u como %s\n", poolHost.c_str(), poolPort, stratumUser().c_str());
  } else {
    nerdminer::stop();
    Serial.println("[miner] parado");
  }
}

bool running() { return on; }

nerdminer::Stats stats() { return nerdminer::stats(); }

}  // namespace miner
