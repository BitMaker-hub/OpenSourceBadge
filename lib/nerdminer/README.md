# nerdminer (núcleo)

Copiado de [NerdMiner_v2](https://github.com/BitMaker-hub/NerdMiner_v2) v1.8.3 (MIT, © Bitmaker):
`stratum.*`, `utils.*`, `mining.h` y `nerdSHA256plus.*` van tal cual.

`miner_core.cpp` es `mining.cpp` recortado: solo el trabajador de stratum y el de SHA-256 por hardware del
ESP32-S3, sin pantallas ni WiFiManager, con arrancar/parar y la configuración que le pasa el badge.
