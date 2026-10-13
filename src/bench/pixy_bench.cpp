/*  pixy_bench.cpp - pixy2 SPI bench test. No FC, no MAVLink, doesn't
    command anything.

    flash:  pio run -e pixy_bench -t upload
    watch:  pio device monitor -e pixy_bench

    Same wiring as flight: pixy on VSPI (MISO 19, SCK 18, MOSI 23), no CS
    wire, PixyMon interface set to "Arduino ICSP SPI".

    Should print init result 0, then a line every 200 ms. With a trained
    signature in view you get a row per block, otherwise "0 blocks". */

#include <Arduino.h>
#include <Pixy2.h>

static Pixy2 pixy;
static bool  pixyOk = false;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("[BENCH] Pixy2 SPI bench test");

    int8_t res = pixy.init();
    Serial.printf("[BENCH] pixy.init() = %d (%s)\n", res,
                  res == PIXY_RESULT_OK      ? "OK" :
                  res == PIXY_RESULT_TIMEOUT ? "TIMEOUT - no answer on SPI" : "ERROR");
    if (res < 0) {
        Serial.println("[BENCH] Check wiring (MISO 19, SCK 18, MOSI 23, GND, 5V) and the "
                       "\"Arduino ICSP SPI\" interface setting in PixyMon.");
        return;
    }

    if (pixy.getVersion() >= 0 && pixy.version) {
        Serial.printf("[BENCH] Pixy2 hw 0x%04x fw %d.%d.%d %s\n",
                      pixy.version->hardware, pixy.version->firmwareMajor,
                      pixy.version->firmwareMinor, pixy.version->firmwareBuild,
                      pixy.version->firmwareType);
    }

    res = pixy.changeProg("ccc");
    Serial.printf("[BENCH] changeProg(\"ccc\") = %d\n", res);
    pixyOk = (res >= 0);
}

void loop() {
    static uint32_t lastMs = 0;
    uint32_t now = millis();
    if (now - lastMs < 200) return;
    lastMs = now;

    if (!pixyOk) {
        Serial.println("[BENCH] Pixy2 not initialised - fix wiring and reset");
        delay(1800);
        return;
    }

    int8_t n = pixy.ccc.getBlocks();
    if (n < 0) {
        Serial.printf("[BENCH] getBlocks() error %d\n", n);
        return;
    }
    Serial.printf("[BENCH] t=%lu ms  %d blocks\n", (unsigned long)now, n);
    for (int i = 0; i < n; i++) {
        const Block& b = pixy.ccc.blocks[i];
        Serial.printf("        sig=%u  x=%u  y=%u  w=%u  h=%u\n",
                      b.m_signature, b.m_x, b.m_y, b.m_width, b.m_height);
    }
}
