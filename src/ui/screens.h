#pragma once
// ============================================================================
//  PANTALLAS · una función por pantalla
//
//  Cada función dibuja su pantalla entera en el lienzo, con los datos que le
//  pasan o los de badge::. No decide nada ni cambia de estado: eso lo hace la
//  máquina de estados (app/state_machine.cpp), que elige cuál llamar.
//
//  Son copia del prototipo pantallas_v2.html; las coordenadas son las mismas
//  salvo donde las fuentes de la placa miden distinto.
// ============================================================================
#include "../app/badge.h"
#include "../hardware/buttons.h"

namespace screens {

// Páginas de inicio, las que se pasan con MODE
constexpr int HOME_PAGES = 4;
void profile();                                              // página 1
void qrCodes(const QrItem* items, int count, int current);   // página 2
void peers();                                                // página 3
void stats();                                                // página 4

// Arranque en tres escenas: Bitcoin Amsterdam, quién lo ha hecho, y la ₿ del badge
constexpr uint32_t BOOT_SCENE_MS = 2400;     // lo que dura cada una de las dos primeras
void bootAmsterdam(uint32_t t);
void bootMakers(uint32_t t);
void bootBadge(uint32_t t);
void qrFullscreen(const QrItem& item);
void setup(uint32_t t, int phonesConnected);
void setupSaved();
void seedLab(int selected);
void zap(const Zap& z, uint32_t t);

// Handshake, un dibujo por paso
void handshakeSearching(uint32_t t);
void handshakeFound(const Peer& other, uint32_t t);
void handshakeMining(const String* hashes, int count, uint8_t progress, uint32_t nonce);
void handshakeMined(const Peer& other);
void handshakeNotFound();

// Prueba de placa
struct HardwareTestInfo {
  String      ledStep;        // "LED 7 · VERDE"
  bool        paused;
  ButtonEvent lastEvent;
  uint8_t     i2c[8];
  uint8_t     i2cCount;
  uint16_t    usbMv;
  uint16_t    batteryMv;
  uint16_t    ledMa;
};
void hardwareTest(const HardwareTestInfo& info);

}  // namespace screens
