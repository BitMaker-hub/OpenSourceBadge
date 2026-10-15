// ============================================================================
//  OPEN SOURCE BADGE · Proof of Peer · Bitcoin Amsterdam 2026
//  Diseñado por Bitmaker · bitronics.store
//
//  Cómo está organizado:
//
//    config.h              pines y límites de la placa
//    hardware/             lo que toca hierro: botones, LEDs, alimentación, Qwiic
//    effects/              efectos de LEDs reutilizables (estela, parpadeo...)
//    ui/                   pantalla: piezas comunes, personajes y una función por pantalla
//    app/badge             los datos: mi perfil, mis peers, los sats recibidos
//    app/state_machine     QUÉ hace el badge en cada momento (empezar a leer por aquí)
//
//  El loop es siempre igual: leer botones -> el estado actual decide, enciende
//  LEDs y dibuja -> mandar LEDs y pantalla.
// ============================================================================
#include <Arduino.h>
#include "config.h"
#include "hardware/buttons.h"
#include "hardware/leds.h"
#include "hardware/power.h"
#include "hardware/qwiic.h"
#include "ui/display.h"
#include "app/state_machine.h"

static const uint32_t FRAME_MS = 33;   // ~30 imágenes por segundo

// Para probar desde el ordenador con el monitor serie:
//   p / m / s  = PEER / MODE / SEED corto
//   P / M / S  = los mismos, pulsación larga
//   z          = simular que llega un zap
static ButtonEvent readSerialCommand() {
  if (!Serial.available()) return ButtonEvent::None;
  switch (Serial.read()) {
    case 'p': return ButtonEvent::PeerShort;
    case 'P': return ButtonEvent::PeerLong;
    case 'm': return ButtonEvent::ModeShort;
    case 'M': return ButtonEvent::ModeLong;
    case 's': return ButtonEvent::SeedShort;
    case 'S': return ButtonEvent::SeedLong;
    case 'z': app::simulateZap(); return ButtonEvent::None;
    default:  return ButtonEvent::None;
  }
}

void setup() {
  Serial.begin(115200);
  power::begin();     // lo primero: sin GPIO15 alto, con batería no hay pantalla
  buttons::begin();
  leds::begin();
  qwiic::begin();
  display::begin();
  app::begin();
  Serial.println("Open Source Badge listo. Teclas: p m s (P M S largas), z = zap");
}

void loop() {
  ButtonEvent event = buttons::poll();
  if (event == ButtonEvent::None) event = readSerialCommand();
  if (event != ButtonEvent::None) Serial.printf("[boton] %s\n", buttons::eventName(event));

  // Sin eventos, solo se redibuja cada FRAME_MS; con un evento, al momento
  static uint32_t lastFrame = 0;
  if (event == ButtonEvent::None && millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();

  app::update(event);   // el estado actual pinta LEDs y pantalla en memoria
  leds::show();
  display::push();
}
