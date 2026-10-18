#include "lnbits.h"
#include "radio.h"
#include "bech32.h"
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

namespace lnbits {

// Lo que la tarea le pasa al loop principal por una cola (sin String)
struct PaymentMessage {
  uint32_t amount;
  char from[40];
  char via[12];
  char message[120];
  char id[64];
};

static Preferences prefs;
static String serverUrl, inkey, lnAddress, access;   // se escriben al arrancar o en configure()
static String host, basePath;
static uint16_t port = 443;
static bool   tls = true;

static QueueHandle_t   payments = nullptr;
static volatile Status state = Status::NotConfigured;
static volatile int32_t balance = -1;
static volatile bool   restart = false;
static WebSocketsClient socket;
static bool            socketStarted = false;

// ============================================================ configuración

static void copy(char* dst, const String& src, size_t size) {
  strncpy(dst, src.c_str(), size - 1);
  dst[size - 1] = 0;
}

// "https://zaps.bitronics.store/algo" -> tls, host, puerto y ruta base
static void parseUrl() {
  String u = serverUrl;
  tls = !u.startsWith("http://");
  u.replace("https://", "");
  u.replace("http://", "");
  const int slash = u.indexOf('/');
  basePath = slash >= 0 ? u.substring(slash) : "";
  if (basePath.endsWith("/")) basePath.remove(basePath.length() - 1);
  String hostPort = slash >= 0 ? u.substring(0, slash) : u;
  const int colon = hostPort.indexOf(':');
  host = colon >= 0 ? hostPort.substring(0, colon) : hostPort;
  port = colon >= 0 ? hostPort.substring(colon + 1).toInt() : (tls ? 443 : 80);
}

static String get(const char* key) { return prefs.isKey(key) ? prefs.getString(key) : ""; }

static void load() {
  serverUrl = get("w_url");
  inkey     = get("w_key");
  lnAddress = get("w_addr");
  access    = get("w_access");
  parseUrl();
}

void configure(const String& url, const String& key, const String& addr, const String& acc) {
  prefs.putString("w_url", url);
  prefs.putString("w_key", key);
  prefs.putString("w_addr", addr);
  prefs.putString("w_access", acc);
  load();
  balance = -1;
  restart = true;
}

bool configured()       { return serverUrl.length() && inkey.length(); }
String address()        { return lnAddress; }
String accessUrl()      { return access; }
int32_t balanceSats()   { return balance; }
Status status()         { return state; }

// ============================================================ lo que llega

// El saldo, preguntado una vez al conectar (luego llega con cada pago)
static void fetchBalance() {
  HTTPClient http;
  WiFiClientSecure secure;
  WiFiClient plain;
  const String url = String(tls ? "https://" : "http://") + host + ":" + port + basePath + "/api/v1/wallet";
  if (tls) { secure.setInsecure(); http.begin(secure, url); }
  else     { http.begin(plain, url); }
  http.addHeader("X-Api-Key", inkey);
  http.setTimeout(5000);
  if (http.GET() == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, http.getString())) balance = (doc["balance"] | 0) / 1000;   // viene en milisats
  }
  http.end();
}

// Un pago: {"wallet_balance": sats, "payment": {"amount": msat, "memo", "bolt11", "extra": {...}}}
static void onMessage(uint8_t* payload, size_t length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  if (doc["wallet_balance"].is<int>()) balance = doc["wallet_balance"].as<int>();
  JsonObject p = doc["payment"];
  if (p.isNull()) return;
  const int64_t msat = p["amount"] | 0;
  if (msat <= 0) return;                                  // solo pagos que entran

  PaymentMessage m = {};
  m.amount = msat / 1000;
  copy(m.id, p["bolt11"].as<String>(), sizeof m.id);     // para no contar dos veces el mismo pago
  JsonObject extra = p["extra"];

  // ¿Zap de Nostr? LNbits guarda la petición de zap (kind 9734) en extra.nostr
  String zapRequest = extra["nostr"] | "";
  JsonDocument zr;
  if (zapRequest.length() && !deserializeJson(zr, zapRequest)) {
    copy(m.via, "Nostr", sizeof m.via);
    copy(m.message, badge::clean(zr["content"] | "", sizeof m.message - 1), sizeof m.message);
    const String npub = bech32::hexToNpub(zr["pubkey"] | "");
    if (npub.length()) copy(m.from, npub.substring(0, 10) + ".." + npub.substring(npub.length() - 4), sizeof m.from);
  } else {
    copy(m.via, "Lightning", sizeof m.via);
    String comment = extra["comment"] | "";
    if (comment.isEmpty() && extra["comment"].is<JsonArray>()) comment = extra["comment"][0] | "";
    if (comment.isEmpty()) comment = p["memo"] | "";
    copy(m.message, badge::clean(comment, sizeof m.message - 1), sizeof m.message);
  }
  if (!m.from[0]) copy(m.from, "someone", sizeof m.from);
  xQueueSend(payments, &m, 0);
  Serial.printf("[lnbits] pago de %lu sats (%s)\n", (unsigned long)m.amount, m.via);
}

static void onEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:    Serial.println("[lnbits] escuchando el monedero"); break;
    case WStype_DISCONNECTED: Serial.println("[lnbits] desconectado"); break;
    case WStype_TEXT:         onMessage(payload, length); break;
    default: break;
  }
}

// ============================================================ la tarea

static void stopSocket() {
  if (!socketStarted) return;
  socket.disconnect();
  socketStarted = false;
}

static void task(void*) {
  for (;;) {
    if (restart) { stopSocket(); restart = false; }
    if (!configured()) { stopSocket(); state = Status::NotConfigured; vTaskDelay(pdMS_TO_TICKS(500)); continue; }
    if (!radio::online()) { stopSocket(); state = Status::NoInternet; vTaskDelay(pdMS_TO_TICKS(200)); continue; }

    if (!socketStarted) {
      fetchBalance();
      const String path = basePath + "/api/v1/ws/" + inkey;
      if (tls) socket.beginSSL(host.c_str(), port, path.c_str());
      else     socket.begin(host.c_str(), port, path.c_str());
      socket.onEvent(onEvent);
      socket.setReconnectInterval(5000);
      socket.enableHeartbeat(30000, 5000, 2);   // si el servidor deja de contestar, se reconecta
      socketStarted = true;
    }
    socket.loop();
    state = socket.isConnected() ? Status::Listening : Status::Connecting;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void begin() {
  prefs.begin("badge", false);
  load();
  payments = xQueueCreate(4, sizeof(PaymentMessage));
  xTaskCreatePinnedToCore(task, "lnbits", 10240, nullptr, 1, nullptr, 0);
}

bool nextZap(Zap& z) {
  PaymentMessage m;
  if (!payments || xQueueReceive(payments, &m, 0) != pdTRUE) return false;
  z.amount = m.amount;
  z.from = m.from;
  z.via = m.via;
  z.message = m.message;
  z.id = m.id;
  return true;
}

}  // namespace lnbits
