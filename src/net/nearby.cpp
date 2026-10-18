#include "nearby.h"
#include "radio.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace nearby {

// El mensaje: "estoy aquí" o "te invito". Mucho más corto que el del handshake.
enum : uint8_t { HELLO = 1, INVITE = 2 };
struct __attribute__((packed)) Message {
  char     magic[4];               // "OSBN"
  uint8_t  type;
  uint8_t  target[6];              // a quién va la invitación (en HELLO, ceros)
  uint16_t number;
  uint8_t  role;
  uint8_t  channel;                // en qué canal estoy
  char     name[NAME_LEN + 1];
};

struct Received {
  uint8_t mac[6];
  int8_t  rssi;
  Message msg;
};

static const uint8_t  BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint32_t HELLO_EVERY_MS = 1000;
static const uint32_t HOP_EVERY_MS   = 1100;   // un poco más que un HELLO: así siempre se oye uno
static const uint32_t FORGET_MS      = 6000;   // si no le oímos en 6 s, ya no está
static const uint32_t STAY_MS        = 8000;   // tras oír a alguien, nos quedamos en su canal
static const int      MAX_NEIGHBORS  = 12;

static bool          started = false;
static QueueHandle_t inbox = nullptr;
static uint8_t       myMac[6];
static Neighbor      table[MAX_NEIGHBORS];
static int           nTable = 0;
static uint32_t      lastHello = 0, lastHop = 0, lastHeard = 0;
static int           hopIndex = 0;
static Neighbor      pendingInvite;
static bool          hasInvite = false;
static uint32_t      lastInviteAt = 0;

// ---- RSSI: ESP-NOW no lo da en esta versión, así que lo sacamos del modo promiscuo ----
struct RssiSeen { uint8_t mac[6]; int8_t rssi; };
static RssiSeen      rssiTable[8];
static volatile int  rssiNext = 0;

static void onPromiscuous(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t* pkt = (const wifi_promiscuous_pkt_t*)buf;
  if (pkt->payload[0] != 0xD0) return;            // solo tramas de acción (las de ESP-NOW)
  RssiSeen& r = rssiTable[rssiNext];
  memcpy(r.mac, pkt->payload + 10, 6);            // dirección de quien lo manda
  r.rssi = pkt->rx_ctrl.rssi;
  rssiNext = (rssiNext + 1) % 8;
}

static int8_t rssiOf(const uint8_t* mac) {
  for (const RssiSeen& r : rssiTable) if (memcmp(r.mac, mac, 6) == 0) return r.rssi;
  return -100;
}

static void onReceive(const uint8_t* mac, const uint8_t* data, int len) {
  if (len != sizeof(Message) || memcmp(data, "OSBN", 4) != 0) return;
  Received r;
  memcpy(r.mac, mac, 6);
  r.rssi = rssiOf(mac);
  memcpy(&r.msg, data, sizeof(Message));
  xQueueSend(inbox, &r, 0);
}

// ============================================================ arrancar y parar

static uint8_t currentChannel() {
  uint8_t primary;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&primary, &second);
  return primary;
}

static void start() {
  esp_read_mac(myMac, ESP_MAC_WIFI_STA);
  if (esp_now_init() != ESP_OK) return;
  esp_now_peer_info_t everyone = {};
  memcpy(everyone.peer_addr, BROADCAST, 6);
  everyone.channel = 0;                           // el canal en el que esté la radio
  esp_now_add_peer(&everyone);
  esp_now_register_recv_cb(onReceive);

  wifi_promiscuous_filter_t filter = {WIFI_PROMIS_FILTER_MASK_MGMT};
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(onPromiscuous);
  esp_wifi_set_promiscuous(true);
  started = true;
}

void stop() {
  if (!started) return;
  esp_wifi_set_promiscuous(false);
  esp_now_unregister_recv_cb();
  esp_now_deinit();
  started = false;
  nTable = 0;
}

void begin() {
  inbox = xQueueCreate(8, sizeof(Received));
}

// ============================================================ mensajes

static void send(uint8_t type, const uint8_t* target) {
  const Profile& me = badge::me();
  Message m = {};
  memcpy(m.magic, "OSBN", 4);
  m.type = type;
  if (target) memcpy(m.target, target, 6);
  m.number = badge::number();
  m.role = (uint8_t)me.role;
  m.channel = currentChannel();
  strncpy(m.name, me.name.c_str(), NAME_LEN);
  esp_now_send(BROADCAST, (const uint8_t*)&m, sizeof m);
}

