#pragma once
// ============================================================================
//  LNBITS · el monedero del badge
//
//  Cada badge lleva su monedero en un servidor LNbits, con su dirección
//  Lightning (badge042@zaps.bitronics.store). Se graba en fábrica con el
//  comando serie "L" y queda en la memoria:
//
//    - url:     el servidor LNbits
//    - inkey:   la clave de SOLO LECTURA (invoice key): ve los pagos, no puede gastar
//    - address: la dirección Lightning del badge, la que sale en su QR
//    - access:  el enlace de retirada (LNURL-withdraw) de ESTE monedero: con él
//               se sacan los sats a la wallet de cada uno. Se enseña solo detrás
//               de un aviso, en la pantalla WITHDRAW. (No el enlace de la cuenta:
//               los 100 monederos cuelgan de la misma y daría acceso a todos.)
//
//  Con internet (net/radio), el badge abre un websocket con su monedero y
//  LNbits le avisa al momento de cada pago: un pago normal o un zap de Nostr
//  hecho a su dirección. Eso es lo que enciende la ₿.
//
//  Los sats se quedan en el servidor (custodia): para cantidades pequeñas.
// ============================================================================
#include "../app/badge.h"

namespace lnbits {

enum class Status : uint8_t {
  NotConfigured,   // este badge no tiene monedero
  NoInternet,
  Connecting,
  Listening,
};

void begin();

// Fábrica: guardar el monedero de este badge
void configure(const String& url, const String& inkey, const String& address, const String& access);

bool   configured();
String address();       // dirección Lightning del badge ("" si no tiene)
String accessUrl();     // enlace para sacar los sats ("" si no tiene)
int32_t balanceSats();  // -1 si aún no lo sabemos

Status status();

// Si ha llegado un pago, lo copia en `z` y devuelve true
bool nextZap(Zap& z);

}  // namespace lnbits
