#pragma once
// ============================================================================
//  BADGE · los datos del badge: quién soy, a quién he conocido y lo recibido
//
//  Todo se guarda con storage:: y se recupera al arrancar. El perfil se rellena
//  desde el móvil (modo SETUP, página badge.local); los peers llegan por radio
//  en el handshake (app/peer_link).
// ============================================================================
#include <Arduino.h>

// Longitudes máximas de cada campo (también las usa la radio y la página web)
constexpr int NAME_LEN      = 20;
constexpr int HANDLE_LEN    = 20;
constexpr int NPUB_LEN      = 63;
constexpr int LIGHTNING_LEN = 60;
constexpr int TAGLINE_LEN   = 40;

// Los personajes que puede elegir cada uno
enum class Role : uint8_t { Dev, Miner, Pleb, Whale, Nostrich, Anon, Count };

struct RoleInfo {
  const char* name;     // lo que sale en el chip
  uint16_t    color;    // color del marco y del chip
};
const RoleInfo& roleInfo(Role r);
Role roleFromName(const String& name);   // "dev" -> Role::Dev (Anon si no lo conoce)

struct Profile {
  String name;
  String handle;        // @ de X
  String npub;          // Nostr
  String lightning;     // dirección Lightning
  String tagline;
  Role   role = Role::Anon;
};

struct Peer {
  String   name;
  String   handle;
  String   npub;
  String   lightning;
  Role     role = Role::Anon;
  uint8_t  mac[6] = {};   // identifica al badge: si repetimos handshake no cuenta doble
  uint16_t number = 0;    // su número de badge
  uint32_t block = 0;     // el bloque que "minamos" juntos
  uint32_t when = 0;      // cuándo (segundos Unix; 0 si no sabíamos la hora)
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

constexpr int MAX_PEERS = 200;
constexpr int MAX_QRS = 3;

void begin();                         // lee de la memoria todo lo guardado

// ---- yo ----
uint16_t      number();               // número de serie, el "#042"
void          setNumber(uint16_t n);
Profile&      me();
bool          isConfigured();         // ¿ha pasado alguien ya por badge.local?
void          saveMe();               // guarda me() después de cambiarlo
String        wifiName();             // "BADGE-042"

// ---- peers ----
int           peerCount();
const Peer&   peer(int i);            // 0 = el más reciente
void          addPeer(const Peer& p); // si ya lo conocíamos, lo actualiza
bool          metRole(Role r);

// ---- números del perfil ----
uint32_t      blockHeight();          // cada handshake "mina" un bloque
void          setBlockHeight(uint32_t h);
uint32_t      satsReceived();
void          addSats(uint32_t amount);
void          resetSats();            // contador de zaps a cero (comando serie Z)
uint8_t       decentralization();     // 0..99, cuantos más roles distintos, más alto
String        socialHash();           // "1.4"

int           qrItems(QrItem* out);   // la lista de QR según lo que haya configurado

// ---- hora ----
// El badge no tiene reloj con pila: la hora la pone el móvil al configurar o
// la copia de otro badge en un handshake. Hasta entonces, no sabe qué hora es.
bool          hasTime();
uint32_t      now();                  // segundos Unix, 0 si no sabe la hora
void          setTime(uint32_t unixSeconds);
void          setTimezone(int16_t minutesFromUtc);
String        clock(uint32_t unixSeconds);   // "14:02" en hora local, o "--:--"

// Deja el texto en algo que la pantalla sabe pintar: sin acentos ni emojis,
// sin espacios de más y como mucho `max` letras
String        clean(const String& s, int max);

// ---- demos, mientras no hay zaps de verdad ----
void          loadDemo();             // rellena con los datos del prototipo (sin guardar)
Zap           nextDemoZap();
void          factoryReset();

}  // namespace badge
