#pragma once

#include <stddef.h>
#include <stdint.h>

#include "control/WorkoutJson.h"
#include "core/SessionSummary.h"

/**
 * Physio-Progression: Hauptteil-Dauer schrittweise erhoehen (WEBINTERFACE §5).
 * Arduino-frei.
 */
namespace ergo {

struct ProgressionOffer {
    bool pending = false;
    bool clean = false;
    char reason[56] = {};
    char workoutId[24] = {};
    char workoutName[40] = {};
    uint32_t currentMainS = 0;
    uint32_t nextMainS = 0;
    uint16_t stepS = 60;
    uint32_t maxS = 1800;
};

/** Sauber: done, keine Deckel-Eingriffe, Ist nahe Soll (±12 %). */
bool progressionIsClean(const SessionSummary& s, char* reason, size_t reasonLen);

uint32_t progressionNextMainS(uint32_t currentS, uint16_t stepS, uint32_t maxS);

/** Setzt steps[spec.stepIndex].durationS, wenn Progression aktiv und mainS>0. */
void progressionApplyMain(WorkoutDoc& doc, uint32_t mainS);

/** Index des Hauptteils: Label „Hauptteil“ oder mittlerer/laengster Schritt. */
uint8_t progressionResolveStepIndex(const WorkoutDoc& doc);

}  // namespace ergo
