#include "CyclingCodec.h"

namespace cycling {
namespace {

inline void put16le(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

inline void put32le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

}  // namespace

size_t encodeCyclingPower(int16_t watt, uint8_t* out, size_t cap) {
    if (!out || cap < 4) return 0;
    put16le(out, 0);  // flags: nur Instantaneous Power
    put16le(out + 2, (uint16_t)watt);
    return 4;
}

size_t encodeCyclingPowerFeature(uint32_t features, uint8_t* out, size_t cap) {
    if (!out || cap < 4) return 0;
    put32le(out, features);
    return 4;
}

size_t encodeCscCrank(uint16_t cumCrankRevs, uint16_t lastEvent1024, uint8_t* out, size_t cap) {
    if (!out || cap < 5) return 0;
    out[0] = 0x02;  // Crank Revolution Data Present
    put16le(out + 1, cumCrankRevs);
    put16le(out + 3, lastEvent1024);
    return 5;
}

size_t encodeCscFeature(uint16_t features, uint8_t* out, size_t cap) {
    if (!out || cap < 2) return 0;
    put16le(out, features);
    return 2;
}

}  // namespace cycling
