#include "radio.h"
#include "../app/storage.h"
#include "../app/badge.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <time.h>

namespace radio {

static const uint32_t RETRY_MS = 20000;   // si no conecta, se vuelve a intentar cada 20 s

// AHORRO (con batería y la pantalla apagada en las páginas de inicio): la radio
// solo se enciende a ratos. Medido en el #980: con la radio siempre encendida
// el badge gasta ~117 mA y la radio es casi todo.
//   - cada 30 s, 10 s de radio para el peer cercano (todos en el canal 1, sin
//     conectar a la WiFi: dos badges pegados coinciden en un minuto o dos)
//   - cada 5 min, en vez de esa ventana, 25 s conectado a la WiFi para recoger
//     los zaps (Nostr los da al reconectar: llegan tarde pero no se pierden)
static const uint32_t ECO_PERIOD_MS     = 30000;
static const uint32_t ECO_ON_MS         = 10000;
static const uint32_t ECO_ZAPS_EVERY_MS = 300000;
static const uint32_t ECO_ZAPS_ON_MS    = 25000;
static const uint8_t  ECO_CHANNEL       = 1;

static String ssid, password;             // los escribe reloadNetwork() con la radio en exclusivo o al arrancar
static volatile bool   wantExclusive = false;
static volatile bool   wantEco = false;
static volatile bool   isOff = true;      // la tarea confirma que ha soltado la radio
static volatile Status state = Status::NoNetwork;
static volatile bool   reloadPending = false;
static volatile bool   users[(int)User::Count] = {};   // quién tiene conexiones abiertas

static bool anyUser() {
  for (int i = 0; i < (int)User::Count; i++)
    if (users[i]) return true;
  return false;
}

// Antes de apagar o de soltar la WiFi, que cada uno cierre sus conexiones (si
// no, al cerrarlas tarde liberan buffers que el driver ya soltó y el chip se
// reinicia). Quien mire status() ya ve que no hay internet.
static void waitUsers() {
  const uint32_t start = millis();
  while (anyUser() && millis() - start < 8000) vTaskDelay(pdMS_TO_TICKS(20));
  if (anyUser()) Serial.println("[radio] aviso: alguien no ha cerrado a tiempo, sigo igual");
}

static void turnOff() {
  waitUsers();
  vTaskDelay(pdMS_TO_TICKS(300));   // y que el peer cercano (en el bucle principal) suelte ESP-NOW
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  isOff = true;
}

// Con internet y sin hora: se la pedimos a un servidor de hora (la usa Nostr
// para pedir los zaps que llegaron mientras la radio estaba apagada)
static void syncTime() {
  static bool asked = false;
  if (badge::hasTime()) return;
  if (!asked) { configTime(0, 0, "pool.ntp.org", "time.google.com"); asked = true; }
  const time_t t = time(nullptr);
  if (t > 1700000000) badge::setTime(t);
}

static void task(void*) {
  uint32_t lastTry = 0;
  uint32_t cycleStart = 0, lastZapCycle = 0;
  bool     zapCycle = false;
  for (;;) {
    // EXCLUSIVO: soltar la WiFi y apagar
    if (wantExclusive) {
      state = Status::Exclusive;   // online() ya dice que no: cada uno cierra lo suyo
      if (!isOff) turnOff();
      cycleStart = 0;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // AHORRO: a ratos
    if (wantEco) {
      const uint32_t now = millis();
      if (cycleStart == 0 || now - cycleStart >= ECO_PERIOD_MS) {     // empieza otro ciclo
        cycleStart = now;
        zapCycle = !ssid.isEmpty() && (lastZapCycle == 0 || now - lastZapCycle >= ECO_ZAPS_EVERY_MS);
        if (zapCycle) lastZapCycle = now;
        lastTry = 0;
      }
      if (now - cycleStart >= (zapCycle ? ECO_ZAPS_ON_MS : ECO_ON_MS)) {   // fase dormida
        if (!isOff) {
          state = Status::Sleeping;
          turnOff();
        }
        state = Status::Sleeping;
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
      }
      if (!zapCycle) {                                                     // ventana del peer cercano
        if (WiFi.getMode() == WIFI_STA && WiFi.isConnected()) {
          state = Status::NoNetwork;   // que Nostr y compañía suelten antes de desconectar
          waitUsers();
          WiFi.disconnect();
        }
        if (isOff || WiFi.getMode() != WIFI_STA) {
          WiFi.mode(WIFI_STA);
          WiFi.setSleep(false);
          isOff = false;
        }
        esp_wifi_set_channel(ECO_CHANNEL, WIFI_SECOND_CHAN_NONE);
        state = Status::NoNetwork;
        vTaskDelay(pdMS_TO_TICKS(200));
        continue;
      }
      // ventana de los zaps: lo mismo que en NORMAL, abajo
    } else {
      cycleStart = 0;
    }

    // NORMAL: siempre en modo estación, aunque no haya WiFi (peer cercano lo necesita)
    if (isOff || WiFi.getMode() != WIFI_STA) {
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);       // sin ahorro del módem: si no, se pierden mensajes de ESP-NOW
      isOff = false;
      lastTry = 0;
    }
    if (reloadPending) {
      WiFi.disconnect();
      lastTry = 0;
      reloadPending = false;
    }

    if (ssid.isEmpty()) {
      state = Status::NoNetwork;
    } else if (WiFi.status() == WL_CONNECTED) {
      state = Status::Online;
      syncTime();
    } else {
      if (lastTry == 0 || millis() - lastTry > RETRY_MS) {
        WiFi.begin(ssid.c_str(), password.c_str());
        lastTry = millis();
      }
      state = Status::Connecting;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void begin() {
  storage::loadNetwork(ssid, password);
  xTaskCreatePinnedToCore(task, "radio", 4096, nullptr, 1, nullptr, 0);
}

void reloadNetwork() {
  storage::loadNetwork(ssid, password);
  reloadPending = true;
}

void exclusive(bool on) {
  wantExclusive = on;
  if (on) {
    const uint32_t start = millis();
    while (!isOff && millis() - start < 10000) delay(10);
  }
}

void setEco(bool on) {
  if (on != wantEco) Serial.printf("[radio] ahorro %s\n", on ? "SI (a ratos)" : "no (siempre)");
  wantEco = on;
}
bool eco() { return wantEco; }

Status status()   { return state; }
bool online()     { return state == Status::Online; }
bool hasNetwork() { return !ssid.isEmpty(); }

void busy(User who, bool open) { users[(int)who] = open; }

}  // namespace radio
