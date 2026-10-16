#pragma once
// ============================================================================
//  SEED LAB · aprender cómo nace una semilla de Bitcoin, paso a paso
//
//  Lo mismo que hace el Seeder, en pequeño: la entropía la pones tú tirando
//  una moneda o un dado de verdad, y el badge la convierte en las palabras
//  BIP39 y te enseña la primera dirección. Así se ve que una semilla no es
//  magia: son bits que has generado tú.
//
//  Va con su propia máquina de estados (como el handshake):
//
//    MENÚ ─MODE─> PALABRAS ─MODE─> CAPTURA ──(completa)──> GENERANDO ──> RESULTADO
//      (dado/moneda)  (12/24)     (monedas o dados)                      (palabras, dirección,
//                                  SEED largo: abandonar                  entropía, final)
//
//  Por seguridad, mientras dura: radio apagada, sin capturas de pantalla por
//  el puerto serie y, al salir, todo se borra de la memoria. Nada se guarda.
//  Aun así es para aprender: el badge tiene radio, y para fondos de verdad
//  hay que usar un dispositivo sin conexión, como el Seeder.
// ============================================================================
#include "../hardware/buttons.h"

namespace seedlab {

void start();                        // al entrar: menú y todo a cero
bool update(ButtonEvent ev, uint32_t t);   // LEDs y pantalla; false = salir del Seed Lab
void wipe();                         // borrar entropía, palabras y todo lo derivado

}  // namespace seedlab
