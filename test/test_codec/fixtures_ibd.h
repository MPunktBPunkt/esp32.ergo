// Echte 0x2AD2-Pakete aus dem Sondenlauf scan-20260910 (Hammer Varon XTR II).
//
// Dies ist der VON HAND gepruefte Kernsatz. `tools/make-fixtures.py` ersetzt
// diese Datei durch alle 383 eindeutigen Pakete des Laufs, sobald sie in einer
// Umgebung mit Python laeuft. Struktur und Feldnamen bleiben identisch.
//
// Bemerkenswert: das Geraet sendet ueber den gesamten Lauf ausschliesslich
// flags = 0x0B54 mit 19 Byte. Die uebrigen Feldkombinationen des Standards
// deckt fixtures_synth.h ab — sonst testet man einen Varon-Decoder statt
// eines FTMS-Decoders.
#pragma once

#include <stddef.h>
#include <stdint.h>

struct IbdFixture {
    const char* note;
    const char* hex;
    uint8_t data[30];
    uint8_t len;
    uint16_t flags;
    uint16_t presence;
    uint8_t consumed;
    uint8_t trailing;
    uint16_t speedRaw;
    uint16_t avgSpeedRaw;
    uint16_t cadenceRaw;
    uint16_t avgCadenceRaw;
    uint32_t distanceM;
    int16_t resistanceRaw;
    int16_t powerW;
    int16_t avgPowerW;
    uint16_t energyTotalKcal;
    uint16_t energyPerHourKcal;
    uint8_t energyPerMinKcal;
    uint8_t heartRateBpm;
    uint8_t metRaw;
    uint16_t elapsedS;
    uint16_t remainingS;
};

// flags 0x0B54 = Speed, Cadence, Distance, Power, Energy, HeartRate, ElapsedTime.
// Kein Resistance-Level-Feld — das Geraet meldet die gestellte Stufe nicht zurueck.
static const IbdFixture kIbdFixtures[] = {
    {"Stillstand, Gurt aktiv", "540B000000006F090000000B0000000052A501",
     {0x54, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x6F, 0x09, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00,
      0x00, 0x52, 0xA5, 0x01},
     19, 0x0B54, 0x0B55, 19, 0,
     /* speed */ 0, /* avgSpeed */ 0, /* cadence */ 0, /* avgCadence */ 0,
     /* distance */ 2415, /* resistance */ 0, /* power */ 0, /* avgPower */ 0,
     /* energy */ 11, 0, 0, /* hr */ 82, /* met */ 0, /* elapsed */ 421, /* remaining */ 0},

    {"kein Gurt, HR-Feld 0", "540B74098200CA04001C00050000000000D900",
     {0x54, 0x0B, 0x74, 0x09, 0x82, 0x00, 0xCA, 0x04, 0x00, 0x1C, 0x00, 0x05, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xD9, 0x00},
     19, 0x0B54, 0x0B55, 19, 0,
     2420, 0, 130, 0, 1226, 0, 28, 0, 5, 0, 0, 0, 0, 217, 0},

    {"Grundlast, HR ueber GymLink", "540BC4098600FC05001D000700000000520C01",
     {0x54, 0x0B, 0xC4, 0x09, 0x86, 0x00, 0xFC, 0x05, 0x00, 0x1D, 0x00, 0x07, 0x00, 0x00, 0x00,
      0x00, 0x52, 0x0C, 0x01},
     19, 0x0B54, 0x0B55, 19, 0,
     2500, 0, 134, 0, 1532, 0, 29, 0, 7, 0, 0, 82, 0, 268, 0},

    {"Grundlast", "540BDE087A009207001A0009000000004D5401",
     {0x54, 0x0B, 0xDE, 0x08, 0x7A, 0x00, 0x92, 0x07, 0x00, 0x1A, 0x00, 0x09, 0x00, 0x00, 0x00,
      0x00, 0x4D, 0x54, 0x01},
     19, 0x0B54, 0x0B55, 19, 0,
     2270, 0, 122, 0, 1938, 0, 26, 0, 9, 0, 0, 77, 0, 340, 0},

    // Hoechste im gesamten Lauf gemessene Leistung. Stufe 10 (wide), 59 rpm,
    // 108 W — die Stuetzstelle, aus der die ~130-W-Extrapolation stammt.
    {"Stufe 10 wide, hoechste Messung", "540B98087600130A006C000C0000000051C301",
     {0x54, 0x0B, 0x98, 0x08, 0x76, 0x00, 0x13, 0x0A, 0x00, 0x6C, 0x00, 0x0C, 0x00, 0x00, 0x00,
      0x00, 0x51, 0xC3, 0x01},
     19, 0x0B54, 0x0B55, 19, 0,
     2200, 0, 118, 0, 2579, 0, 108, 0, 12, 0, 0, 81, 0, 451, 0},
};

static const size_t kIbdFixtureCount = sizeof(kIbdFixtures) / sizeof(kIbdFixtures[0]);
