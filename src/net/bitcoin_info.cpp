#include "bitcoin_info.h"
#include "radio.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

namespace btcinfo {

static const char*    HOST = "https://mempool.space";
static const uint32_t EVERY_MS = 60000;   // los datos cambian como mucho cada bloque: con un minuto sobra

static volatile bool wanted = false;
static Info          info;                // lo escribe la tarea, lo lee la pantalla
static portMUX_TYPE  lock = portMUX_INITIALIZER_UNLOCKED;

// Pide una ruta y la deja en el documento JSON. false si falla.
static bool fetch(WiFiClientSecure& tls, const char* path, JsonDocument& doc) {
  HTTPClient http;
  if (!radio::online()) return false;   // la radio se va: no empezar nada nuevo
  http.begin(tls, String(HOST) + path);
  http.setTimeout(3000);
  bool ok = false;
  if (http.GET() == 200) ok = !deserializeJson(doc, http.getStream());
  http.end();
  return ok;
}

static void update() {
  WiFiClientSecure tls;
  tls.setInsecure();            // solo son datos públicos para enseñar: no hace falta comprobar el certificado
  tls.setHandshakeTimeout(4);   // segundos: si no, por defecto espera hasta 2 minutos
  Info next = get();
  JsonDocument doc;

  if (fetch(tls, "/api/v1/prices", doc)) next.priceUsd = doc["USD"] | next.priceUsd;
  if (fetch(tls, "/api/blocks/tip/height", doc)) next.height = doc.as<uint32_t>();
  if (fetch(tls, "/api/v1/fees/recommended", doc)) {
    next.feeFast = doc["fastestFee"] | next.feeFast;   // float: ya hay comisiones por debajo de 1
    next.feeHour = doc["hourFee"] | next.feeHour;
  }
  if (fetch(tls, "/api/v1/mining/hashrate/3d", doc)) {
    next.difficulty = doc["currentDifficulty"] | next.difficulty;
    next.hashrate = doc["currentHashrate"] | next.hashrate;
  }
  next.valid = next.height > 0 && next.priceUsd > 0;
  next.updatedAt = millis();

  portENTER_CRITICAL(&lock);
  info = next;
  portEXIT_CRITICAL(&lock);
  Serial.printf("[btcinfo] $%lu, bloque %lu, %.1f sat/vB\n", (unsigned long)next.priceUsd, (unsigned long)next.height, next.feeFast);
}

static void task(void*) {
  uint32_t lastAt = 0;
  for (;;) {
    const bool due = lastAt == 0 || millis() - lastAt > EVERY_MS;
    if (wanted && due && radio::online()) {
      radio::busy(radio::User::BitcoinInfo, true);
      { radio::Handshake fullSpeed; update(); }
      radio::busy(radio::User::BitcoinInfo, false);
      lastAt = millis();
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

void begin() { xTaskCreatePinnedToCore(task, "btcinfo", 8192, nullptr, 1, nullptr, 0); }
void want(bool on) { wanted = on; }

Info get() {
  portENTER_CRITICAL(&lock);
  Info copy = info;
  portEXIT_CRITICAL(&lock);
  return copy;
}

}  // namespace btcinfo
