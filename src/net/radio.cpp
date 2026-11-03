#include "radio.h"
#include "../app/storage.h"
#include "../app/badge.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <time.h>
#include <atomic>

namespace radio {

// Si la WiFi no está al alcance (otro sitio, router apagado), no se puede estar
// buscándola todo el día: un intento dura como mucho 9 s (conecta en ~3 s) y,
// si no sale, se espera 20 s, luego 40, 80, 160 y como mucho 5 min.
static const uint32_t TRY_MS       = 9000;
static const uint32_t BACKOFF_MS   = 20000;
static const uint32_t MAX_WAIT_MS  = 300000;
static uint32_t tryStartedAt = 0;          // 0 = no hay intento en marcha
static uint32_t nextTryAt = 0;
static uint8_t  fails = 0;

// AHORRO (con batería y la pantalla apagada en las páginas de inicio): la radio
// solo se enciende a ratos. Medido en el #980: con la radio siempre encendida
// el badge gasta ~117 mA y la radio es casi todo. Por eso: 10 s
// encendida y 10 s apagada. En cada ventana, como siempre: si hay WiFi
// configurada se conecta (zaps casi al momento; Nostr da al reconectar los que
// llegaron con la radio apagada) y el peer cercano va en el canal de la WiFi;
// si no la hay, el peer cercano va quieto en el canal 1 (sin saltar: con
// ventanas cortas, saltando no coincidirían nunca).
static const uint32_t ECO_PERIOD_MS = 20000;
static const uint32_t ECO_ON_MS     = 10000;
static const uint8_t  ECO_CHANNEL   = 1;

// La WiFi de la última vez (canal y router): reconectar sin buscar redes es
// mucho más rápido, y en las ventanas cada segundo cuenta
static int32_t lastChannel = 0;
static uint8_t lastBssid[6];
static uint32_t windowAt = 0;      // millis() en que se encendió la radio (para medir)
static volatile uint32_t onlineAt = 0;

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
  tryStartedAt = 0;                 // el intento en marcha se corta (no cuenta como fallo)
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
  uint32_t cycleStart = 0;
  for (;;) {
    // EXCLUSIVO: soltar la WiFi y apagar
    if (wantExclusive) {
      state = Status::Exclusive;   // online() ya dice que no: cada uno cierra lo suyo
      if (!isOff) turnOff();
      cycleStart = 0;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // AHORRO: a ratos. Fuera de la ventana, apagada; dentro, como en NORMAL.
    if (wantEco) {
      const uint32_t now = millis();
      if (cycleStart == 0 || now - cycleStart >= ECO_PERIOD_MS) {     // empieza otro ciclo
        cycleStart = now;
      }
      if (now - cycleStart >= ECO_ON_MS) {                             // fase dormida
        if (!isOff) {
          state = Status::Sleeping;
          turnOff();
        }
        state = Status::Sleeping;
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
      }
      // Ventana sin WiFi configurada: el peer cercano, quieto en el canal 1
      if (ssid.isEmpty() && !isOff && WiFi.getMode() == WIFI_STA) {
        uint8_t ch; wifi_second_chan_t second;
        esp_wifi_get_channel(&ch, &second);
        if (ch != ECO_CHANNEL) esp_wifi_set_channel(ECO_CHANNEL, WIFI_SECOND_CHAN_NONE);
      }
    } else {
      cycleStart = 0;
    }

    // NORMAL: siempre en modo estación, aunque no haya WiFi (peer cercano lo necesita)
    if (isOff || WiFi.getMode() != WIFI_STA) {
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);       // sin ahorro del módem: si no, se pierden mensajes de ESP-NOW
      isOff = false;
      windowAt = millis();
      if (wantEco && ssid.isEmpty()) esp_wifi_set_channel(ECO_CHANNEL, WIFI_SECOND_CHAN_NONE);
    }
    if (reloadPending) {
      WiFi.disconnect();
      lastChannel = 0;            // otra WiFi: nada de atajos
      tryStartedAt = 0;
      fails = 0;
      nextTryAt = 0;
      reloadPending = false;
    }

    if (ssid.isEmpty()) {
      state = Status::NoNetwork;
    } else if (WiFi.status() == WL_CONNECTED) {
      if (state != Status::Online) {
        onlineAt = millis();
        lastChannel = WiFi.channel();
        memcpy(lastBssid, WiFi.BSSID(), 6);
        Serial.printf("[radio] WiFi conectada en %lu ms (canal %ld)\n", (unsigned long)(onlineAt - windowAt), (long)lastChannel);
      }
      tryStartedAt = 0;
      fails = 0;
      nextTryAt = 0;
      state = Status::Online;
      syncTime();
    } else {
      const uint32_t now = millis();
      if (tryStartedAt && now - tryStartedAt > TRY_MS) {            // este intento no ha salido
        WiFi.disconnect();
        tryStartedAt = 0;
        lastChannel = 0;                                            // ¿cambió el router? la próxima, buscando
        fails = min(fails + 1, 5);
        nextTryAt = now + min(BACKOFF_MS << (fails - 1), MAX_WAIT_MS);
        Serial.printf("[radio] la WiFi no conecta; siguiente intento en %lu s\n", (unsigned long)((nextTryAt - now) / 1000));
      }
      if (!tryStartedAt && (int32_t)(now - nextTryAt) >= 0) {
        // Con el canal y el router de la última vez no hace falta buscar redes
        if (lastChannel) WiFi.begin(ssid.c_str(), password.c_str(), lastChannel, lastBssid);
        else             WiFi.begin(ssid.c_str(), password.c_str());
        tryStartedAt = windowAt = now;
      }
      state = tryStartedAt ? Status::Connecting : Status::NoNetwork;   // esperando: como si no hubiera WiFi
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
uint32_t onlineSince() { return state == Status::Online ? onlineAt : 0; }
bool online()     { return state == Status::Online; }
bool hasNetwork() { return !ssid.isEmpty(); }

void busy(User who, bool open) { users[(int)who] = open; }

static std::atomic<int> handshakes{0};   // las tareas de red están todas en el núcleo 0 y se pisan
Handshake::Handshake()  { handshakes++; }
Handshake::~Handshake() { handshakes--; }
bool handshaking() { return handshakes > 0; }

}  // namespace radio
