#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Die Kennflaeche Stufe x Kadenz -> Leistung.
 *
 * Warum eine Flaeche und keine Tabelle: bei fester Stufe haengt die Leistung
 * an diesem Bike an der Kadenz. Die Bedienungsanleitung sagt es indirekt —
 * der Watt-Modus der Konsole regelt den Widerstand ueber die RPM, damit die
 * Leistung konstant bleibt. „Drehzahlunabhaengig" ist also eine Eigenschaft
 * des Konsolenprogramms, nicht der Stufen. Nachtest 2 belegt das mit Zahlen;
 * bis dahin ist die Flaeche die vorsichtigere Annahme, weil eine Tabelle ein
 * Sonderfall der Flaeche ist und nicht umgekehrt.
 *
 * Arduino-frei und ohne JSON, damit genau der Code nativ getestet wird, der
 * spaeter auch faehrt. Die Zeit kommt als Parameter herein.
 *
 * Zwei Quellen fuellen sie:
 *   - der gefuehrte Sweep (`SweepRunner`), unter kontrollierter Kadenz
 *   - passives Lernen im Fahrbetrieb, laufend und ungeplant
 *
 * Beide werden unterschiedlich gewichtet. Ein Sweep-Punkt entstand unter
 * gehaltener Kadenz und wird von passiven Punkten nur vorsichtig korrigiert;
 * andernfalls wuerde eine einzige unruhige Fahrt eine sauber gemessene
 * Stuetzstelle verwaschen.
 */
namespace ergo {

/** Kadenzbaender: 40 bis 120 rpm in Zehnerschritten. Darunter wird nicht
 *  gelernt — wer unter 40 rpm tritt, liefert keinen brauchbaren Punkt. */
constexpr float kCadMin = 40.0f;
constexpr float kCadStep = 10.0f;
constexpr uint8_t kCadBands = 8;
constexpr uint8_t kMapMaxLevels = 16;

struct MapCell {
    /** Geschaetzte Leistung in Watt. 0 mit `samples == 0` heisst „unbekannt". */
    float watt = 0.0f;
    uint16_t samples = 0;
    /** Zeitstempel des letzten Punktes, in Sekunden. Quelle bestimmt der
     *  Aufrufer — Unixzeit wenn NTP steht, sonst Laufzeit. */
    uint32_t lastS = 0;
    /** Stammt aus einem gefuehrten Sweep. */
    bool sweep = false;

    bool known() const { return samples > 0; }
};

class PowerMap {
public:
    /** Bereich aus den Geraetefaehigkeiten. Aendert sich der Bereich, wird die
     *  Flaeche verworfen — Stufe 8 von 16 ist nicht Stufe 8 von 24. */
    void begin(uint8_t levelCount, int16_t levelMinTenths, uint16_t levelStepTenths);
    void clear();
    bool ready() const { return levels_ > 0; }

    /** Nimmt einen Punkt auf. `false`, wenn er verworfen wurde — Kadenz
     *  ausserhalb der Baender, unbekannte Stufe oder unsinnige Leistung. */
    bool add(int16_t levelTenths, float rpm, float watt, uint32_t nowS, bool fromSweep);

    /** Schaetzt die Leistung fuer Stufe und Kadenz. Interpoliert ueber die
     *  Kadenz und, wenn die Stufenzeile leer ist, ueber die Nachbarstufen. */
    bool estimate(int16_t levelTenths, float rpm, float& wattOut) const;

    /**
     * Kleinste Stufe, deren Schaetzung das Ziel erreicht — die Grundoperation
     * von MANUAL_ERG. `ceiling` wird gesetzt, wenn selbst die hoechste
     * bekannte Stufe das Ziel nicht erreicht. Genau dieser Fall muss in der
     * UI sichtbar werden, statt still zu klemmen.
     */
    bool bestLevel(float targetWatt, float rpm, int16_t& tenthsOut, bool& ceiling) const;

    uint8_t levelCount() const { return levels_; }
    int16_t levelMinTenths() const { return minTenths_; }
    uint16_t levelStepTenths() const { return stepTenths_; }

    /**
     * Das Geraet hat mehr Stufen, als die Flaeche fuehrt.
     *
     * Bei 16 gespeicherten Stufen ist das fuer den Varon nie der Fall. Faellt
     * es bei einem anderen Geraet an, wird nur der untere Teil des Stellwegs
     * gelernt — und weil `reqLevels_` nicht mitserialisiert wird, verliert ein
     * solches Geraet die Flaeche bei jedem Neustart. Sichtbar statt still: die
     * Behebung ist `kMapMaxLevels` anzuheben, nicht dieses Flag zu ignorieren.
     */
    bool truncated() const { return truncated_; }

    /** Anteil belegter Zellen an allen Zellen, 0 bis 1. Bleibt naturgemaess
     *  klein — niemand faehrt alle acht Kadenzbaender. */
    float coverage() const;
    /** Stufen mit mindestens einer belegten Zelle. Das ist das Mass, auf das
     *  sich Abnahmekriterium 6 bezieht. */
    uint8_t levelsCovered() const;
    uint8_t bandsCovered() const;
    uint16_t pointCount() const;
    uint16_t sweepCells() const;

    const MapCell& cell(uint8_t levelIdx, uint8_t bandIdx) const;

    static int8_t bandOf(float rpm);
    static float bandCenter(uint8_t bandIdx) { return kCadMin + kCadStep * (bandIdx + 0.5f); }
    int8_t indexOf(int16_t levelTenths) const;
    int16_t tenthsOf(uint8_t levelIdx) const;

    // ── Serialisierung fuer NVS ─────────────────────────────────────────────
    //
    // Bewusst byteweise und little-endian statt `memcpy` der Struktur: die
    // Ausrichtung von `MapCell` ist implementierungsabhaengig, und eine Flaeche,
    // die ein Compilerwechsel unlesbar macht, waere ein stiller Datenverlust.
    static constexpr size_t kHeaderSize = 8;
    static constexpr size_t kCellSize = 9;
    static constexpr size_t kMaxBytes = kHeaderSize + kMapMaxLevels * kCadBands * kCellSize;

    size_t byteSize() const { return kHeaderSize + (size_t)levels_ * kCadBands * kCellSize; }
    /** Schreibt die Flaeche. Gibt die Anzahl Bytes zurueck, 0 bei zu kleinem Puffer. */
    size_t save(uint8_t* buf, size_t cap) const;
    /** Liest die Flaeche. `false` bei falschem Magic, falscher Version oder
     *  unpassender Laenge — dann bleibt die Flaeche unveraendert leer. */
    bool load(const uint8_t* buf, size_t len);

private:
    /** Schaetzung innerhalb einer Stufenzeile, ueber die Kadenz interpoliert. */
    bool rowEstimate(uint8_t levelIdx, float rpm, float& out) const;
    MapCell& at(uint8_t levelIdx, uint8_t bandIdx) { return cells_[levelIdx][bandIdx]; }

    MapCell cells_[kMapMaxLevels][kCadBands];
    uint8_t levels_ = 0;
    /** Vom Geraet gemeldete Stufenzahl, ungeklemmt — nur damit ein Wechsel
     *  des Bereichs erkannt wird, auch wenn beide Werte auf 16 klemmen. */
    uint8_t reqLevels_ = 0;
    bool truncated_ = false;
    int16_t minTenths_ = 0;
    uint16_t stepTenths_ = 0;
};

}  // namespace ergo
