#include "FtmsCapabilities.h"

namespace ftms {

PowerStrategy Capabilities::powerStrategy() const {
    if (!valid) return PowerStrategy::None;
    if (canTargetPower && powerTargetTrusted) return PowerStrategy::DirectTarget;
    if (canTargetResistance && hasResistanceRange) return PowerStrategy::EmulateResistance;
    return PowerStrategy::None;
}

uint16_t Capabilities::levelCount() const {
    return hasResistanceRange ? resistance.levelCount() : 0;
}

int16_t Capabilities::levelMinTenths() const {
    return hasResistanceRange ? resistance.minRaw : 0;
}

int16_t Capabilities::levelMaxTenths() const {
    return hasResistanceRange ? resistance.maxRaw : 0;
}

uint16_t Capabilities::levelStepTenths() const {
    return hasResistanceRange ? resistance.stepRaw : 0;
}

bool Capabilities::needsWideResistance() const {
    // Hier stand eine Bereichsfrage: passt der Wert in ein uint8? Beim Varon
    // passt er (10 bis 160), also ging die schmale Form hinaus — das Geraet
    // quittierte sie mit Success und tat nichts. Genau die Falle, die
    // GERAETEPROFIL.md beschreibt: eine Erfolgsquittung beweist nichts.
    //
    // Der Stellbereich kann das Drahtformat nicht beantworten. Nach FTMS ist
    // Set Target Resistance sint16 in Zehnteln; die uint8-Form ist eine
    // Geraeteeigenschaft und muss aus einer Messung kommen, nicht aus einer
    // Herleitung. Deshalb ist Spec-Treue der Standard und die schmale Form die
    // ausdrueckliche Ausnahme.
    if (resistanceFormat == ResistanceFormat::Uint8) {
        // Auch dann nur, solange der Bereich ueberhaupt hineinpasst.
        return hasResistanceRange && (resistance.maxRaw > 255 || resistance.minRaw < 0);
    }
    return true;
}

Capabilities deriveCapabilities(const FeatureSet& feat, const ResistanceRange* res,
                                const PowerRange* pow) {
    Capabilities c;
    if (!feat.valid) return c;
    c.valid = true;

    c.canTargetPower = feat.hasTarget(kTgtPower);
    c.canTargetResistance = feat.hasTarget(kTgtResistance);
    c.canTargetHeartRate = feat.hasTarget(kTgtHeartRate);
    c.canSimulate = feat.hasTarget(kTgtIndoorBikeSimulation);

    c.reportsPower = feat.hasMachine(kFeatPowerMeasurement);
    c.reportsCadence = feat.hasMachine(kFeatCadence);
    c.reportsHeartRate = feat.hasMachine(kFeatHeartRate);

    if (res && res->valid) {
        c.hasResistanceRange = true;
        c.resistance = *res;
    }
    if (pow && pow->valid) {
        c.hasPowerRange = true;
        c.power = *pow;
    }

    // Wer ein Wattziel meldet, aber seinen Wattbereich nicht veroeffentlicht,
    // bekommt keinen Vertrauensvorschuss. Genau daran faellt der Varon durch,
    // obwohl er `05` mit Success quittiert.
    c.powerTargetTrusted = c.canTargetPower && c.hasPowerRange;

    // Spec-Treue als Ausgangspunkt, nicht als Herleitung: sint16. Wer die
    // schmale Form braucht, ist die Ausnahme, und diese Ausnahme kommt aus dem
    // Geraeteprofil — nach einer Messung, die belegt, dass die Stufe wirklich
    // greift, und nicht nach einer Quittung.
    if (c.canTargetResistance) {
        c.resistanceFormat = ResistanceFormat::Sint16;
    }

    return c;
}

void noteIndoorBikeData(Capabilities& c, const IndoorBikeData& d) {
    c.sawIndoorBikeData = true;
    c.observedIbdFlags = d.flags;
    c.ibdReportsResistance = d.has(kResistance);
    c.ibdReportsPower = d.has(kPower);
    c.ibdReportsCadence = d.has(kCadence);
    c.ibdReportsHeartRate = d.has(kHeartRate);
}

const char* powerStrategyName(PowerStrategy s) {
    switch (s) {
        case PowerStrategy::None: return "NONE";
        case PowerStrategy::DirectTarget: return "DIRECT";
        case PowerStrategy::EmulateResistance: return "EMULATE";
        default: return "UNKNOWN";
    }
}

const char* resistanceFormatName(ResistanceFormat f) {
    switch (f) {
        case ResistanceFormat::Uint8: return "uint8";
        case ResistanceFormat::Sint16: return "sint16";
        default: return "unknown";
    }
}

}  // namespace ftms