static Neighbor* remember(const Received& r) {
  Neighbor* n = nullptr;
  for (int i = 0; i < nTable; i++) if (memcmp(table[i].mac, r.mac, 6) == 0) n = &table[i];
  if (!n) {
    if (nTable == MAX_NEIGHBORS) return nullptr;
    n = &table[nTable++];
    memcpy(n->mac, r.mac, 6);
    n->rssi = r.rssi;
  }
  n->number = r.msg.number;
  n->role = r.msg.role < (uint8_t)Role::Count ? (Role)r.msg.role : Role::Anon;
  strncpy(n->name, r.msg.name, NAME_LEN);
  n->name[NAME_LEN] = 0;
  n->rssi = (n->rssi * 3 + r.rssi) / 4;           // suavizado: la señal baila mucho
  n->close = n->rssi >= CLOSE_RSSI;
  n->lastSeen = millis();
  return n;
}

static void forgetOld() {
  for (int i = 0; i < nTable;) {
    if (millis() - table[i].lastSeen > FORGET_MS) table[i] = table[--nTable];
    else i++;
  }
}

// Sin WiFi propia, saltamos de canal hasta oír a alguien. Casi todas las WiFi
// están en el 1, el 6 o el 11, así que uno de cada dos saltos va a uno de esos
// tres (y el 1 es además donde se juntan los badges sin WiFi).
static void hopIfAlone() {
  if (radio::status() != radio::Status::NoNetwork) return;   // conectado: el canal es el de la WiFi
  if (millis() - lastHeard < STAY_MS) return;                 // hay alguien: nos quedamos
  if (millis() - lastHop < HOP_EVERY_MS) return;
  lastHop = millis();
  static const uint8_t COMMON[3] = {1, 6, 11};
  hopIndex = (hopIndex + 1) % 78;                            // 3 x 13 x 2: vuelve a empezar
  const uint8_t ch = hopIndex % 2 == 0 ? COMMON[(hopIndex / 2) % 3] : 1 + (hopIndex / 2) % 13;
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  send(HELLO, nullptr);                                       // que nos oigan también en este canal
}

// ============================================================ bucle

void loop() {
  if (!inbox) return;
  const bool radioReady = radio::status() != radio::Status::Exclusive && WiFi.getMode() == WIFI_STA;
  if (!radioReady) { stop(); return; }
  if (!started) start();
  if (!started) return;

  Received r;
  while (xQueueReceive(inbox, &r, 0) == pdTRUE) {
    lastHeard = millis();
    Neighbor* n = remember(r);
    // Invitación para nosotros (no repetimos la misma en 5 s: llega tres veces)
    if (n && r.msg.type == INVITE && memcmp(r.msg.target, myMac, 6) == 0 &&
        millis() - lastInviteAt > 5000) {
      pendingInvite = *n;
      hasInvite = true;
      lastInviteAt = millis();
      Serial.printf("[nearby] invitación de #%03u %s\n", n->number, n->name);
    }
    // Si estamos saltando sin WiFi y el otro está en otro canal fijo, nos vamos al suyo
    if (radio::status() == radio::Status::NoNetwork && r.msg.channel && r.msg.channel != currentChannel())
      esp_wifi_set_channel(r.msg.channel, WIFI_SECOND_CHAN_NONE);
  }
  forgetOld();
  hopIfAlone();

  if (millis() - lastHello >= HELLO_EVERY_MS) {
    send(HELLO, nullptr);
    lastHello = millis();
  }
}

// ============================================================ consultas

const Neighbor* closest() {
  const Neighbor* best = nullptr;
  for (int i = 0; i < nTable; i++)
    if (table[i].close && (!best || table[i].rssi > best->rssi)) best = &table[i];
  return best;
}

int count() { return nTable; }

void invite(const Neighbor& who) {
  for (int i = 0; i < 3; i++) {
    send(INVITE, who.mac);
    delay(30);
  }
  Serial.printf("[nearby] invito a #%03u %s\n", who.number, who.name);
}

bool takeInvite(Neighbor& from) {
  if (!hasInvite) return false;
  hasInvite = false;
  if (millis() - lastInviteAt > 15000) return false;          // demasiado vieja
  from = pendingInvite;
  return true;
}

uint8_t channel() { return started ? currentChannel() : 0; }

}  // namespace nearby
