#pragma once
// ============================================================================
//  BADGE · los datos del badge: quién soy, a quién he conocido y lo recibido
//
//  De momento se rellena con datos de ejemplo (los del prototipo). Cuando esté
//  la página de configuración (badge.local), el perfil se leerá de la memoria
//  del ESP32 (NVS) y los peers llegarán por ESP-NOW.
// ============================================================================
#include <Arduino.h>

// Los personajes que puede elegir cada uno
enum class Role : uint8_t { Dev, Miner, Pleb, Whale, Nostrich, Anon, Count };

struct RoleInfo {
  const char* name;     // lo que sale en el chip
  uint16_t    color;    // color del marco y del chip
};
const RoleInfo& roleInfo(Role r);

struct Profile {
  uint16_t number;      // número de serie del badge (el "#42")
  String   name;
  String   handle;      // @ de X
  String   npub;        // Nostr
  String   lightning;   // dirección Lightning del badge (LNbits)
  String   tagline;
  Role     role;
};

struct Peer {
  String   name;
  Role     role;
  uint32_t block;       // en qué "bloque" nos conocimos
  String   time;        // hh:mm
};

// Un QR de los que van rotando en la página de QR
enum class QrKind : uint8_t { Lightning, Nostr, X, Setup };
struct QrItem {
  QrKind   kind;
  String   tab;         // "LN", "NOSTR", "X"
  String   header;      // título de la cabecera
  String   data;        // lo que lleva el QR
  String   title;       // "ZAP ME", "FOLLOW ON NOSTR"...
  String   subtitle;
  uint16_t color;
};

// Un zap que nos han mandado
struct Zap {
  uint32_t amount;
  String   from;
  String   via;          // "Nostr" o "Lightning"
  String   message;
};

namespace badge {

constexpr int MAX_PEERS = 64;
constexpr int MAX_QRS = 3;

void loadDemo();                      // datos de ejemplo del prototipo

Profile&      me();
int           peerCount();
const Peer&   peer(int i);            // 0 = el más reciente
void          addPeer(const Peer& p);
uint32_t      blockHeight();          // cada handshake "mina" un bloque
uint32_t      satsReceived();
void          addSats(uint32_t amount);
uint8_t       decentralization();     // 0..99, cuantos más roles distintos, más alto
String        socialHash();           // "1.4"
bool          metRole(Role r);

int           qrItems(QrItem* out);   // la lista de QR según lo que haya configurado

// Para las demos mientras no hay radio: un peer y un zap inventados
Peer          randomDemoPeer();
Zap           nextDemoZap();

}  // namespace badge
