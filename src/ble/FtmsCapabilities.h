#pragma once

#include "FtmsTypes.h"

/**
 * Was das angeschlossene Geraet tatsaechlich kann — hergeleitet aus dem, was
 * es meldet, und aus dem, was beim Verbinden beobachtet wurde.
 *
 * Der Sinn dieser Schicht: nichts oberhalb von FtmsClient darf wissen, dass
 * an diesem Stand ein Hammer Varon XTR II haengt. Ein anderes Ergometer, ein
 * echter Smarttrainer oder ein zweites Bike im Haushalt muessen ohne Reflash
 * laufen. Alles Geraetespezifische ist entweder ein gemeldetes Feature, ein
 * gelesener Bereich oder eine gemessene Eigenart — nie eine Konstante im
 * Code.
 *
 * Arduino-frei und damit nativ testbar.
 */
namespace ftms {

/** Wie eine Wattvorgabe umgesetzt wird. */
enum class PowerStrategy : uint8_t {
    None = 0,          // kein Stellweg — reines Dashboard
    DirectTarget,      // 0x05 Set Target Power, dem Geraet wird geglaubt
    EmulateResistance, // ueber 0x04, braucht die Kennflaeche
};

/**
 * Zwei Lesarten von 0x04 Set Target Resistance Level. Die Spec kennt uint8
 * mit 0,1er-Aufloesung; verbreitet ist sint16 in denselben Einheiten. Welche
 * ein Geraet versteht, ist Erfahrungswissen und gehoert ins Geraeteprofil,
 * nicht in eine `#define`.
 */
enum class ResistanceFormat : uint8_t {
    Unknown = 0,
    Uint8,   // 04 <uint8>
    Sint16,  // 04 <sint16 LE> — was der Varon XTR II braucht
};

struct Capabilities {
    bool valid = false;

    // --- aus 0x2ACC gemeldet
    bool canTargetPower = false;
    bool canTargetResistance = false;
    bool canTargetHeartRate = false;
    bool canSimulate = false;
    bool reportsPower = false;
    bool reportsCadence = false;
    bool reportsHeartRate = false;

    // --- aus 0x2AD6 / 0x2AD8 gelesen
    bool hasResistanceRange = false;
    ResistanceRange resistance;
    bool hasPowerRange = false;
    PowerRange power;

    // --- beim Verbinden beobachtet
    /** Control Point meldet per Notify statt Indicate (Varon tut das). */
    bool controlPointNotify = false;
    bool sawIndoorBikeData = false;
    uint16_t observedIbdFlags = 0;
    /** Liefert 0x2AD2 ein Resistance-Level-Feld? Sonst ist die Stufe ein
     *  Schattenwert, den nur der ESP kennt. */
    bool ibdReportsResistance = false;
    bool ibdReportsPower = false;
    bool ibdReportsCadence = false;
    bool ibdReportsHeartRate = false;

    // --- Betriebsentscheidungen
    ResistanceFormat resistanceFormat = ResistanceFormat::Unknown;

    /**
     * Ob dem gemeldeten Wattziel geglaubt wird.
     *
     * Nicht dasselbe wie `canTargetPower`: der Varon quittiert `05` mit
     * Success, obwohl er das Feature nicht meldet und keine 0x2AD8 liefert.
     * Umgekehrt gibt es Geraete, die das Feature melden und es koennen.
     * Die Regel ist deshalb "melden UND den Bereich veroeffentlichen" — wer
     * kein 0x2AD8 hat, hat auch keine belastbare Wattsteuerung.
     * Per Geraeteprofil in beide Richtungen ueberschreibbar.
     */
    bool powerTargetTrusted = false;

    PowerStrategy powerStrategy() const;

    /** 0, wenn kein Bereich bekannt ist. */
    uint16_t levelCount() const;
    int16_t levelMinTenths() const;
    int16_t levelMaxTenths() const;
    uint16_t levelStepTenths() const;

    /** Kann die gewuenschte Stufe ueberhaupt in uint8-Zehnteln stehen? */
    bool needsWideResistance() const;
};

/**
 * Leitet die gemeldeten Faehigkeiten ab. `res` und `pow` duerfen null sein —
 * genau das ist der Normalfall bei einem Geraet ohne 0x2AD8.
 */
Capabilities deriveCapabilities(const FeatureSet& feat, const ResistanceRange* res,
                                const PowerRange* pow);

/** Ergaenzt, was erst am laufenden Datenstrom sichtbar wird. */
void noteIndoorBikeData(Capabilities& c, const IndoorBikeData& d);

const char* powerStrategyName(PowerStrategy s);
const char* resistanceFormatName(ResistanceFormat f);

}  // namespace ftms
