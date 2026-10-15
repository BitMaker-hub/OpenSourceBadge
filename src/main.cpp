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
#include "app/badge.h"
#include "net/setup_portal.h"
#include "net/nostr_zaps.h"

static const uint32_t FRAME_MS = 33;   // ~30 imágenes por segundo

// Para probar desde el ordenador con el monitor serie:
//   p / m / s  = PEER / MODE / SEED corto
//   P / M / S  = los mismos, pulsación larga
//   z          = simular que llega un zap
//   d          = cargar los datos de demo del prototipo (no se guardan)
//   n042       = grabar el número de serie del badge (fábrica)
//   Z          = poner a cero el contador de sats recibidos
//   X          = borrar perfil y peers (dejarlo como nuevo)
//   c          = captura de la pantalla (la lee tools/captura.py)
//   J{...}     = probar un mensaje de relay de Nostr (un recibo de zap en una línea)
//   q          = escribir los QR del badge con '#' y '.' para comprobarlos
static bool screenshotPending = false;

static ButtonEvent readSerialCommand() {
  if (!Serial.available()) return ButtonEvent::None;
  switch (Serial.read()) {
    case 'd': badge::loadDemo(); return ButtonEvent::None;
    case 'n': {
      const int n = Serial.parseInt();
      if (n > 0 && n < 1000) { badge::setNumber(n); Serial.printf("Badge #%03d\n", n); }
      return ButtonEvent::None;
    }
    case 'c': screenshotPending = true; return ButtonEvent::None;
    case 'J': nostrzaps::injectForTest(Serial.readStringUntil('\n')); return ButtonEvent::None;
    case 'q': {
      display::qrToSerial(portal::wifiQr());
      QrItem items[badge::MAX_QRS];
      const int n = badge::qrItems(items);
      for (int i = 0; i < n; i++) display::qrToSerial(items[i].data);
      return ButtonEvent::None;
    }
    case 'Z': badge::resetSats(); Serial.println("Sats a cero"); return ButtonEvent::None;
    case 'X': badge::factoryReset(); Serial.println("Badge borrado"); return ButtonEvent::None;
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
  Serial.setRxBufferSize(8192);   // para el comando J: un recibo de zap ocupa varios KB
  Serial.begin(115200);
  power::begin();     // lo primero: sin GPIO15 alto, con batería no hay pantalla
  buttons::begin();
  leds::begin();
  qwiic::begin();
  display::begin();
  app::begin();
  Serial.println("Open Source Badge listo. Teclas: p m s (P M S largas), z zap, d demo, n042 numero, X borrar");
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

  if (screenshotPending) {
    display::screenshotToSerial();
    screenshotPending = false;
  }
}
