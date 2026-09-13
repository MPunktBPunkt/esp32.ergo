#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Minimale CPS/CSC-Encoder fuer die Bridge — Arduino-frei, hosttestbar.
 * Nur die Felder, die Indoor-Apps typischerweise brauchen.
 */
namespace cycling {

constexpr uint16_t kSvcCyclingPower = 0x1818;
constexpr uint16_t kChrCyclingPowerMeasurement = 0x2A63;
constexpr uint16_t kChrCyclingPowerFeature = 0x2A65;

constexpr uint16_t kSvcCsc = 0x1816;
constexpr uint16_t kChrCscMeasurement = 0x2A5B;
constexpr uint16_t kChrCscFeature = 0x2A5C;

/** Feature: Crank Revolution Data Supported. */
constexpr uint32_t kCpsFeatCrank = 1u << 3;
/** CSC Feature bit1: Crank Revolution Data Supported. */
constexpr uint16_t kCscFeatCrank = 1u << 1;

/** CPS Measurement: Flags=0 + Instantaneous Power (Watt). */
size_t encodeCyclingPower(int16_t watt, uint8_t* out, size_t cap);

/** CPS Feature uint32 LE. */
size_t encodeCyclingPowerFeature(uint32_t features, uint8_t* out, size_t cap);

/**
 * CSC Measurement mit Crank: Flags bit1, cumCrank (uint16), lastEvent (uint16,
 * 1/1024 s).
 */
size_t encodeCscCrank(uint16_t cumCrankRevs, uint16_t lastEvent1024, uint8_t* out, size_t cap);

/** CSC Feature uint16 LE. */
size_t encodeCscFeature(uint16_t features, uint8_t* out, size_t cap);

}  // namespace cycling
