#pragma once

#include "FtmsTypes.h"

/**
 * Reiner FTMS-Codec: Bytes rein, Struct raus und umgekehrt. Kein Arduino,
 * kein NimBLE, kein Zustand — damit er nativ gegen die aufgezeichneten Pakete
 * aus dem Sondenlauf getestet werden kann (test/test_codec).
 *
 * Alles, was mit einer Verbindung zu tun hat, gehoert in FtmsClient.
 */
namespace ftms {

// ------------------------------------------------------------- Dekodieren

/** 0x2AD2 Indoor Bike Data. `out` wird in jedem Fall zurueckgesetzt. */
IbdStatus decodeIndoorBikeData(const uint8_t* data, size_t len, IndoorBikeData& out);

/** 0x2ACC Fitness Machine Feature, 8 Byte. */
bool decodeFeature(const uint8_t* data, size_t len, FeatureSet& out);

/** 0x2AD6 Supported Resistance Level Range, 6 Byte. */
bool decodeResistanceRange(const uint8_t* data, size_t len, ResistanceRange& out);

/** 0x2AD8 Supported Power Range, 6 Byte. Dieses Geraet liefert sie nicht. */
bool decodePowerRange(const uint8_t* data, size_t len, PowerRange& out);

/** 0x2AD9 Antwort `80 <opcode> <result>`. */
bool decodeControlResponse(const uint8_t* data, size_t len, ControlResponse& out);

// -------------------------------------------------------------- Kodieren
//
// Alle Encoder geben die Anzahl geschriebener Bytes zurueck, 0 bei zu
// kleinem Puffer. `kMaxControlLen` deckt das laengste Kommando ab (0x11).

constexpr size_t kMaxControlLen = 8;

size_t encodeRequestControl(uint8_t* out, size_t cap);
size_t encodeReset(uint8_t* out, size_t cap);
size_t encodeStartResume(uint8_t* out, size_t cap);
size_t encodeStop(uint8_t* out, size_t cap);
size_t encodePause(uint8_t* out, size_t cap);

/**
 * 0x04 Set Target Resistance Level.
 *
 * Die Spec kennt fuer dieses Kommando zwei Auslegungen — uint8 und sint16,
 * beide in 0,1er-Schritten. Der Varon XTR II quittiert **beide** mit Success,
 * aber nur die 2-Byte-Form hat im Sondenlauf tatsaechlich gewirkt (`04 0A`
 * blieb ohne Laststufenwechsel, `04 64 00` nicht). Deshalb ist `wide` der
 * Standard.
 *
 * @param tenths Stufe in Zehnteln, also 120 fuer Stufe 12,0.
 */
size_t encodeSetTargetResistance(uint8_t* out, size_t cap, int16_t tenths, bool wide = true);

/**
 * 0x05 Set Target Power.
 *
 * Vorsicht: das Geraet meldet das Feature NICHT (0x2ACC Target-Bit 3
 * geloescht) und liefert auch keine 0x2AD8 — antwortet auf `05 64 00`
 * aber mit `80 05 01` Success. Eine Erfolgsquittung beweist hier also
 * nichts. Ob das Kommando wirkt, klaert Test 3 in
 * docs/ergometer/NACHTESTS.md; bis dahin steuert nichts in dieser Firmware
 * ueber diesen Weg.
 */
size_t encodeSetTargetPower(uint8_t* out, size_t cap, int16_t watt);

/**
 * 0x06 Set Target Heart Rate. Target-Bit 4 ist ebenfalls geloescht.
 * Pulsfuehrung laeuft deshalb als Kaskade ueber den Widerstand.
 */
size_t encodeSetTargetHeartRate(uint8_t* out, size_t cap, uint8_t bpm);

/**
 * 0x11 Indoor Bike Simulation Parameters. Target-Bit 13 ist gesetzt, aber
 * ungetestet — siehe Test 4 in docs/ergometer/NACHTESTS.md.
 *
 * @param windMms       Windgeschwindigkeit in mm/s (sint16)
 * @param gradeHundredth Steigung in 0,01 % (sint16), also 250 fuer 2,5 %
 * @param crr10000      Rollwiderstand in 0,0001 (uint8)
 * @param cw100         Luftwiderstand in 0,01 kg/m (uint8)
 */
size_t encodeIndoorBikeSimulation(uint8_t* out, size_t cap, int16_t windMms,
                                  int16_t gradeHundredth, uint8_t crr10000, uint8_t cw100);

}  // namespace ftms
