#include "control/SimAssist.h"

#include <math.h>

namespace ergo {

int16_t simAssistGradeHundredth(float targetW, float smoothedW, bool ceiling,
                               const SimAssistConfig& cfg) {
    if (!ceiling || targetW <= 0.0f) return 0;
    if (!(smoothedW >= 0.0f)) return 0;

    SimAssistConfig c = cfg;
    if (c.wattsPerGradePct < 1.0f) c.wattsPerGradePct = 1.0f;
    if (c.maxGradePct < 0.0f) c.maxGradePct = 0.0f;
    if (c.maxGradePct > 20.0f) c.maxGradePct = 20.0f;
    if (c.minDeficitW < 0.0f) c.minDeficitW = 0.0f;

    const float deficit = targetW - smoothedW;
    if (deficit < c.minDeficitW) return 0;

    float pct = deficit / c.wattsPerGradePct;
    if (pct > c.maxGradePct) pct = c.maxGradePct;
    // Unter ~2 % oft wirkungslos — mind. 2 % wenn Assist greift, sonst 0.
    if (pct > 0.0f && pct < 2.0f) pct = 2.0f;

    const long h = lroundf(pct * 100.0f);
    if (h <= 0) return 0;
    if (h > 2000) return 2000;
    return (int16_t)h;
}

}  // namespace ergo
