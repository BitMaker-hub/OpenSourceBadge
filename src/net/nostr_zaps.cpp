#include "nostr_zaps.h"
#include "bech32.h"
#include "../app/storage.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

namespace nostrzaps {

// Dos relays grandes: si uno falla, el otro sigue. Los recibos suelen estar en los dos.
struct Relay { const char* host; const char* path; };
static const Relay RELAYS[] = {
  {"relay.damus.io", "/"},
  {"relay.primal.net", "/"},
};
static const int RELAY_COUNT = sizeof(RELAYS) / sizeof(RELAYS[0]);

static const uint32_t WIFI_RETRY_MS = 20000;
static const uint32_t NAME_WAIT_MS  = 2000;    // cuánto esperamos a saber el nombre de quien paga

// Lo que la tarea le pasa al loop principal (sin String: va por una cola de FreeRTOS)
struct ZapMessage {
  uint32_t amount;
  char from[40];
  char message[120];
};

// ---- configuración (la escribe reload() con la tarea en pausa) ----
static String wifiSsid, wifiPassword, myPubkeyHex;

// ---- estado compartido entre la tarea y el loop principal ----
static QueueHandle_t   zaps = nullptr;
static volatile bool   paused = true;          // arrancamos parados hasta que begin() termine
static volatile bool   radioOff = true;        // la tarea confirma que ha soltado la radio
static volatile Status state = Status::NotConfigured;

// ---- solo los usa la tarea ----
static WebSocketsClient sockets[RELAY_COUNT];
static bool     relaysStarted = false;
static String   seenIds[16];                   // los dos relays mandan el mismo recibo: no lo contamos dos veces
static int      seenNext = 0;
static bool     waitingName = false;           // zap esperando a saber el nombre del que paga
static ZapMessage pending;
static String   pendingSender;
static uint32_t pendingDeadline = 0;

// ============================================================ utilidades

static void copy(char* dst, const String& src, size_t size) {
  strncpy(dst, src.c_str(), size - 1);
  dst[size - 1] = 0;
}

// Cantidad de una factura Lightning: "lnbc210n1..." son 210 nano-BTC = 21 sats
static uint32_t bolt11Sats(const String& invoice) {
  int i = 2;                                              // tras "ln"
  while (i < (int)invoice.length() && isalpha(invoice[i])) i++;   // la red: bc, tb, bcrt
  uint64_t value = 0;
  while (i < (int)invoice.length() && isdigit(invoice[i])) value = value * 10 + (invoice[i++] - '0');
  if (value == 0) return 0;
  switch (invoice[i]) {                                   // multiplicador sobre 1 BTC = 1e8 sats
    case 'm': return value * 100000;
    case 'u': return value * 100;
    case 'n': return value / 10;
    case 'p': return value / 10000;
    default:  return value * 100000000;
  }
}

static bool alreadySeen(const String& id) {
  for (const String& s : seenIds) if (s == id) return true;
  seenIds[seenNext] = id;
  seenNext = (seenNext + 1) % 16;
  return false;
}

static void deliver(const ZapMessage& z) {
  xQueueSend(zaps, &z, 0);
  Serial.printf("[nostr] zap de %lu sats de %s\n", (unsigned long)z.amount, z.from);
}

// ============================================================ lo que llega de los relays

// Un recibo de zap (kind 9735)
static void onZapReceipt(JsonObject event, int relay) {
  // Dentro va la petición de zap original (kind 9734) como texto JSON:
  // de ahí sacamos quién paga, cuánto y el mensaje
  String description, invoice;
  for (JsonArray tag : event["tags"].as<JsonArray>()) {
    if (tag[0] == "description") description = tag[1].as<String>();
    if (tag[0] == "bolt11") invoice = tag[1].as<String>();
  }
  JsonDocument request;
  if (deserializeJson(request, description)) return;

  ZapMessage z = {};
  z.amount = bolt11Sats(invoice);
  if (z.amount == 0) {
    for (JsonArray tag : request["tags"].as<JsonArray>())
      if (tag[0] == "amount") z.amount = tag[1].as<String>().toInt() / 1000;   // viene en milisats
  }
  copy(z.message, badge::clean(request["content"].as<String>(), sizeof z.message - 1), sizeof z.message);   // sin emojis: la fuente no los tiene

  const String sender = request["pubkey"].as<String>();
  const String npub = bech32::hexToNpub(sender);
  copy(z.from, npub.substring(0, 10) + ".." + npub.substring(npub.length() - 4), sizeof z.from);

  // Si había otro zap esperando su nombre, sale ya tal cual
  if (waitingName) deliver(pending);

  // Preguntamos el perfil del que paga (kind 0) para enseñar su nombre en vez del npub
  pending = z;
  pendingSender = sender;
  pendingDeadline = millis() + NAME_WAIT_MS;
  waitingName = true;
  String ask = "[\"REQ\",\"name\",{\"kinds\":[0],\"authors\":[\"" + sender + "\"],\"limit\":1}]";
  sockets[relay].sendTXT(ask);
}

// Un perfil (kind 0): si es el de quien nos acaba de pagar, ya tenemos nombre
static void onProfile(JsonObject event) {
  if (!waitingName || event["pubkey"].as<String>() != pendingSender) return;
  JsonDocument profile;
  if (deserializeJson(profile, event["content"].as<String>())) return;
  String name = profile["display_name"] | "";
  if (name.isEmpty()) name = profile["name"] | "";
  name = badge::clean(name, 30);
  if (name.length()) copy(pending.from, name, sizeof pending.from);
  deliver(pending);
  waitingName = false;
}

static void onRelayMessage(int relay, uint8_t* payload, size_t length) {
  JsonDocument msg;
  if (deserializeJson(msg, payload, length)) return;
  if (msg[0] != "EVENT") return;

  JsonObject event = msg[2];
  const String id = event["id"].as<String>().substring(0, 16);
  const int kind = event["kind"] | -1;
  if (kind == 9735 && !alreadySeen(id)) onZapReceipt(event, relay);
  if (kind == 0) onProfile(event);
}

static void onRelayEvent(int relay, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      Serial.printf("[nostr] conectado a %s\n", RELAYS[relay].host);
      // Recibos de zap para mí, solo los nuevos (limit 0: nada de lo antiguo)
      String req = "[\"REQ\",\"zaps\",{\"kinds\":[9735],\"#p\":[\"" + myPubkeyHex + "\"],\"limit\":0}]";
      sockets[relay].sendTXT(req);
      break;
    }
    case WStype_DISCONNECTED:
      Serial.printf("[nostr] desconectado de %s\n", RELAYS[relay].host);
      break;
    case WStype_TEXT:
      onRelayMessage(relay, payload, length);
      break;
    default:
      break;
  }
}

