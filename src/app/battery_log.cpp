#include "battery_log.h"
#include "../hardware/power.h"
#include "../net/radio.h"
#include "../net/miner.h"
#include <LittleFS.h>
#include <Preferences.h>
#include <sys/stat.h>

namespace batterylog {

static const char*    FILE_NOW  = "/batlog.csv";
static const char*    FILE_OLD  = "/batlog_old.csv";
static const uint32_t EVERY_MS  = 30000;
static const size_t   MAX_BYTES = 96 * 1024;   // ~2.000 líneas, unas 16 h; luego pasa a _old y empieza otro

static uint32_t bootId = 0;

// exists() de esta versión se queja por el puerto cuando el fichero no
// está (lo abre para mirarlo); stat() pregunta sin abrir y no dice nada
static bool exists(const char* name) {
  struct stat st;
  return stat((String("/littlefs") + name).c_str(), &st) == 0;
}
static uint32_t lastAt = 0;

// La batería se lee varias veces y se hace la media: con la radio y la pantalla
// el ADC baila unas decenas de mV
static uint16_t averagedBattery() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += power::batteryMillivolts();
  return sum / 8;
}

void begin() {
  Preferences prefs;
  prefs.begin("badge", false);
  bootId = prefs.getULong("boot_id", 0) + 1;
  prefs.putULong("boot_id", bootId);
  prefs.end();
}

void loop(const char* stateName) {
  if (lastAt != 0 && millis() - lastAt < EVERY_MS) return;
  lastAt = millis();

  File f = LittleFS.open(FILE_NOW, FILE_APPEND);
  if (!f) return;
  if (f.size() > MAX_BYTES) {                     // lleno: lo de antes a _old y uno nuevo
    f.close();
    if (exists(FILE_OLD)) LittleFS.remove(FILE_OLD);
    LittleFS.rename(FILE_NOW, FILE_OLD);
    f = LittleFS.open(FILE_NOW, FILE_APPEND);
    if (!f) return;
  }
  static const char* const RADIO[] = {"exclusiva", "sinwifi", "conectando", "online"};
  f.printf("%lu,%lu,%u,%u,%s,%s,%d\n", (unsigned long)bootId, (unsigned long)(millis() / 1000), averagedBattery(),
           power::usbMillivolts(), stateName, RADIO[(int)radio::status()], miner::running() ? 1 : 0);
  f.close();
}

void dump() {
  Serial.println("BATLOG arranque,segundos,bateria_mV,usb_mV,estado,radio,minando");
  for (const char* name : {FILE_OLD, FILE_NOW}) {
    if (!exists(name)) continue;   // (sin esto, el sistema de ficheros se queja por el puerto)
    File f = LittleFS.open(name, "r");
    if (!f) continue;
    while (f.available()) Serial.write(f.read());
    f.close();
  }
  Serial.println("BATLOG END");
}

void clear() {
  for (const char* name : {FILE_NOW, FILE_OLD})
    if (exists(name)) LittleFS.remove(name);
  Serial.println("Registro de batería borrado");
}

}  // namespace batterylog
