#include "FtmsCodec.h"

namespace ftms {
namespace {

inline uint16_t u16le(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

inline int16_t s16le(const uint8_t* p) {
    return (int16_t)u16le(p);
}

inline uint32_t u24le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

inline uint32_t u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

inline void put16le(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

}  // namespace

// --------------------------------------------------------------- 0x2AD2

IbdStatus decodeIndoorBikeData(const uint8_t* data, size_t len, IndoorBikeData& out) {
    out = IndoorBikeData{};
    if (!data || len < 2) return IbdStatus::TooShort;

    const uint16_t flags = u16le(data);
    out.flags = flags;
    size_t off = 2;

    // Die eine Falle: Bit 0 gesetzt heisst "kein Instantaneous Speed".
    const bool speedPresent = (flags & 0x0001) == 0;

    auto fail = [&](Field f) -> IbdStatus {
        out.truncatedField = (uint16_t)f;
        out.consumed = (uint8_t)off;
        return IbdStatus::FieldTruncated;
    };
    auto room = [&](size_t n) { return off + n <= len; };

    if (speedPresent) {
        if (!room(2)) return fail(kSpeed);
        out.speedRaw = u16le(data + off);
        off += 2;
        out.presence |= kSpeed;
    }
    if (flags & kAvgSpeed) {
        if (!room(2)) return fail(kAvgSpeed);
        out.avgSpeedRaw = u16le(data + off);
        off += 2;
        out.presence |= kAvgSpeed;
    }
    if (flags & kCadence) {
        if (!room(2)) return fail(kCadence);
        out.cadenceRaw = u16le(data + off);
        off += 2;
        out.presence |= kCadence;
    }
    if (flags & kAvgCadence) {
        if (!room(2)) return fail(kAvgCadence);
        out.avgCadenceRaw = u16le(data + off);
        off += 2;
        out.presence |= kAvgCadence;
    }
    if (flags & kDistance) {
        if (!room(3)) return fail(kDistance);
        out.distanceM = u24le(data + off);
        off += 3;
        out.presence |= kDistance;
    }
    if (flags & kResistance) {
        if (!room(2)) return fail(kResistance);
        out.resistanceRaw = s16le(data + off);
        off += 2;
        out.presence |= kResistance;
    }
    if (flags & kPower) {
        if (!room(2)) return fail(kPower);
        out.powerW = s16le(data + off);
        off += 2;
        out.presence |= kPower;
    }
    if (flags & kAvgPower) {
        if (!room(2)) return fail(kAvgPower);
        out.avgPowerW = s16le(data + off);
        off += 2;
        out.presence |= kAvgPower;
    }
    if (flags & kEnergy) {
        if (!room(5)) return fail(kEnergy);
        out.energyTotalKcal = u16le(data + off);
        out.energyPerHourKcal = u16le(data + off + 2);
        out.energyPerMinKcal = data[off + 4];
        off += 5;
        out.presence |= kEnergy;
    }
    if (flags & kHeartRate) {
        if (!room(1)) return fail(kHeartRate);
        out.heartRateBpm = data[off];
        off += 1;
        out.presence |= kHeartRate;
    }
    if (flags & kMet) {
        if (!room(1)) return fail(kMet);
        out.metRaw = data[off];
        off += 1;
        out.presence |= kMet;
    }
    if (flags & kElapsedTime) {
        if (!room(2)) return fail(kElapsedTime);
        out.elapsedS = u16le(data + off);
        off += 2;
        out.presence |= kElapsedTime;
    }
    if (flags & kRemainingTime) {
        if (!room(2)) return fail(kRemainingTime);
        out.remainingS = u16le(data + off);
        off += 2;
        out.presence |= kRemainingTime;
    }

    out.consumed = (uint8_t)off;
    out.trailing = (uint8_t)(len - off);
    return IbdStatus::Ok;
}

// --------------------------------------------------------------- 0x2ACC

bool decodeFeature(const uint8_t* data, size_t len, FeatureSet& out) {
    out = FeatureSet{};
    if (!data || len < 8) return false;
    out.machine = u32le(data);
    out.target = u32le(data + 4);
    out.valid = true;
    return true;
}

// ------------------------------------------------------- 0x2AD6 / 0x2AD8

bool decodeResistanceRange(const uint8_t* data, size_t len, ResistanceRange& out) {
    out = ResistanceRange{};
    if (!data || len < 6) return false;
    out.minRaw = s16le(data);
    out.maxRaw = s16le(data + 2);
    out.stepRaw = u16le(data + 4);
    out.valid = true;
    return true;
}

bool decodePowerRange(const uint8_t* data, size_t len, PowerRange& out) {
    out = PowerRange{};
    if (!data || len < 6) return false;
    out.minW = s16le(data);
    out.maxW = s16le(data + 2);
    out.stepW = u16le(data + 4);
    out.valid = true;
    return true;
}

// --------------------------------------------------------------- 0x2AD9

bool decodeControlResponse(const uint8_t* data, size_t len, ControlResponse& out) {
    out = ControlResponse{};
    if (!data || len < 3) return false;
    if (data[0] != (uint8_t)Opcode::ResponseCode) return false;
    out.request = (Opcode)data[1];
    out.result = (ControlResult)data[2];
    out.valid = true;
    return true;
}

// -------------------------------------------------------------- Kodieren

namespace {

size_t encodeBare(uint8_t* out, size_t cap, Opcode op) {
    if (!out || cap < 1) return 0;
    out[0] = (uint8_t)op;
    return 1;
}

size_t encodeStopPause(uint8_t* out, size_t cap, StopParam p) {
    if (!out || cap < 2) return 0;
    out[0] = (uint8_t)Opcode::StopPause;
    out[1] = (uint8_t)p;
    return 2;
}

}  // namespace

size_t encodeRequestControl(uint8_t* out, size_t cap) {
    return encodeBare(out, cap, Opcode::RequestControl);
}

size_t encodeReset(uint8_t* out, size_t cap) {
    return encodeBare(out, cap, Opcode::Reset);
}

size_t encodeStartResume(uint8_t* out, size_t cap) {
    return encodeBare(out, cap, Opcode::StartResume);
}

size_t encodeStop(uint8_t* out, size_t cap) {
    return encodeStopPause(out, cap, StopParam::Stop);
}

size_t encodePause(uint8_t* out, size_t cap) {
    return encodeStopPause(out, cap, StopParam::Pause);
}

size_t encodeSetTargetResistance(uint8_t* out, size_t cap, int16_t tenths, bool wide) {
    if (!out) return 0;
    if (wide) {
        if (cap < 3) return 0;
        out[0] = (uint8_t)Opcode::SetTargetResistance;
        put16le(out + 1, (uint16_t)tenths);
        return 3;
    }
    if (cap < 2) return 0;
    if (tenths < 0 || tenths > 255) return 0;
    out[0] = (uint8_t)Opcode::SetTargetResistance;
    out[1] = (uint8_t)tenths;
    return 2;
}

size_t encodeSetTargetPower(uint8_t* out, size_t cap, int16_t watt) {
    if (!out || cap < 3) return 0;
    out[0] = (uint8_t)Opcode::SetTargetPower;
    put16le(out + 1, (uint16_t)watt);
    return 3;
}

size_t encodeSetTargetHeartRate(uint8_t* out, size_t cap, uint8_t bpm) {
    if (!out || cap < 2) return 0;
    out[0] = (uint8_t)Opcode::SetTargetHeartRate;
    out[1] = bpm;
    return 2;
}

size_t encodeIndoorBikeSimulation(uint8_t* out, size_t cap, int16_t windMms,
                                  int16_t gradeHundredth, uint8_t crr10000, uint8_t cw100) {
    if (!out || cap < 7) return 0;
    out[0] = (uint8_t)Opcode::SetIndoorBikeSimulation;
    put16le(out + 1, (uint16_t)windMms);
    put16le(out + 3, (uint16_t)gradeHundredth);
    out[5] = crr10000;
    out[6] = cw100;
    return 7;
}

size_t encodeIndoorBikeData(const IndoorBikeData& in, uint8_t* out, size_t cap) {
    if (!out || cap < 2) return 0;
    uint16_t flags = 0;
    // Bit 0 invertiert: gesetzt = kein Instantaneous Speed.
    if (!in.has(kSpeed)) flags |= 0x0001;
    if (in.has(kAvgSpeed)) flags |= kAvgSpeed;
    if (in.has(kCadence)) flags |= kCadence;
    if (in.has(kAvgCadence)) flags |= kAvgCadence;
    if (in.has(kDistance)) flags |= kDistance;
    if (in.has(kResistance)) flags |= kResistance;
    if (in.has(kPower)) flags |= kPower;
    if (in.has(kAvgPower)) flags |= kAvgPower;
    if (in.has(kEnergy)) flags |= kEnergy;
    if (in.has(kHeartRate)) flags |= kHeartRate;
    if (in.has(kMet)) flags |= kMet;
    if (in.has(kElapsedTime)) flags |= kElapsedTime;
    if (in.has(kRemainingTime)) flags |= kRemainingTime;

    size_t need = 2;
    if (in.has(kSpeed)) need += 2;
    if (in.has(kAvgSpeed)) need += 2;
    if (in.has(kCadence)) need += 2;
    if (in.has(kAvgCadence)) need += 2;
    if (in.has(kDistance)) need += 3;
    if (in.has(kResistance)) need += 2;
    if (in.has(kPower)) need += 2;
    if (in.has(kAvgPower)) need += 2;
    if (in.has(kEnergy)) need += 5;
    if (in.has(kHeartRate)) need += 1;
    if (in.has(kMet)) need += 1;
    if (in.has(kElapsedTime)) need += 2;
    if (in.has(kRemainingTime)) need += 2;
    if (need > cap || need > kMaxIndoorBikeLen) return 0;

    size_t off = 0;
    put16le(out + off, flags);
    off += 2;
    if (in.has(kSpeed)) {
        put16le(out + off, in.speedRaw);
        off += 2;
    }
    if (in.has(kAvgSpeed)) {
        put16le(out + off, in.avgSpeedRaw);
        off += 2;
    }
    if (in.has(kCadence)) {
        put16le(out + off, in.cadenceRaw);
        off += 2;
    }
    if (in.has(kAvgCadence)) {
        put16le(out + off, in.avgCadenceRaw);
        off += 2;
    }
    if (in.has(kDistance)) {
        out[off] = (uint8_t)(in.distanceM & 0xFF);
        out[off + 1] = (uint8_t)((in.distanceM >> 8) & 0xFF);
        out[off + 2] = (uint8_t)((in.distanceM >> 16) & 0xFF);
        off += 3;
    }
    if (in.has(kResistance)) {
        put16le(out + off, (uint16_t)in.resistanceRaw);
        off += 2;
    }
    if (in.has(kPower)) {
        put16le(out + off, (uint16_t)in.powerW);
        off += 2;
    }
    if (in.has(kAvgPower)) {
        put16le(out + off, (uint16_t)in.avgPowerW);
        off += 2;
    }
    if (in.has(kEnergy)) {
        put16le(out + off, in.energyTotalKcal);
        put16le(out + off + 2, in.energyPerHourKcal);
        out[off + 4] = in.energyPerMinKcal;
        off += 5;
    }
    if (in.has(kHeartRate)) {
        out[off++] = in.heartRateBpm;
    }
    if (in.has(kMet)) {
        out[off++] = in.metRaw;
    }
    if (in.has(kElapsedTime)) {
        put16le(out + off, in.elapsedS);
        off += 2;
    }
    if (in.has(kRemainingTime)) {
        put16le(out + off, in.remainingS);
        off += 2;
    }
    return off;
}

size_t encodeFeature(const FeatureSet& in, uint8_t* out, size_t cap) {
    if (!out || cap < 8) return 0;
    out[0] = (uint8_t)(in.machine & 0xFF);
    out[1] = (uint8_t)((in.machine >> 8) & 0xFF);
    out[2] = (uint8_t)((in.machine >> 16) & 0xFF);
    out[3] = (uint8_t)((in.machine >> 24) & 0xFF);
    out[4] = (uint8_t)(in.target & 0xFF);
    out[5] = (uint8_t)((in.target >> 8) & 0xFF);
    out[6] = (uint8_t)((in.target >> 16) & 0xFF);
    out[7] = (uint8_t)((in.target >> 24) & 0xFF);
    return 8;
}

size_t encodeResistanceRange(const ResistanceRange& in, uint8_t* out, size_t cap) {
    if (!out || cap < 6) return 0;
    put16le(out + 0, (uint16_t)in.minRaw);
    put16le(out + 2, (uint16_t)in.maxRaw);
    put16le(out + 4, in.stepRaw);
    return 6;
}

size_t encodePowerRange(const PowerRange& in, uint8_t* out, size_t cap) {
    if (!out || cap < 6) return 0;
    put16le(out + 0, (uint16_t)in.minW);
    put16le(out + 2, (uint16_t)in.maxW);
    put16le(out + 4, in.stepW);
    return 6;
}

size_t encodeControlResponse(Opcode request, ControlResult result, uint8_t* out, size_t cap) {
    if (!out || cap < 3) return 0;
    out[0] = (uint8_t)Opcode::ResponseCode;
    out[1] = (uint8_t)request;
    out[2] = (uint8_t)result;
    return 3;
}

bool decodeControlWrite(const uint8_t* data, size_t len, ControlWrite& out) {
    out = ControlWrite{};
    if (!data || len < 1) return false;
    out.op = (Opcode)data[0];
    out.valid = true;
    switch (out.op) {
        case Opcode::RequestControl:
        case Opcode::Reset:
        case Opcode::StartResume:
            return true;
        case Opcode::StopPause:
            if (len < 2) {
                out.valid = false;
                return false;
            }
            out.stopParam = data[1];
            return true;
        case Opcode::SetTargetResistance:
            if (len >= 3) {
                out.resistanceTenths = s16le(data + 1);
                return true;
            }
            if (len >= 2) {
                out.resistanceTenths = data[1];
                return true;
            }
            out.valid = false;
            return false;
        case Opcode::SetTargetPower:
            if (len < 3) {
                out.valid = false;
                return false;
            }
            out.watt = s16le(data + 1);
            return true;
        case Opcode::SetIndoorBikeSimulation:
            if (len < 7) {
                out.valid = false;
                return false;
            }
            out.windMms = s16le(data + 1);
            out.gradeHundredth = s16le(data + 3);
            out.crr10000 = data[5];
            out.cw100 = data[6];
            return true;
        default:
            // Unbekannte Ops bleiben valid mit Opcode — Server antwortet NotSupported.
            return true;
    }
}

FeatureSet bridgeFeatureSet() {
    FeatureSet f;
    f.valid = true;
    f.machine = kFeatCadence | kFeatTotalDistance | kFeatResistanceLevel | kFeatExpendedEnergy |
                kFeatHeartRate | kFeatElapsedTime | kFeatPowerMeasurement;
    f.target = kTgtResistance | kTgtPower;
    return f;
}

ResistanceRange bridgeResistanceRange() {
    ResistanceRange r;
    r.valid = true;
    r.minRaw = 10;
    r.maxRaw = 160;
    r.stepRaw = 10;
    return r;
}

PowerRange bridgePowerRange() {
    PowerRange r;
    r.valid = true;
    r.minW = 20;
    r.maxW = 400;
    r.stepW = 1;
    return r;
}

// ----------------------------------------------------------------- Namen

const char* opcodeName(Opcode op) {
    switch (op) {
        case Opcode::RequestControl: return "RequestControl";
        case Opcode::Reset: return "Reset";
        case Opcode::SetTargetSpeed: return "SetTargetSpeed";
        case Opcode::SetTargetInclination: return "SetTargetInclination";
        case Opcode::SetTargetResistance: return "SetTargetResistance";
        case Opcode::SetTargetPower: return "SetTargetPower";
        case Opcode::SetTargetHeartRate: return "SetTargetHeartRate";
        case Opcode::StartResume: return "StartResume";
        case Opcode::StopPause: return "StopPause";
        case Opcode::SetIndoorBikeSimulation: return "SetIndoorBikeSimulation";
        case Opcode::ResponseCode: return "ResponseCode";
        default: return "Unknown";
    }
}

const char* controlResultName(ControlResult r) {
    switch (r) {
        case ControlResult::Success: return "Success";
        case ControlResult::NotSupported: return "NotSupported";
        case ControlResult::InvalidParameter: return "InvalidParameter";
        case ControlResult::OperationFailed: return "OperationFailed";
        case ControlResult::ControlNotPermitted: return "ControlNotPermitted";
        default: return "Unknown";
    }
}

const char* ibdStatusName(IbdStatus s) {
    switch (s) {
        case IbdStatus::Ok: return "OK";
        case IbdStatus::TooShort: return "TOO_SHORT";
        case IbdStatus::FieldTruncated: return "FIELD_TRUNCATED";
        default: return "UNKNOWN";
    }
}

const char* fieldName(Field f) {
    switch (f) {
        case kSpeed: return "speed";
        case kAvgSpeed: return "avg_speed";
        case kCadence: return "cadence";
        case kAvgCadence: return "avg_cadence";
        case kDistance: return "distance";
        case kResistance: return "resistance";
        case kPower: return "power";
        case kAvgPower: return "avg_power";
        case kEnergy: return "energy";
        case kHeartRate: return "heart_rate";
        case kMet: return "met";
        case kElapsedTime: return "elapsed_time";
        case kRemainingTime: return "remaining_time";
        default: return "unknown";
    }
}

}  // namespace ftms
