#pragma once

#include "ble/FtmsCapabilities.h"
#include "ble/FtmsCodec.h"

/**
 * Der einzige Pfad, durch den ein Byte an den FTMS Control Point geht.
 *
 * Bewusst ohne Arduino: keine `String`, kein `millis()` — die Zeit kommt als
 * Parameter herein. Dadurch laeuft die gesamte Sicherheitslogik im nativen
 * Test, und zwar die, die spaeter auch auf dem Geraet laeuft. Bei einer
 * Komponente, die verhindern soll, dass ein Ergometer unter einem Menschen
 * stehen bleibt, ist das kein Luxus.
 *
 * Fuenf Regeln:
 *   1. Opcode-Whitelist. Alles Unbekannte wird abgelehnt.
 *   2. Ein Wattziel geht nur an Geraete, denen es zugetraut wird
 *      (`Capabilities::powerTargetTrusted`) — der Varon quittiert `05` mit
 *      Success, ohne das Feature zu melden.
 *   3. Klemmen auf den Schnitt aus Geraetebereich, Profilgrenze und
 *      absoluter Schranke, gerastert auf die Schrittweite des Geraets.
 *   4. Rampe **nur nach oben**. Last wegnehmen darf immer sofort passieren.
 *   5. Deadman. Bleibt das Lebenszeichen aus, meldet `expired()` das; der
 *      Aufrufer schickt dann Stop.
 */
namespace ergo {

struct LimiterConfig {
    /** Absolute Schranken — die letzte Instanz, auch gegen falsche 0x2AD6. */
    int16_t absMaxLevelTenths = 1000;
    int16_t absMaxPowerW = 600;

    /** Grenzen des aktiven Nutzerprofils. 0 heisst "keine Vorgabe". */
    int16_t profileMaxLevelTenths = 0;
    int16_t profileMaxPowerW = 0;

    /** Rampe nach oben: hoechstens `rampStepTenths` je `rampMs`.
     *  `rampStepTenths` 0 heisst "eine Schrittweite des Geraets". */
    uint32_t rampMs = 2000;
    uint16_t rampStepTenths = 0;

    /** Lebenszeichen-Fenster nach einem Lastkommando. 0 schaltet ab. */
    uint32_t deadmanMs = 0;

    /** 0x11 Simulation — nach Nachtest 4 freigebbar (Config). */
    bool allowSimulation = false;

    /**
     * Einmaliger/Diagnose-Pfad: 0x05 trotz fehlendem Feature / ohne 0x2AD8.
     * Nur fuer Nachtest 3 (raw&force). Default aus.
     */
    bool allowUntrustedPower = false;

    /** Steigungsklemme fuer 0x11, in 0,01 %. */
    int16_t maxGradeHundredth = 800;
};

enum class Decision : uint8_t {
    Allow = 0,  // unveraendert durchgelassen
    Clamp,      // beschnitten, die Bytes in `Verdict` sind massgeblich
    Defer,      // noch nicht — Rampe. Nichts senden, spaeter erneut fragen
    Deny,       // abgelehnt, mit Begruendung
};

const char* decisionName(Decision d);

class Limiter {
public:
    struct Verdict {
        Decision decision = Decision::Deny;
        uint8_t data[ftms::kMaxControlLen] = {0};
        uint8_t len = 0;
        const char* reason = "";

        bool sendable() const { return decision == Decision::Allow || decision == Decision::Clamp; }
    };

    void begin(const LimiterConfig& cfg) { cfg_ = cfg; }
    void setConfig(const LimiterConfig& cfg) { cfg_ = cfg; }
    const LimiterConfig& config() const { return cfg_; }

    /** Ohne Capabilities wird jedes Lastkommando abgelehnt. */
    void setCapabilities(const ftms::Capabilities* caps) { caps_ = caps; }

    /** Prueft und beschneidet. Aendert keinen Zustand — das tut noteWritten. */
    Verdict check(const uint8_t* cmd, size_t len, uint32_t nowMs) const;

    /** Nach einem tatsaechlich abgesetzten Write aufrufen. */
    void noteWritten(const uint8_t* cmd, size_t len, uint32_t nowMs);

    /** Lebenszeichen der Regelschleife — nicht des Schreibens. Solange ein
     *  Steuermodus laeuft, gehoert das in jeden Durchlauf. */
    void keepalive(uint32_t nowMs) { lastAlive_ = nowMs; }

    bool armed() const { return armed_; }
    bool expired(uint32_t nowMs) const;
    uint32_t remainingMs(uint32_t nowMs) const;

    /** Zuletzt gestellte Stufe in Zehnteln. Schattenwert: das Bike meldet
     *  sie nicht zurueck. -1, solange nichts gestellt wurde. */
    int16_t currentLevelTenths() const { return haveLevel_ ? level_ : (int16_t)-1; }

    uint16_t writeCount() const { return writes_; }
    uint16_t denyCount() const { return denies_; }

    /** Nach Verbindungsverlust: Schattenwert und Deadman sind ungueltig. */
    void reset();

    /**
     * Temporaere Aufwaerts-Rampe (z. B. Bridge-Gang). 0 = Config-Default.
     * Gilt bis clearRampOverride() oder reset().
     */
    void setRampOverrideMs(uint32_t rampMs) { rampOverrideMs_ = rampMs; }
    void clearRampOverride() { rampOverrideMs_ = 0; }
    uint32_t effectiveRampMs() const {
        return rampOverrideMs_ > 0 ? rampOverrideMs_ : cfg_.rampMs;
    }

    /** Wirksame Obergrenze fuer Stufen, in Zehnteln. */
    int16_t effectiveMaxLevelTenths() const;
    int16_t effectiveMinLevelTenths() const;
    int16_t effectiveMaxPowerW() const;

private:
    Verdict deny(const char* reason) const;
    Verdict allow(const uint8_t* cmd, size_t len) const;
    Verdict clampTo(const uint8_t* data, size_t len, const char* reason) const;
    Verdict checkResistance(const uint8_t* cmd, size_t len, uint32_t nowMs) const;
    Verdict checkPower(const uint8_t* cmd, size_t len) const;
    Verdict checkSimulation(const uint8_t* cmd, size_t len) const;
    int16_t snapLevel(int16_t tenths) const;

    LimiterConfig cfg_;
    const ftms::Capabilities* caps_ = nullptr;

    int16_t level_ = 0;
    bool haveLevel_ = false;
    uint32_t lastLevelWriteMs_ = 0;
    uint32_t rampOverrideMs_ = 0;
    bool armed_ = false;
    uint32_t lastAlive_ = 0;
    uint16_t writes_ = 0;
    mutable uint16_t denies_ = 0;
};

}  // namespace ergo
