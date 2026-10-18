#include "radio.h"
#include "../app/storage.h"
#include <WiFi.h>
#include <esp_wifi.h>

namespace radio {

static const uint32_t RETRY_MS = 20000;   // si no conecta, se vuelve a intentar cada 20 s

static String ssid, password;             // los escribe reloadNetwork() con la radio en exclusivo o al arrancar
static volatile bool   wantExclusive = false;
static volatile bool   isOff = true;      // la tarea confirma que ha soltado la radio
static volatile Status state = Status::NoNetwork;
static volatile bool   reloadPending = false;
static volatile bool   users[(int)User::Count] = {};   // quién tiene conexiones abiertas

static bool anyUser() {
  for (int i = 0; i < (int)User::Count; i++)
    if (users[i]) return true;
  return false;
}

static void task(void*) {
  uint32_t lastTry = 0;
  for (;;) {
    // EXCLUSIVO: soltar la WiFi y apagar
    if (wantExclusive) {
      state = Status::Exclusive;   // online() ya dice que no: cada uno cierra lo suyo
      if (!isOff) {
        // Esperar a que todos hayan cerrado sus conexiones antes de apagar
        const uint32_t start = millis();
        while (anyUser() && millis() - start < 8000) vTaskDelay(pdMS_TO_TICKS(20));
        if (anyUser()) Serial.println("[radio] aviso: alguien no ha cerrado a tiempo, apago igual");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        isOff = true;
      }
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // NORMAL: siempre en modo estación, aunque no haya WiFi (peer cercano lo necesita)
    if (isOff || WiFi.getMode() != WIFI_STA) {
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);       // sin ahorro: si no, se pierden mensajes de ESP-NOW (pendiente de afinar con batería)
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

Status status()   { return state; }
bool online()     { return state == Status::Online; }
bool hasNetwork() { return !ssid.isEmpty(); }

void busy(User who, bool open) { users[(int)who] = open; }

}  // namespace radio
