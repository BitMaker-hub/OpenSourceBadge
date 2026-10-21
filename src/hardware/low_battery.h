#pragma once
// ============================================================================
//  LOW BATTERY · que el badge no deje la LiPo a cero
//
//  El T-Display no corta solo: al bajar la tensión el ESP32 se reinicia en
//  bucle y sigue chupando de la batería. Si la celda no lleva protección, por
//  debajo de ~2,5 V se estropea. Así que, con batería, si la tensión se queda
//  por debajo de 3,4 V durante 30 s: aviso en pantalla, todo apagado (LEDs,
//  luz y alimentación de la pantalla) y sueño profundo. Cualquier botón lo
//  despierta; si sigue sin carga, vuelve a dormirse.
// ============================================================================
#include <Arduino.h>

namespace lowbattery {

void begin();                 // lo primero del todo: suelta los pines que dejó sujetos el sueño
void checkAtBoot();           // con la pantalla ya lista: si sigue vacía, a dormir otra vez
void loop(bool usb);          // vigila la tensión; si toca, duerme (no vuelve)

}  // namespace lowbattery
