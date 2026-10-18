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
| **SEED** | LAB: Seed Lab o NerdMiner | Prueba de placa |

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

## Monedero LNbits

Cada badge puede llevar su monedero LNbits (`net/lnbits`): su dirección Lightning sale en el QR, y por un websocket
con el servidor se entera al momento de cada pago (normal o zap de Nostr a su dirección) y lo celebra. El badge solo
tiene la clave de lectura. En STATS, PEER largo abre WITHDRAW: el QR de retirada (LNURL-withdraw) de ese monedero,
detrás de un aviso. Si un mismo pago llega por LNbits y por Nostr, sale una sola vez (se reconoce por la factura).

- Grabar un badge: `py tools/grabar_badge.py COM67 42` (crea el monedero, la dirección y el enlace de retirada, y lo
  graba con los comandos serie `n`, `L` y `W`). Necesita `tools/.env` (ver `tools/.env.ejemplo`). **Sin probar aún
  contra el LNbits de verdad.**
- Probar sin servidor: `py tools/lnbits_falso.py 5055` imita a LNbits; `/pagar` y `/zap` lanzan pagos de prueba.

## NerdMiner

`lib/nerdminer` es el núcleo del [NerdMiner_v2](https://github.com/BitMaker-hub/NerdMiner_v2) (MIT): stratum, SHA-256
y el trabajador por hardware del ESP32-S3, recortado en `miner_core.cpp` (sin pantallas ni WiFiManager, con
arrancar/parar). `net/miner` lo envuelve con el pool y la dirección (en `badge.local` o con el comando serie
`Gpool|puerto|direccion`). Se entra desde LAB (SEED), y
cuando la radio está en exclusivo (handshake, SETUP, Seed Lab) espera y sigue solo.

- **De fábrica** mina en `pool.bitronics.store:3334` (el pool de NerdMiners de Bitronics) para la dirección de
  Bitronics, worker `nerdminerBadge`: se mina sin configurar nada. Con una dirección propia en `badge.local`, el
  premio es del dueño; la pantalla dice siempre a quién va.
- Unos **140 kH/s**, a la vez que escucha Nostr y LNbits. "Shares" son los que acepta el pool (los mismos que
  enseña su web); los de 32 bits del NerdMiner original salen en el comando `i`.
- Mientras mina se quita del watchdog la tarea de reposo del núcleo 0 (como hace el NerdMiner); al parar se vuelve a
  poner. Sin eso el chip se reiniciaba a los 5 s.
- **Cuándo mina:** solo mientras se mira su pantalla, con USB o sin él (decisión de Roger: ni gasta batería ni
  calienta a escondidas). Al salir, para.
- **Dos páginas** (PEER o MODE cambian, SEED vuelve), siempre con el logotipo del NerdMiner arriba a la izquierda:
  el minero (hashrate, gráfica, shares, dificultades, tiempo) y la red Bitcoin de mempool.space (precio, bloque,
  comisión, dificultad, hashrate y una barra vertical hasta el halving; `net/bitcoin_info`, solo mientras se mira).
- **Avisos:** "New job received" con una vuelta de 4 LEDs azules por la B, y "Share accepted!" con un parpadeo
  verde de RX. Nunca la B entera de golpe: con el chip minando, el salto de consumo hace pitar la fuente.

## Seed Lab

Para aprender cómo nace una semilla. Se elige dado (50 o 99 tiradas) o moneda (128 o 256), 12 o 24 palabras, y se
mete la entropía a mano con un dado o una moneda de verdad (moneda: PEER cara, MODE cruz; dado: PEER cambia la cara,
MODE apunta). El badge saca las palabras BIP39, la primera dirección (m/84'/0'/0'/0/0) y la entropía, igual que el
Seeder (las tiradas se pasan por SHA-256 como en Coldcard). Comprobado contra embit con las mismas tiradas.

Mientras dura: radio apagada, capturas por el puerto serie bloqueadas y todo borrado de la memoria al salir. Es para
aprender: el badge tiene radio y para fondos de verdad hay que usar un dispositivo sin conexión, como el Seeder.

## Si se reinicia

El comando `i` dice cuánto lleva encendido y por qué se reinició la última vez. Si fue un fallo (panic), el volcado
queda en la partición `coredump` y se lee con el ELF de esa misma compilación:

    esptool.py --chip esp32s3 --port COM67 read_flash 0xFF0000 0x10000 core.bin
    python -m esp_coredump --chip esp32s3 info_corefile -t raw -c core.bin .pio/build/badge/firmware.elf

Así se encontró el fallo del 2026-10-18: al entrar en el Seed Lab la radio apagaba la WiFi mientras Nostr aún
cerraba sus conexiones TLS. Ahora Nostr, LNbits y el minero avisan con `radio::busy()` y la radio espera a que
todos hayan cerrado antes de apagar.

## Pendiente

- Batería: medir consumo y apagar o atenuar pantalla y WiFi.
- Avisar de pagos Lightning que no vengan de Nostr (Nostr Wallet Connect).