// ============================================================ la tarea

static void stopRadio() {
  if (relaysStarted) {
    for (auto& s : sockets) s.disconnect();
    relaysStarted = false;
  }
  if (WiFi.getMode() != WIFI_OFF) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
}

static void task(void*) {
  uint32_t wifiStartedAt = 0;
  for (;;) {
    // Parados (o sin configurar): soltamos la radio y esperamos
    if (paused || wifiSsid.isEmpty() || myPubkeyHex.isEmpty()) {
      if (!radioOff) { stopRadio(); radioOff = true; wifiStartedAt = 0; }
      state = paused ? Status::Paused : Status::NotConfigured;
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }
    radioOff = false;

    // 1. WiFi
    if (WiFi.status() != WL_CONNECTED) {
      if (wifiStartedAt == 0 || millis() - wifiStartedAt > WIFI_RETRY_MS) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
        wifiStartedAt = millis();
      }
      state = Status::ConnectingWifi;
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // 2. Relays
    if (!relaysStarted) {
      for (int i = 0; i < RELAY_COUNT; i++) {
        sockets[i].beginSSL(RELAYS[i].host, 443, RELAYS[i].path);
        sockets[i].onEvent([i](WStype_t type, uint8_t* payload, size_t length) { onRelayEvent(i, type, payload, length); });
        sockets[i].setReconnectInterval(5000);
      }
      relaysStarted = true;
    }
    bool anyConnected = false;
    for (auto& s : sockets) {
      s.loop();
      if (s.isConnected()) anyConnected = true;
    }
    state = anyConnected ? Status::Listening : Status::ConnectingRelay;

    // 3. Un zap que no ha recibido su nombre a tiempo sale con el npub
    if (waitingName && (int32_t)(millis() - pendingDeadline) > 0) {
      deliver(pending);
      waitingName = false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ============================================================ lo que usa el resto del firmware

static void readConfig() {
  storage::loadNetwork(wifiSsid, wifiPassword);
  myPubkeyHex = bech32::npubToHex(badge::me().npub);
}

void begin() {
  zaps = xQueueCreate(4, sizeof(ZapMessage));
  readConfig();
  xTaskCreatePinnedToCore(task, "nostr", 12288, nullptr, 1, nullptr, 0);
  paused = false;
}

void reload() {
  const bool was = paused;
  pause();
  readConfig();
  if (!was) resume();
}

void pause() {
  paused = true;
  const uint32_t start = millis();
  while (!radioOff && millis() - start < 3000) delay(10);
}

void resume()     { paused = false; }
Status status()   { return state; }

bool nextZap(Zap& z) {
  ZapMessage m;
  if (!zaps || xQueueReceive(zaps, &m, 0) != pdTRUE) return false;
  z.amount = m.amount;
  z.from = m.from;
  z.via = "Nostr";
  z.message = m.message;
  return true;
}

void injectForTest(const String& relayMessage) {
  onRelayMessage(0, (uint8_t*)relayMessage.c_str(), relayMessage.length());
  if (waitingName) { deliver(pending); waitingName = false; }   // sin internet no llegará el nombre
}

}  // namespace nostrzaps
