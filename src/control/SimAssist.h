#pragma once

#include <stdint.h>

/**
 * Feine Zusatzlast ueber 0x11, wenn die Stufendecke das Wattziel nicht erreicht.
 *
 * Nachtest 2026-09-13: ab ~3 % Grade messbar (~+25 % W/rpm bei Stufe 7);
 * 1 % oft NO_EFFECT. Deshalb: nur bei `ceiling`, Mindest-Defizit, Max 6 %.
 * Arduino-frei.
 */
namespace ergo {

struct SimAssistConfig {
    /** Grobe Kalibrierung aus Nachtest 4: ~8 W je 1 % bei mittlerer Last. */
    float wattsPerGradePct = 8.0f;
    float maxGradePct = 6.0f;
    /** Unterhalb lohnt 0x11 nicht (1 %-Totzone). */
    float minDeficitW = 15.0f;
};

/**
 * Zielsteigung in 0,01 %. 0 = keine Assist-Last.
 * Nur wenn `ceiling` und Defizit gross genug.
 */
int16_t simAssistGradeHundredth(float targetW, float smoothedW, bool ceiling,
                               const SimAssistConfig& cfg = {});

}  // namespace ergo
