#include "peer_link.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace peerlink {

// El mensaje que se gritan los badges: la tarjeta de visita entera. ESP-NOW
// admite 250 bytes y esto ocupa 241; por eso la huella PGP va en binario (20
// bytes) y no en texto (40).
struct __attribute__((packed)) Hello {
  char     magic[4];                    // "OSB2": si no, no es un badge nuestro (OSB1 era sin Telegram ni PGP)
  uint8_t  heard[6];                    // a quién oigo yo (ceros = a nadie todavía)
  uint16_t number;
  uint8_t  role;
  uint32_t block;
  uint32_t now;                         // mi hora, o 0 si no la sé
  char     name[NAME_LEN + 1];
  char     handle[HANDLE_LEN + 1];
  char     npub[NPUB_LEN + 1];
  char     lightning[LIGHTNING_LEN + 1];
  char     telegram[TELEGRAM_LEN + 1];
  uint8_t  pgp[PGP_LEN / 2];            // ceros = sin PGP
};
static_assert(sizeof(Hello) <= 250, "ESP-NOW no manda más de 250 bytes");

// Lo que llega por radio se apunta aquí (lo escribe la tarea de la WiFi)
// y lo procesa loop(), en nuestro hilo
struct Received {
  uint8_t mac[6];
  Hello   hello;
};

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint32_t SEND_EVERY_MS = 150;

static QueueHandle_t inbox = nullptr;
static bool     running = false;
static Status   state = Status::Searching;
static uint8_t  myMac[6];
static Peer     candidate;
static uint32_t block = 0;
static uint32_t lastSend = 0;


static void copy(char* dst, const String& src, size_t size) {
  strncpy(dst, src.c_str(), size - 1);
  dst[size - 1] = 0;
}

// ---------------------------------------------------------------- radio

static void onReceive(const uint8_t* mac, const uint8_t* data, int len) {
  if (len != sizeof(Hello) || memcmp(data, "OSB2", 4) != 0) return;
  Received r;
  memcpy(r.mac, mac, 6);
  memcpy(&r.hello, data, sizeof(Hello));
  xQueueSend(inbox, &r, 0);   // si la cola está llena, se pierde: ya llegará el siguiente
}

static void sendHello() {
  const Profile& me = badge::me();
  Hello h = {};
  memcpy(h.magic, "OSB2", 4);
  memcpy(h.heard, candidate.mac, 6);
  h.number = badge::number();
  h.role = (uint8_t)me.role;
  h.block = badge::blockHeight();
  h.now = badge::now();
  copy(h.name, me.name, sizeof h.name);
  copy(h.handle, me.handle, sizeof h.handle);
  copy(h.npub, me.npub, sizeof h.npub);
  copy(h.lightning, me.lightning, sizeof h.lightning);
  copy(h.telegram, me.telegram, sizeof h.telegram);
  if (me.pgp.length() == PGP_LEN)
    for (int i = 0; i < PGP_LEN / 2; i++) h.pgp[i] = strtoul(me.pgp.substring(i * 2, i * 2 + 2).c_str(), nullptr, 16);
  esp_now_send(BROADCAST, (const uint8_t*)&h, sizeof h);
}

// ---------------------------------------------------------------- lo que oímos

static void handle(const Received& r) {
  const Hello& h = r.hello;

  // Si el otro sabe la hora y nosotros no, nos la quedamos
  if (!badge::hasTime() && h.now > 1700000000) badge::setTime(h.now);

  // Buscando: el primero que oímos es nuestro candidato
  if (state == Status::Searching) {
    memcpy(candidate.mac, r.mac, 6);
    state = Status::Found;
    Serial.printf("[peer] oigo a #%03u %s\n", h.number, h.name);
  }

  // Solo nos interesa el candidato; el resto de badges que griten cerca, no
  if (memcmp(r.mac, candidate.mac, 6) != 0) return;

  candidate.name = h.name;
  candidate.handle = h.handle;
  candidate.npub = h.npub;
  candidate.lightning = h.lightning;
  candidate.telegram = String(h.telegram).substring(0, TELEGRAM_LEN);
  candidate.pgp = "";
  for (uint8_t b : h.pgp)
    if (b) { candidate.pgp = "x"; break; }               // ¿hay huella? (20 ceros = no)
  if (candidate.pgp.length()) {
    char hex[PGP_LEN + 1];
    for (int i = 0; i < PGP_LEN / 2; i++) snprintf(hex + i * 2, 3, "%02X", h.pgp[i]);
    candidate.pgp = hex;
  }
  candidate.role = h.role < (uint8_t)Role::Count ? (Role)h.role : Role::Anon;
  candidate.number = h.number;

  // ¿Nos oye él a nosotros? Entonces ya está
  if (state == Status::Found && memcmp(h.heard, myMac, 6) == 0) {
    block = max(badge::blockHeight(), h.block) + 1;
    candidate.block = block;
    candidate.when = badge::now();
    state = Status::Done;
    Serial.printf("[peer] handshake con #%03u, bloque %lu\n", h.number, (unsigned long)block);
  }
}

// ---------------------------------------------------------------- arrancar y parar

void start() {
  if (!inbox) inbox = xQueueCreate(8, sizeof(Received));
  xQueueReset(inbox);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);   // todos los badges en el mismo canal
  esp_read_mac(myMac, ESP_MAC_WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("[peer] no arranca ESP-NOW");
    return;
  }
  esp_now_peer_info_t everyone = {};
  memcpy(everyone.peer_addr, BROADCAST, 6);
  everyone.channel = 1;
  esp_now_add_peer(&everyone);
  esp_now_register_recv_cb(onReceive);

  candidate = Peer();
  state = Status::Searching;
  lastSend = 0;
  running = true;
}

void stop() {
  if (!running) return;
  esp_now_unregister_recv_cb();
  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
  running = false;
}

void loop() {
  if (!running) return;

  // Seguimos gritando también cuando ya está hecho: el otro puede necesitar
  // oír nuestro "te oigo" para terminar él también
  if (millis() - lastSend >= SEND_EVERY_MS) {
    sendHello();
    lastSend = millis();
  }

  Received r;
  while (xQueueReceive(inbox, &r, 0) == pdTRUE) handle(r);
}

Status status()       { return state; }
const Peer& other()   { return candidate; }
uint32_t agreedBlock(){ return block; }

}  // namespace peerlink
