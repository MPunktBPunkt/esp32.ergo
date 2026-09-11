#pragma once

#include <stdint.h>

#include "PowerMap.h"

/**
 * Der gefuehrte Stufen-Sweep aus docs/ergometer/NACHTESTS.md Test 1 und 2.
 *
 * Vorgehen je Stufe: stellen, 20 s einschwingen, 40 s mitteln. Ein Punkt zaehlt
 * nur, wenn die Kadenz im Fenster gehalten wurde — und genau das ist der Grund,
 * warum dieses Ding existiert. Zwei der drei Wirkungsmessungen des ersten
 * Sondenlaufs sind wertlos, weil die Kadenz weggelaufen ist und niemand es
 * gemerkt hat. Die Verwerfung von Hand zu machen heisst, sie zu vergessen.
 *
 * Wie der Limiter trifft diese Klasse nur Entscheidungen: `tick()` liefert
 * „stelle Stufe x" oder „schicke Stop", und der Aufrufer setzt das um — durch
 * den Limiter, wie jeder andere Schreibweg. Dadurch ist der gesamte Ablauf
 * nativ testbar, inklusive Zeitverhalten, weil die Zeit hereinkommt.
 *
 * Arduino-frei.
 */
namespace ergo {

struct SweepPlan {
    int16_t levels[kMapMaxLevels] = {0};
    uint8_t count = 0;
    /** Nur fuer Anzeige und Protokoll — geregelt wird die Kadenz nicht. */
    float targetRpm = 60.0f;
    uint32_t settleMs = 20000;
    uint32_t windowMs = 40000;
    /** Darunter ist ein Fenster ungueltig. */
    float minRpm = 45.0f;
    /** Zulaessige Spanne der Kadenz im Fenster, in Prozent des Mittels. */
    float maxDriftPct = 10.0f;
    /** So lange unter `minRpm` fuehrt zum Abbruch — der Fahrer hat aufgehoert. */
    uint32_t abortAfterMs = 15000;
};

enum class SweepState : uint8_t { Idle = 0, Settle, Measure, Done, Aborted };

const char* sweepStateName(SweepState s);

struct SweepPoint {
    int16_t levelTenths = 0;
    float meanRpm = 0.0f;
    float meanWatt = 0.0f;
    float rpmMin = 0.0f;
    float rpmMax = 0.0f;
    uint16_t samples = 0;
    bool valid = false;
    /** Bei `valid == false` der Grund. Wird in der UI ausgewiesen — ein
     *  verworfener Punkt, den niemand sieht, ist ein stiller Datenverlust. */
    const char* reason = "";
};

class SweepRunner {
public:
    struct Tick {
        enum class Do : uint8_t { Nothing = 0, SetLevel, Stop };
        Do action = Do::Nothing;
        int16_t levelTenths = 0;
    };

    /** Baut den Plan aus dem Geraetebereich. `coarse` nimmt die verkuerzte
     *  Liste aus Test 2 (Stufen 4, 8, 12, 16), sonst die volle aus Test 1
     *  (1, 2, 4, 6, 8, 10, 12, 14, 16). Stufen jenseits des Bereichs fallen
     *  weg, statt den Plan unbrauchbar zu machen. */
    static SweepPlan planFor(uint8_t levelCount, int16_t minTenths, uint16_t stepTenths,
                             float targetRpm, bool coarse);

    /**
     * Entfernt Stufen oberhalb `maxTenths` (0 = keine Grenze). Noetig, weil der
     * Limiter still klemmt: sonst misst der Sweep „Stufe 16", faehrt aber 8,
     * und die Kennflaeche wird vergiftet (gesehen mit Profil reha).
     */
    static void clipPlanToMax(SweepPlan& plan, int16_t maxTenths);

    bool start(const SweepPlan& plan, uint32_t nowMs);
    void cancel(uint32_t nowMs);
    void reset();

    /**
     * Einen Durchlauf weiterdrehen. `fresh` sagt, ob Kadenz und Leistung aus
     * einem aktuellen Datenpaket stammen; veraltete Werte werden nicht
     * gemittelt, sonst zaehlt ein abgerissener Notify-Strom als ruhige Fahrt.
     */
    Tick tick(uint32_t nowMs, float rpm, float watt, bool fresh);

    /** Nach einem tatsaechlich abgesetzten Stufen-Write aufrufen. Erst dann
     *  beginnt das Einschwingen — die Rampe des Limiters kann den Write um
     *  Sekunden verzoegern, und diese Sekunden gehoeren nicht ins Fenster. */
    void noteLevelSet(uint32_t nowMs);

    SweepState state() const { return state_; }
    bool running() const { return state_ == SweepState::Settle || state_ == SweepState::Measure; }
    const SweepPlan& plan() const { return plan_; }
    uint8_t index() const { return idx_; }
    int16_t currentLevelTenths() const;
    uint8_t pointCount() const { return points_; }
    const SweepPoint& point(uint8_t i) const;
    uint8_t validCount() const;
    const char* abortReason() const { return abortReason_; }

    /** 0 bis 100. Zaehlt Stufen, nicht Zeit — das ist die Zahl, die der
     *  Fahrer braucht („noch drei Stufen"). */
    uint8_t progressPct() const;
    uint32_t phaseRemainingMs(uint32_t nowMs) const;

private:
    void finishWindow(uint32_t nowMs);
    void advance(uint32_t nowMs);

    SweepPlan plan_;
    SweepState state_ = SweepState::Idle;
    uint8_t idx_ = 0;
    bool needSet_ = false;
    uint32_t lastSetTryMs_ = 0;
    uint32_t phaseStartMs_ = 0;
    uint32_t lowSinceMs_ = 0;
    bool low_ = false;
    const char* abortReason_ = "";

    // Fensterstatistik
    uint16_t n_ = 0;
    float sumRpm_ = 0.0f;
    float sumWatt_ = 0.0f;
    float minRpm_ = 0.0f;
    float maxRpm_ = 0.0f;

    SweepPoint pts_[kMapMaxLevels];
    uint8_t points_ = 0;
};

}  // namespace ergo
