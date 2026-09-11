#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * FTMS-Datentypen und Konstanten — bewusst ohne Arduino-Abhaengigkeit,
 * damit FtmsCodec nativ getestet werden kann (env:native).
 *
 * Quelle der Wahrheit fuer das Verhalten dieses konkreten Geraets ist
 * docs/ergometer/GERAETEPROFIL.md. Die Abweichungen vom Standard sind
 * unten an der jeweiligen Stelle vermerkt.
 */
namespace ftms {

// ---------------------------------------------------------------- UUIDs

constexpr uint16_t kSvcFitnessMachine = 0x1826;
constexpr uint16_t kChrFeature = 0x2ACC;          // Fitness Machine Feature
constexpr uint16_t kChrIndoorBikeData = 0x2AD2;   // Indoor Bike Data
constexpr uint16_t kChrResistanceRange = 0x2AD6;  // Supported Resistance Level
constexpr uint16_t kChrPowerRange = 0x2AD8;       // Supported Power Range
constexpr uint16_t kChrControlPoint = 0x2AD9;     // Control Point
constexpr uint16_t kChrStatus = 0x2ADA;           // Fitness Machine Status

// ------------------------------------------------------- Indoor Bike Data

/**
 * Praesenzmaske. Die Bitpositionen entsprechen den Flag-Bits aus 0x2AD2 —
 * mit der einen Ausnahme, die dieses Protokoll beruehmt macht:
 *
 *   Flag-Bit 0 ist INVERTIERT. Geloescht heisst "Instantaneous Speed
 *   vorhanden". In der Praesenzmaske ist Speed dagegen normal gesetzt,
 *   wenn das Feld da ist.
 */
enum Field : uint16_t {
    kSpeed = 1u << 0,
    kAvgSpeed = 1u << 1,
    kCadence = 1u << 2,
    kAvgCadence = 1u << 3,
    kDistance = 1u << 4,
    kResistance = 1u << 5,
    kPower = 1u << 6,
    kAvgPower = 1u << 7,
    kEnergy = 1u << 8,
    kHeartRate = 1u << 9,
    kMet = 1u << 10,
    kElapsedTime = 1u << 11,
    kRemainingTime = 1u << 12,
};

/** Laengstmoegliches 0x2AD2-Paket: alle Felder gesetzt, Flag-Bit 0 geloescht. */
constexpr size_t kMaxIndoorBikeLen = 30;

/**
 * Rohwerte in Wire-Einheiten. Bewusst keine floats im Struct: der Debug-Modus
 * protokolliert die Bytes, und eine Umrechnung, die man nicht rueckgaengig
 * machen kann, kostet genau dort die Beweiskraft. Skalierte Werte liefern die
 * Accessors.
 */
struct IndoorBikeData {
    uint16_t flags = 0;
    uint16_t presence = 0;  // Field-Maske

    uint16_t speedRaw = 0;      // 0,01 km/h
    uint16_t avgSpeedRaw = 0;   // 0,01 km/h
    uint16_t cadenceRaw = 0;    // 0,5 rpm
    uint16_t avgCadenceRaw = 0; // 0,5 rpm
    uint32_t distanceM = 0;     // uint24, Meter
    int16_t resistanceRaw = 0;  // einheitenlos laut Spec
    int16_t powerW = 0;
    int16_t avgPowerW = 0;
    uint16_t energyTotalKcal = 0;
    uint16_t energyPerHourKcal = 0;
    uint8_t energyPerMinKcal = 0;
    uint8_t heartRateBpm = 0;
    uint8_t metRaw = 0;  // 0,1
    uint16_t elapsedS = 0;
    uint16_t remainingS = 0;

    uint8_t consumed = 0;        // ausgewertete Bytes
    uint8_t trailing = 0;        // unerwarteter Rest
    uint16_t truncatedField = 0; // Field, an dem das Paket ausging; 0 = keins

    bool has(Field f) const { return (presence & f) != 0; }

