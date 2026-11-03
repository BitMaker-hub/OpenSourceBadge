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
static String serverUrl, inkey, lnAddress, access, adminKey;   // se escriben al arrancar o en configure()
static String host, basePath;
static uint16_t port = 443;
static bool   tls = true;

static QueueHandle_t   payments = nullptr;
static volatile Status state = Status::NotConfigured;
static volatile int32_t balance = -1;
static volatile bool   restart = false;
static WebSocketsClient socket;
static bool            socketStarted = false;

// Con el ahorro de radio el websocket se corta mientras la radio duerme: un pago
// que llegue entonces no avisa. Al reconectar se piden los últimos pagos y se
// enseñan los que sean más nuevos que el último visto (hora del servidor).
static uint32_t lastPaymentTime = 0;
static bool     baselineSet = false;    // la primera vez tras encender, solo se apunta por dónde va

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
  adminKey  = get("w_admin");
  parseUrl();
}

void configure(const String& url, const String& key, const String& addr, const String& acc, const String& admin) {
  prefs.putString("w_url", url);
  prefs.putString("w_key", key);
  prefs.putString("w_addr", addr);
  prefs.putString("w_access", acc);
  prefs.putString("w_admin", admin);
  load();
  balance = -1;
  restart = true;
}

bool configured()       { return serverUrl.length() && inkey.length(); }
String address()        { return lnAddress; }
String accessUrl()      { return access; }
bool canWithdraw()      { return configured() && (adminKey.length() || access.length()); }
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

