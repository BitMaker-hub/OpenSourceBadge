# Open Source Badge · Proof of Peer

Firmware del badge de Bitcoin Amsterdam 2026: una placa HAT con una ₿ de 18 LEDs, dos LEDs TX/RX y tres
botones (PEER, MODE, SEED) encima de un LilyGO T-Display-S3.

Diseñado por Bitmaker · [bitronics.store](https://bitronics.store)

## Compilar y grabar

```
pio run -t upload
pio device monitor
```

## Cómo se usa

| Botón | Corto | Largo |
|---|---|---|
| **MODE** | Página siguiente: perfil → QR → peers → stats | Setup: WiFi abierta BADGE-xxx y badge.local |
| **PEER** | Handshake con otro badge | QR a pantalla completa |
| **SEED** | Seed Lab | Prueba de placa |

- **Prueba de placa:** se entra manteniendo SEED pulsado al encender, o con SEED largo. Recorre los 20 LEDs en
  rojo, verde y azul, enseña los botones pulsados, el voltaje del USB y de la batería, el consumo de los LEDs y lo
  que haya en el Qwiic. PEER pausa y MODE avanza un paso.
- **Desde el ordenador** (monitor serie): `p` `m` `s` son las pulsaciones cortas, `P` `M` `S` las largas y `z`
  simula un zap.

## Cómo está organizado

```
src/
  config.h               pines y límites de la placa (rev0.9-final)
  main.cpp               el loop: botones -> estado -> LEDs y pantalla
  app/state_machine      QUÉ hace el badge en cada momento. Empezar a leer por aquí
  app/badge              los datos: perfil, peers, sats recibidos
  hardware/              botones, LEDs, alimentación, Qwiic
  effects/led_effects    efectos de LEDs que sirven para cualquier grupo (la B, TX, RX...)
  ui/display             piezas comunes de las pantallas (cabecera, tarjeta, chip, QR)
  ui/screens             una función por pantalla
  ui/avatars             los personajes en pixel art
tools/exportar_sprites.js  saca los personajes del prototipo de pantallas
```

### La máquina de estados

El badge siempre está en un estado. Cada estado tiene una función que, en cada vuelta del loop:

1. mira los botones y el tiempo, y decide si cambia de estado (`go(...)`);
2. enciende los LEDs que le tocan;
3. dibuja su pantalla.

Lo que un estado no enciende queda apagado, así que no hay que acordarse de apagar nada al salir.

### Los LEDs

Los efectos trabajan sobre **grupos**: `groups::B`, `groups::BOutline`, `groups::Tx`, `groups::Rx`...
El mismo `effects::trail(...)` hace la estela de un zap por la B o un barrido por TX/RX.

`leds::show()` aplica el brillo y **limita el consumo a 200 mA**. Los LEDs comen del regulador de 3V3 del
T-Display (600 mA, compartido con el ESP32), así que ningún efecto puede pasarse. Si todos están apagados,
corta la alimentación de los LEDs (Q1).

## Pendiente

- Perfil guardado en la memoria (NVS), WiFi propia y página `badge.local`.
- Handshake de verdad por ESP-NOW (ahora el peer es inventado).
- Monedero LNbits y zaps de verdad (ahora `z` los simula).
- Las fuentes del prototipo (VT323 y Silkscreen); ahora se usan las de TFT_eSPI.