    float speedKmh() const { return speedRaw / 100.0f; }
    float avgSpeedKmh() const { return avgSpeedRaw / 100.0f; }
    float cadenceRpm() const { return cadenceRaw / 2.0f; }
    float avgCadenceRpm() const { return avgCadenceRaw / 2.0f; }
    float met() const { return metRaw / 10.0f; }
};

enum class IbdStatus : uint8_t {
    Ok = 0,
    TooShort,       // weniger als 2 Byte, nicht einmal die Flags
    FieldTruncated, // Flags versprechen ein Feld, das Paket ist zu kurz
};

// ------------------------------------------------------------- Feature

/** 0x2ACC, erste uint32. */
enum MachineFeature : uint32_t {
    kFeatAvgSpeed = 1u << 0,
    kFeatCadence = 1u << 1,
    kFeatTotalDistance = 1u << 2,
    kFeatInclination = 1u << 3,
    kFeatElevationGain = 1u << 4,
    kFeatPace = 1u << 5,
    kFeatStepCount = 1u << 6,
    kFeatResistanceLevel = 1u << 7,
    kFeatStrideCount = 1u << 8,
    kFeatExpendedEnergy = 1u << 9,
    kFeatHeartRate = 1u << 10,
    kFeatMetabolicEquivalent = 1u << 11,
    kFeatElapsedTime = 1u << 12,
    kFeatRemainingTime = 1u << 13,
    kFeatPowerMeasurement = 1u << 14,
    kFeatForceOnBelt = 1u << 15,
    kFeatUserDataRetention = 1u << 16,
};

/** 0x2ACC, zweite uint32 — was sich ueber den Control Point stellen laesst. */
enum TargetFeature : uint32_t {
    kTgtSpeed = 1u << 0,
    kTgtInclination = 1u << 1,
    kTgtResistance = 1u << 2,
    kTgtPower = 1u << 3,
    kTgtHeartRate = 1u << 4,
    kTgtExpendedEnergy = 1u << 5,
    kTgtStepNumber = 1u << 6,
    kTgtStrideNumber = 1u << 7,
    kTgtDistance = 1u << 8,
    kTgtTrainingTime = 1u << 9,
    kTgtTimeTwoHrZones = 1u << 10,
    kTgtTimeThreeHrZones = 1u << 11,
    kTgtTimeFiveHrZones = 1u << 12,
    kTgtIndoorBikeSimulation = 1u << 13,
    kTgtWheelCircumference = 1u << 14,
    kTgtSpinDown = 1u << 15,
    kTgtCadence = 1u << 16,
};

struct FeatureSet {
    bool valid = false;
    uint32_t machine = 0;
    uint32_t target = 0;

    bool hasMachine(MachineFeature f) const { return (machine & f) != 0; }
    bool hasTarget(TargetFeature f) const { return (target & f) != 0; }
};

/** 0x2AD6 / 0x2AD8 — beides drei sint16 bzw. uint16 nach Spec. */
struct ResistanceRange {
    bool valid = false;
    int16_t minRaw = 0;
    int16_t maxRaw = 0;
    uint16_t stepRaw = 0;
    /** Anzahl anfahrbarer Stufen, inklusive Minimum und Maximum. */
    uint16_t levelCount() const {
        if (!valid || stepRaw == 0 || maxRaw < minRaw) return 0;
        return (uint16_t)((maxRaw - minRaw) / stepRaw + 1);
    }
};

struct PowerRange {
    bool valid = false;
    int16_t minW = 0;
    int16_t maxW = 0;
    uint16_t stepW = 0;
};

// ------------------------------------------------------- Control Point

enum class Opcode : uint8_t {
    RequestControl = 0x00,
    Reset = 0x01,
    SetTargetSpeed = 0x02,
    SetTargetInclination = 0x03,
    SetTargetResistance = 0x04,
    SetTargetPower = 0x05,
    SetTargetHeartRate = 0x06,
    StartResume = 0x07,
    StopPause = 0x08,
    SetIndoorBikeSimulation = 0x11,
    ResponseCode = 0x80,
};

enum class ControlResult : uint8_t {
    Success = 0x01,
    NotSupported = 0x02,
    InvalidParameter = 0x03,
    OperationFailed = 0x04,
    ControlNotPermitted = 0x05,
};

/** Parameter von 0x08: 0x01 Stop, 0x02 Pause. */
enum class StopParam : uint8_t { Stop = 0x01, Pause = 0x02 };

struct ControlResponse {
    bool valid = false;
    Opcode request = Opcode::RequestControl;
    ControlResult result = ControlResult::OperationFailed;
    bool ok() const { return valid && result == ControlResult::Success; }
};

const char* opcodeName(Opcode op);
const char* controlResultName(ControlResult r);
const char* ibdStatusName(IbdStatus s);
const char* fieldName(Field f);

}  // namespace ftms
