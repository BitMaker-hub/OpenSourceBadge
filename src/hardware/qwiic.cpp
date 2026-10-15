#include "qwiic.h"
#include "../config.h"
#include <Wire.h>

namespace qwiic {

void begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setTimeOut(20);   // si no hay nada conectado, que no se quede esperando
}

uint8_t scan(uint8_t* found, uint8_t max) {
  uint8_t n = 0;
  for (uint8_t addr = 0x08; addr < 0x78 && n < max; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) found[n++] = addr;
  }
  return n;
}

}  // namespace qwiic
