// Konstruierte 0x2AD2-Pakete fuer die Feldkombinationen, die der Varon XTR II
// nie sendet. Von Hand gerechnet, nicht generiert — diese Datei wird von
// tools/make-fixtures.py NICHT ueberschrieben.
//
// Ohne sie testet die Suite genau einen Pfad (flags 0x0B54), weil das Geraet
// im Laborlauf keine anderen Flag-Layouts schickt (siehe fixtures_ibd.h Kopf).
#pragma once

#include "fixtures_ibd.h"  // struct IbdFixture

static const IbdFixture kSynthFixtures[] = {
    // Nur Flags. Bit 0 GESETZT heisst "keine Instantaneous Speed" — das Paket
    // ist damit vollstaendig und leer zugleich.
    {"leer, Bit 0 gesetzt", "0100",
     {0x01, 0x00},
     2, 0x0001, 0x0000, 2, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},

    // Gegenprobe: Bit 0 GELOESCHT und sonst nichts — Speed ist da.
    {"nur Speed, Bit 0 geloescht", "0000E803",
     {0x00, 0x00, 0xE8, 0x03},
     4, 0x0000, 0x0001, 4, 0,
     1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},

    // Resistance Level (Bit 5) — das Feld, das dieses Geraet weglaesst, das
    // die Bridge in v0.3 aber selbst senden wird.
    {"Speed + Resistance + Power", "6000B80B7800C800",
     {0x60, 0x00, 0xB8, 0x0B, 0x78, 0x00, 0xC8, 0x00},
     8, 0x0060, 0x0061, 8, 0,
     3000, 0, 0, 0, 0, /* resistance */ 120, /* power */ 200, 0, 0, 0, 0, 0, 0, 0, 0},

    // Negative Leistung: Power ist sint16, nicht uint16.
    {"negative Power, kein Speed", "4100CEFF",
     {0x41, 0x00, 0xCE, 0xFF},
     4, 0x0041, 0x0040, 4, 0,
     0, 0, 0, 0, 0, 0, /* power */ -50, 0, 0, 0, 0, 0, 0, 0, 0},

    // Alle 13 Felder, Maximallaenge 30 Byte (= ftms::kMaxIndoorBikeLen).
    {"alle Felder, 30 Byte",
     "FE1FD204B004B400AA0040E201FBFFFA00F000410120030D9B58100E0807",
     {0xFE, 0x1F, 0xD2, 0x04, 0xB0, 0x04, 0xB4, 0x00, 0xAA, 0x00, 0x40, 0xE2, 0x01, 0xFB, 0xFF,
      0xFA, 0x00, 0xF0, 0x00, 0x41, 0x01, 0x20, 0x03, 0x0D, 0x9B, 0x58, 0x10, 0x0E, 0x08, 0x07},
     30, 0x1FFE, 0x1FFF, 30, 0,
     /* speed */ 1234, /* avgSpeed */ 1200, /* cadence */ 180, /* avgCadence */ 170,
     /* distance */ 123456, /* resistance */ -5, /* power */ 250, /* avgPower */ 240,
     /* energy */ 321, 800, 13, /* hr */ 155, /* met */ 88,
     /* elapsed */ 3600, /* remaining */ 1800},

    // Ueberzaehlige Bytes hinter den angekuendigten Feldern: gueltig, aber
    // meldepflichtig. Der Debug-Modus soll so etwas sichtbar machen.
    {"zwei Bytes Ueberhang", "0000E803FFFF",
     {0x00, 0x00, 0xE8, 0x03, 0xFF, 0xFF},
     6, 0x0000, 0x0001, 4, 2,
     1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static const size_t kSynthFixtureCount = sizeof(kSynthFixtures) / sizeof(kSynthFixtures[0]);
