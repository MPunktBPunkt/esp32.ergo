#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Rohbyte-Ring im JSONL-Format der Sonde (Abnahmekriterium 6a).
 *
 * Zweck: eine Fahrer-Session ist knapp und kommt nicht wieder. Was nicht
 * mitgeschrieben wurde, ist weg — und live mitlesen kann niemand, der
 * gleichzeitig tritt. Der Ring schreibt deshalb die Rohbytes weg, und zwar in
 * genau dem Format, das `tools/make-fixtures.py` als `bike-data.jsonl` liest:
 *
 *     {"t":123456,"uuid":"2AD2","dir":"notify","hex":"540B..."}
 *
 * Nur `uuid`, `dir` und `hex` werden vom Erzeuger ausgewertet, `t` ist Zugabe
 * fuer die Zeitreihe. Ein Export aus diesem Ring laesst sich damit unveraendert
 * in Fixtures verwandeln — aus einer echten Fahrt statt aus einem Laborlauf.
 *
 * Rohbytes und nicht dekodierte Werte, weil eine Umrechnung, die man nicht
 * rueckgaengig machen kann, genau dort die Beweiskraft kostet.
 *
 * Arduino-frei, Zeit kommt herein.
 */
namespace ergo {

/** 256 Datensaetze a 32 Byte = 8 kB. Bei 2 Hz und `ibdEvery = 5` reicht das
 *  fuer gut zehn Minuten Fahrt, inklusive des vollstaendigen Steuerverkehrs. */
constexpr uint16_t kRingSlots = 256;
/** Das laengste beobachtete 0x2AD2 des Varon hat 19 Byte. */
constexpr uint8_t kRingPayload = 24;

class DebugRing {
public:
    enum class Dir : uint8_t { Notify = 0, Write };

    struct Rec {
        uint32_t atMs = 0;
        uint16_t uuid = 0;
        uint8_t len = 0;
        Dir dir = Dir::Notify;
        uint8_t data[kRingPayload] = {0};
    };

    /** Aus bleibt der Standard. Mitschreiben kostet RAM und soll eine
     *  Entscheidung sein, keine Nebenwirkung. */
    void setEnabled(bool on);
    bool enabled() const { return enabled_; }

    /**
     * Nur jedes N-te 0x2AD2 behalten. `0` und `1` heissen: alle.
     *
     * Ausgenommen von der Ausduennung sind Pakete mit **neuem Flagwort** und
     * der gesamte Steuerverkehr. Das ist der Unterschied zwischen den beiden
     * Zwecken des Rings: fuer Fixtures zaehlt jedes neue Paketlayout, fuer die
     * Zeitreihe zaehlt Reichweite. Der Varon sendet ueber einen ganzen Lauf
     * ausschliesslich `0x0B54` — bei ihm traegt die Ausduennung, bei einem
     * gespraechigeren Geraet der Flagwechsel.
     */
    void setIbdEvery(uint16_t n);
    uint16_t ibdEvery() const { return ibdEvery_; }

    void add(uint16_t uuid, Dir dir, const uint8_t* data, size_t len, uint32_t nowMs);
    void clear();

    uint16_t count() const { return count_; }
    /** Wie viele Pakete angeboten wurden, auch die verworfenen. */
    uint32_t seen() const { return seen_; }
    /** Durch Ausduennung uebersprungen. */
    uint32_t thinned() const { return thinned_; }
    /** Vom Ring ueberschrieben, weil er voll war. */
    uint32_t overwritten() const { return overwritten_; }
    /** Payload war laenger als `kRingPayload` und wurde beschnitten. */
    uint32_t truncated() const { return truncated_; }

    /** Datensatz `i` in Aufzeichnungsreihenfolge, 0 ist der aelteste. */
    const Rec* at(uint16_t i) const;

    /** Eine JSONL-Zeile ohne Zeilenumbruch. Gibt die Laenge zurueck, 0 wenn
     *  der Puffer nicht reicht. */
    static size_t formatLine(const Rec& r, char* out, size_t cap);

private:
    Rec slots_[kRingSlots];
    uint16_t head_ = 0;  // naechster Schreibplatz
    uint16_t count_ = 0;
    uint32_t seen_ = 0;
    uint32_t thinned_ = 0;
    uint32_t overwritten_ = 0;
    uint32_t truncated_ = 0;
    uint16_t ibdEvery_ = 5;
    uint32_t ibdSeen_ = 0;
    uint16_t lastFlags_ = 0;
    bool haveFlags_ = false;
    bool enabled_ = false;
};

}  // namespace ergo
