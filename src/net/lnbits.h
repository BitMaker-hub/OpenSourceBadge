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
//    - admin:   la clave de ADMIN de ESTE monedero (solo de este, nunca la de la
//               cuenta). Con ella, al abrir la pantalla WITHDRAW, el badge crea un
//               enlace de retirada de un solo uso POR EL SALDO que tiene. Un enlace
//               fijo enseñaba su máximo (1.000.000 sats) y confundía. Riesgo: quien saque la memoria del badge puede gastar los
//               sats de este monedero; casi lo mismo que quien vea el QR de retirada.
//    - access:  (de antes) un enlace de retirada fijo. Solo se usa si no hay admin.
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
void configure(const String& url, const String& inkey, const String& address, const String& access,
               const String& admin = "");

bool   configured();
String address();       // dirección Lightning del badge ("" si no tiene)
String accessUrl();     // enlace fijo de antes para sacar los sats ("" si no tiene)
bool   canWithdraw();   // tiene algo con lo que retirar (la clave de admin o el enlace fijo)

// La retirada por el saldo. La pantalla WITHDRAW la pide al entrar; la tarea
// consulta el saldo y crea un enlace de un solo uso por él (menos la reserva
// para la comisión de la red, que LNbits guarda al pagar fuera).
enum class Withdraw : uint8_t { Idle, Preparing, Ready, Empty, Failed };
void     requestWithdraw();
Withdraw withdrawState();
String   withdrawLnurl();   // "lightning:LNURL1..." cuando está Ready
int32_t  withdrawSats();    // por cuánto es el enlace
int32_t balanceSats();  // -1 si aún no lo sabemos

Status status();

// Si ha llegado un pago, lo copia en `z` y devuelve true
bool nextZap(Zap& z);

}  // namespace lnbits
