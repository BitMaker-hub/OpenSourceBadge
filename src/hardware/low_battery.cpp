#include "low_battery.h"
#include "power.h"
#include "leds.h"
#include "backlight.h"
#include "../config.h"
#include "../ui/display.h"
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

namespace lowbattery {

static const uint16_t EMPTY_MV  = 3400;    // con carga; en reposo la celda está algo más alta
static const uint32_t FOR_MS    = 30000;   // un pico de consumo no cuenta: tiene que durar
static const uint16_t WAKE_OK_MV = 3500;   // al despertar, por debajo de esto vuelve a dormir

static uint16_t averaged() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) sum += power::batteryMillivolts();
  return sum / 16;
}

static void sleepNow() {
  Serial.printf("[bateria] vacía (%u mV): a dormir hasta que la carguen\n", averaged());

  // El aviso, unos segundos, con la luz baja
  display::clear();
  display::text("Battery empty", SCREEN_W / 2, 130, Font::Title, ink::Red, Align::Center);
  display::text("Charge me and", SCREEN_W / 2, 170, Font::Body, ink::Soft, Align::Center);
  display::text("press any button", SCREEN_W / 2, 188, Font::Body, ink::Soft, Align::Center);
  display::push();
  leds::clear();
  leds::show();                      // con todo en negro, corta la alimentación de los LEDs
  delay(4000);

  // Todo apagado y quieto mientras duerme: LEDs sin alimentación, pantalla sin luz ni corriente
  pinMode(38, OUTPUT);
  digitalWrite(38, LOW);
  digitalWrite(PIN_LED_POWER_OFF, HIGH);
  digitalWrite(PIN_POWER_ON, LOW);
  gpio_hold_en((gpio_num_t)PIN_LED_POWER_OFF);
  gpio_hold_en((gpio_num_t)PIN_POWER_ON);
  gpio_deep_sleep_hold_en();

  // Despierta con cualquiera de los tres botones (a GND al pulsar). Los botones
  // usan la resistencia interna del chip, que al dormir se apaga: se dejan las
  // del dominio RTC encendidas para que los pines no queden al aire.
  const uint64_t buttons = (1ULL << PIN_BTN_PEER) | (1ULL << PIN_BTN_MODE) | (1ULL << PIN_BTN_SEED);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  for (gpio_num_t pin : {(gpio_num_t)PIN_BTN_PEER, (gpio_num_t)PIN_BTN_MODE, (gpio_num_t)PIN_BTN_SEED}) {
    rtc_gpio_pullup_en(pin);
    rtc_gpio_pulldown_dis(pin);
  }
  esp_sleep_enable_ext1_wakeup(buttons, ESP_EXT1_WAKEUP_ANY_LOW);
  Serial.flush();
  esp_deep_sleep_start();
}

void begin() {
  // Al despertar de este sueño, los pines siguen sujetos: hay que soltarlos
  // antes de que power:: y leds:: los usen, o no les harían caso
  gpio_hold_dis((gpio_num_t)PIN_LED_POWER_OFF);
  gpio_hold_dis((gpio_num_t)PIN_POWER_ON);
  gpio_deep_sleep_hold_dis();
}

void checkAtBoot() {
  if (!power::usbConnected() && averaged() < WAKE_OK_MV) sleepNow();
}

void loop(bool usb) {
  static uint32_t lowSince = 0;
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck < 1000) return;   // una vez por segundo basta
  lastCheck = millis();
  if (usb || averaged() >= EMPTY_MV) { lowSince = 0; return; }
  if (lowSince == 0) lowSince = millis();
  if (millis() - lowSince > FOR_MS) sleepNow();
}

}  // namespace lowbattery
