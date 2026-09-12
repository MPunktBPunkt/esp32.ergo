#include "core/Progression.h"

#include <stdio.h>
#include <string.h>

namespace ergo {

bool progressionIsClean(const SessionSummary& s, char* reason, size_t reasonLen) {
    auto set = [&](const char* msg) {
        if (reason && reasonLen) {
            strncpy(reason, msg, reasonLen - 1);
            reason[reasonLen - 1] = 0;
        }
        return false;
    };
    if (!s.valid) return set("keine Session");
    if (strcmp(s.endReason, "done") != 0) return set("nicht bis zum Ende gefahren");
    if (s.interventions > 0) return set("Pulsdeckel hat eingegriffen");
    if (s.avgDesiredW > 5.0f && s.avgPowerW > 0.0f) {
        const float ratio = s.avgPowerW / s.avgDesiredW;
        if (ratio < 0.88f) return set("Zielleistung nicht gehalten");
    }
    if (reason && reasonLen) {
        strncpy(reason, "sauber", reasonLen - 1);
        reason[reasonLen - 1] = 0;
    }
    return true;
}

uint32_t progressionNextMainS(uint32_t currentS, uint16_t stepS, uint32_t maxS) {
    if (stepS == 0) stepS = 60;
    if (maxS == 0) maxS = 1800;
    if (currentS == 0) currentS = stepS;
    uint32_t next = currentS + stepS;
    if (next > maxS) next = maxS;
    return next;
}

uint8_t progressionResolveStepIndex(const WorkoutDoc& doc) {
    if (doc.progression.enabled && doc.progression.stepIndex < doc.stepCount)
        return doc.progression.stepIndex;
    for (uint8_t i = 0; i < doc.stepCount; i++) {
        if (strstr(doc.steps[i].label, "Haupt") != nullptr) return i;
    }
    if (doc.stepCount >= 3) return 1;
    uint8_t best = 0;
    uint32_t bestD = 0;
    for (uint8_t i = 0; i < doc.stepCount; i++) {
        if (doc.steps[i].durationS > bestD) {
            bestD = doc.steps[i].durationS;
            best = i;
        }
    }
    return best;
}

void progressionApplyMain(WorkoutDoc& doc, uint32_t mainS) {
    if (!doc.progression.enabled || mainS == 0 || doc.stepCount == 0) return;
    const uint8_t idx = progressionResolveStepIndex(doc);
    doc.steps[idx].durationS = mainS;
    doc.progression.stepIndex = idx;
    if (doc.progression.baseDurationS == 0) doc.progression.baseDurationS = mainS;
}

}  // namespace ergo
