#pragma once

#include <stdint.h>

#include "control/PowerMap.h"

/**
 * ERG-Emulation: Zielwatt + aktuelle Kadenz → Widerstandsstufe.
 *
 * Vorsteuerung aus der Kennflaeche (`PowerMap::bestLevel`) plus langsamer
 * Integralterm auf dem Leistungsfehler — Bauform laut PFLICHTENHEFT §6.
 * Arduino-frei; Zeit und Messwerte kommen als Parameter.
 *
 * Die Stufe ist diskret (16 Schritte). Das Watt-Ziel wird im Regelfall nicht
 * exakt getroffen; `ceiling` sagt, wenn selbst die hoechste bekannte Stufe
 * nicht reicht.
 *
 * Stufenjagd-Schutz (0.3.15): max. eine Stufe pro Regelzyklus Richtung Ziel;
 * kleine Ziel-Updates (MyWhoosh) loesen keinen Sofort-Sprung aus.
 */
namespace ergo {

struct PowerControllerConfig {
    /** Mindestabstand zwischen zwei Stufenwechseln. */
    uint32_t periodMs = 6000;
    /** I-Anteil: Watt Korrektur pro Sekunde und Watt Fehler. */
    float iGain = 0.05f;
    /** Integral geklemmt auf ± so viele Watt. */
    float iLimitW = 40.0f;
    /** Unterhalb dieses |Fehlers| kein Integral. */
    float deadbandW = 8.0f;
    /** Glättung der Ist-Leistung (Zeitkonstante, Sekunden). */
    float smoothTauS = 5.0f;
    /**
     * Max. Stufenwechsel pro Write in Zehnteln (10 = 1 Stufe).
     * 0 = unbegrenzter Sprung (nur Tests / Sonderfälle).
     */
    uint16_t maxStepTenths = 10;
    /**
     * |ΔZielwatt| ab dem I-Reset und sofortiger Write erlaubt wird.
     * Darunter nur Zielwert aktualisieren (Bridge-Spam).
     */
    float retargetW = 20.0f;
};

class PowerController {
public:
    struct Tick {
        bool wantWrite = false;
        int16_t levelTenths = -1;
        bool ceiling = false;
        bool mapReady = false;
        float targetW = 0.0f;
        float effectiveTargetW = 0.0f;  // inkl. Integral
        float smoothedW = 0.0f;
        float estimatedW = 0.0f;  // Map-Schätzung für gewählte Stufe
        /** Map-Wunschstufe vor Slew (Diagnose). */
        int16_t desiredTenths = -1;
    };

    void begin(const PowerControllerConfig& cfg = {});
    void reset();

    void setTargetW(float watt);
    float targetW() const { return targetW_; }
    bool hasTarget() const { return targetW_ > 0.0f; }
    bool ceiling() const { return ceiling_; }
    float smoothedW() const { return smoothW_; }
    int16_t lastLevelTenths() const { return lastLevel_; }

    /**
     * `fresh` = aktuelles 0x2AD2. Ohne frische Daten kein Integral und kein
     * neuer Write — sonst regelt man auf veraltete Watt.
     */
    Tick tick(uint32_t nowMs, float rpm, float watt, bool fresh, const PowerMap& map);

private:
    PowerControllerConfig cfg_{};
    float targetW_ = 0.0f;
    float integralW_ = 0.0f;
    float smoothW_ = 0.0f;
    bool smoothInit_ = false;
    int16_t lastLevel_ = -1;
    bool ceiling_ = false;
    uint32_t lastWriteMs_ = 0;
    uint32_t lastTickMs_ = 0;
};

}  // namespace ergo
