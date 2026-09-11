#include <Arduino.h>

#include "BuildFlags.h"
#include "ble/FtmsCodec.h"

/**
 * Platzhalter, bis App existiert. Haelt den S3-Build linkfaehig, damit
 * Build-Flags und Bibliotheksversionen schon jetzt geprueft werden koennen.
 */
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("\nesp32.ergo %s auf %s\n", FW_VERSION, ERGO_BOARD_LABEL);

    // Selbsttest des Codecs gegen ein aufgezeichnetes Paket des Varon XTR II.
    const uint8_t pkt[] = {0x54, 0x0B, 0x98, 0x08, 0x76, 0x00, 0x13, 0x0A, 0x00, 0x6C,
                           0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x51, 0xC3, 0x01};
    ftms::IndoorBikeData d;
    if (ftms::decodeIndoorBikeData(pkt, sizeof(pkt), d) == ftms::IbdStatus::Ok) {
        Serial.printf("Codec: %.0f W, %.1f rpm, %.2f km/h, %u bpm\n", (double)d.powerW,
                      (double)d.cadenceRpm(), (double)d.speedKmh(), d.heartRateBpm);
    } else {
        Serial.println("Codec: FEHLER");
    }
}

void loop() {
    delay(1000);
}
