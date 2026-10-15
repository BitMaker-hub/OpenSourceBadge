#pragma once
// ============================================================================
//  BECH32 · pasar claves de Nostr de "npub1..." a hexadecimal y al revés
//
//  En la página el usuario pone su npub (bech32), pero los relays de Nostr
//  hablan en hexadecimal. Y al revés: de quien nos hace zap llega el hex y en
//  pantalla queda mejor "npub1abc..xyz".
// ============================================================================
#include <Arduino.h>

namespace bech32 {

// "npub1..." -> 64 caracteres hex. Devuelve "" si no es un npub válido.
String npubToHex(const String& npub);

// 64 caracteres hex -> "npub1..."
String hexToNpub(const String& hex);

}  // namespace bech32
