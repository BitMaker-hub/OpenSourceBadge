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

## Peer cercano

Cada badge manda por ESP-NOW un "estoy aquí" cada segundo (`net/nearby`). Con la fuerza de la señal se sabe si
otro badge está pegado (más de -60 dBm, un palmo o dos): sale un aviso abajo ("NEARBY · Satoshi · PEER TO
CONNECT", o "ALREADY PEERS" en verde) y TX/RX laten en morado. Al pulsar PEER con alguien pegado, al otro le llega
una invitación (pantalla INVITE): PEER acepta y los dos entran en el handshake.

Los badges conectados a una WiFi están en el canal de esa WiFi; los que no tienen WiFi saltan de canal (primero
1, 6 y 11) hasta oír a alguien y se quedan en su canal. Probado con dos badges: se encuentran en ~5 s.

La radio la reparte `net/radio`: en NORMAL hay WiFi (si está configurada) y peer cercano; el handshake, el SETUP y
el Seed Lab la piden en EXCLUSIVO y se apaga todo lo demás.

## Zaps de Nostr

En `badge.local` se pone el npub y una WiFi con internet (la del evento o el hotspot del móvil). El badge se
conecta a relay.damus.io y relay.primal.net y se suscribe a los recibos de zap (kind 9735) dirigidos a ese npub.
Cuando llega uno enseña la cantidad, quién lo manda (su nombre de Nostr si lo encuentra) y el mensaje, y enciende
la ₿. Los sats van a la wallet de cada uno: el badge solo escucha. El punto morado de la cabecera indica que está
escuchando; gris, que está conectando.

Para probar sin internet: `J` seguido del mensaje del relay en una línea (ver `tools/`).

## Seed Lab

Para aprender cómo nace una semilla. Se elige dado (50 o 99 tiradas) o moneda (128 o 256), 12 o 24 palabras, y se
mete la entropía a mano con un dado o una moneda de verdad (moneda: PEER cara, MODE cruz; dado: PEER cambia la cara,
MODE apunta). El badge saca las palabras BIP39, la primera dirección (m/84'/0'/0'/0/0) y la entropía, igual que el
Seeder (las tiradas se pasan por SHA-256 como en Coldcard). Comprobado contra embit con las mismas tiradas.

Mientras dura: radio apagada, capturas por el puerto serie bloqueadas y todo borrado de la memoria al salir. Es para
aprender: el badge tiene radio y para fondos de verdad hay que usar un dispositivo sin conexión, como el Seeder.

## Pendiente

- Batería: medir consumo y apagar o atenuar pantalla y WiFi.
- Modo NerdMiner.
- Avisar de pagos Lightning que no vengan de Nostr (Nostr Wallet Connect).