// La hora de un pago. LNbits 1.x la da como texto ISO ("2026-10-30T12:34:56.123+00:00"),
// las versiones viejas como segundos Unix. Solo hace falta para ordenar y comparar.
static uint32_t paymentTime(JsonVariant t) {
  if (t.is<uint32_t>() || t.is<int64_t>()) return t.as<uint32_t>();
  int y, mo, d, h, mi, s;
  if (sscanf(t | "", "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &s) != 6) return 0;
  // días desde 1970 (algoritmo de Howard Hinnant, calendario civil)
  y -= mo <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = y - era * 400;
  const unsigned doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const int32_t days = era * 146097 + (int32_t)doe - 719468;
  return (uint32_t)days * 86400 + h * 3600 + mi * 60 + s;
}

// Un pago que entra, a la cola del loop principal (venga del websocket o de la lista)
static void queuePayment(JsonObject p) {
  const int64_t msat = p["amount"] | 0;
  if (msat <= 0) return;                                  // solo pagos que entran
  const uint32_t t = paymentTime(p["time"]);
  if (t > lastPaymentTime) lastPaymentTime = t;

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

// Un aviso del websocket: {"wallet_balance": sats, "payment": {"amount": msat, "memo", "bolt11", "extra": {...}}}
static void onMessage(uint8_t* payload, size_t length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  if (doc["wallet_balance"].is<int>()) balance = doc["wallet_balance"].as<int>();
  JsonObject p = doc["payment"];
  if (!p.isNull()) queuePayment(p);
}

// Al (re)conectar: los últimos pagos cobrados, por si alguno llegó con la radio dormida
static void fetchMissed() {
  HTTPClient http;
  WiFiClientSecure secure;
  WiFiClient plain;
  const String url = String(tls ? "https://" : "http://") + host + ":" + port + basePath +
                     "/api/v1/payments?limit=10&sortby=time&direction=desc&status=success";
  if (tls) { secure.setInsecure(); http.begin(secure, url); }
  else     { http.begin(plain, url); }
  http.addHeader("X-Api-Key", inkey);
  http.setTimeout(5000);
  if (http.GET() != 200) { http.end(); return; }
  JsonDocument doc;
  const bool bad = (bool)deserializeJson(doc, http.getString());
  http.end();
  if (bad || !doc.is<JsonArray>()) return;
  JsonArray list = doc.as<JsonArray>();

  if (!baselineSet) {                                     // recién encendido: lo de antes no se enseña
    for (JsonObject p : list) lastPaymentTime = max(lastPaymentTime, paymentTime(p["time"]));
    baselineSet = true;
    return;
  }
  // Los nuevos, del más antiguo al más reciente (la lista viene al revés)
  const uint32_t since = lastPaymentTime;
  int found = 0;
  for (int i = list.size() - 1; i >= 0; i--) {
    JsonObject p = list[i];
    if ((int64_t)(p["amount"] | 0) > 0 && paymentTime(p["time"]) > since) {
      queuePayment(p);
      found++;
    }
  }
  if (found) Serial.printf("[lnbits] %d pago(s) que llegaron con la radio dormida\n", found);
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
  radio::busy(radio::User::Lnbits, false);
}

// ============================================================ retirada por el saldo

static volatile bool     wantWithdraw = false;
static volatile Withdraw wState = Withdraw::Idle;
static String            wLnurl, wLinkId;   // los escribe la tarea; la pantalla los lee cuando está Ready
static volatile int32_t  wSats = 0;

void     requestWithdraw() { wState = Withdraw::Preparing; wantWithdraw = true; }
Withdraw withdrawState()   { return wState; }
String   withdrawLnurl()   { return wState == Withdraw::Ready ? wLnurl : String(""); }
int32_t  withdrawSats()    { return wSats; }

// Una llamada a la API con la clave de admin del monedero (POST si hay cuerpo, DELETE si del)
static int adminCall(const char* method, const String& path, const String& body, String& out) {
  HTTPClient http;
  WiFiClientSecure secure;
  WiFiClient plain;
  const String url = String(tls ? "https://" : "http://") + host + ":" + port + basePath + path;
  if (tls) { secure.setInsecure(); http.begin(secure, url); }
  else     { http.begin(plain, url); }
  http.addHeader("X-Api-Key", adminKey);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);
  const int code = http.sendRequest(method, body);
  out = code > 0 ? http.getString() : "";
  http.end();
  return code;
}

static void createWithdraw() {
  fetchBalance();
  if (balance < 0) { wState = Withdraw::Failed; return; }
  // LNbits guarda una reserva para la comisión al pagar fuera: 1 % con un mínimo de 2 sats
  const int32_t reserve = max((int32_t)2, (int32_t)((balance + 99) / 100));
  const int32_t amount = balance - reserve;
  String out;
  if (wLinkId.length()) {                                   // el enlace de antes ya no vale: fuera
    adminCall("DELETE", "/withdraw/api/v1/links/" + wLinkId, "", out);
    wLinkId = "";
  }
  if (amount < 1) { wSats = 0; wState = Withdraw::Empty; return; }
  JsonDocument req;
  req["title"] = "Badge withdraw";
  req["min_withdrawable"] = 1;
  req["max_withdrawable"] = amount;
  req["uses"] = 1;
  req["wait_time"] = 1;
  req["is_unique"] = false;
  String body;
  serializeJson(req, body);
  const int code = adminCall("POST", "/withdraw/api/v1/links", body, out);
  JsonDocument doc;
  if (code != 200 && code != 201) { Serial.printf("[lnbits] retirada: LNbits dice %d\n", code); wState = Withdraw::Failed; return; }
  if (deserializeJson(doc, out) || !doc["lnurl"].is<const char*>()) { wState = Withdraw::Failed; return; }
  wLinkId = doc["id"] | "";
  wLnurl = String("lightning:") + (doc["lnurl"] | "");
  wSats = amount;
  wState = Withdraw::Ready;
  Serial.printf("[lnbits] enlace de retirada por %ld sats (saldo %ld)\n", (long)amount, (long)balance);
}

static void task(void*) {
  for (;;) {
    if (wantWithdraw && configured()) {
      if (!adminKey.length()) {                             // sin admin: el enlace fijo de antes
        wLnurl = access; wSats = balance; wState = access.length() ? Withdraw::Ready : Withdraw::Failed;
        wantWithdraw = false;
      } else if (radio::online()) {
        wantWithdraw = false;
        radio::busy(radio::User::Lnbits, true);
        radio::Handshake fullSpeed;
        createWithdraw();
      }
    }
    if (restart) { stopSocket(); restart = false; }
    if (!configured()) { stopSocket(); state = Status::NotConfigured; vTaskDelay(pdMS_TO_TICKS(500)); continue; }
    if (!radio::online()) { stopSocket(); state = Status::NoInternet; vTaskDelay(pdMS_TO_TICKS(200)); continue; }

    if (!socketStarted) {
      radio::busy(radio::User::Lnbits, true);
      radio::Handshake fullSpeed;   // dos llamadas HTTPS seguidas
      fetchBalance();
      fetchMissed();
      const String path = basePath + "/api/v1/ws/" + inkey;
      if (tls) socket.beginSSL(host.c_str(), port, path.c_str());
      else     socket.begin(host.c_str(), port, path.c_str());
      socket.onEvent(onEvent);
      socket.setReconnectInterval(5000);
      socket.enableHeartbeat(30000, 5000, 2);   // si el servidor deja de contestar, se reconecta
      socketStarted = true;
    }
    if (socket.isConnected()) {
      socket.loop();
    } else {
      radio::Handshake fullSpeed;   // si le toca, aquí dentro conecta: el saludo TLS
      socket.loop();
    }
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
