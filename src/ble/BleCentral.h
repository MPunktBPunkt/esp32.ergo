#pragma once

#include <ArduinoJson.h>
#include <NimBLEDevice.h>

#include "BleTypes.h"
#include "core/ConfigStore.h"

namespace ergo {

/**
 * Verbindungsschicht: Scan, Connect, Merken, Reconnect — fuer zwei Rollen.
 *
 * Kennt FTMS absichtlich nur als Advertising-Kennzeichen (`hasFtms`), damit
 * beim Scannen die richtige Zeile hervorgehoben werden kann. Alles darueber
 * liegt in FtmsClient bzw. HrClient; die bekommen von hier nur den fertig
 * verbundenen `NimBLEClient*` und die Meldung, wann er kommt und geht.
 *
 * Warum die Trennung: ein anderes Ergometer oder ein anderer Gurt aendert die
 * Protokollschicht, nicht die Verbindungsschicht. Genau diese Grenze soll ein
 * Geraetewechsel spaeter nicht ueberschreiten.
 */
class BleCentral {
public:
    struct Link {
        LinkState state = LinkState::Idle;
        NimBLEClient* client = nullptr;
        char mac[18] = {0};
        char name[32] = {0};
        uint8_t addrType = 0;
        int8_t rssi = 0;
        uint32_t connectedAt = 0;
        uint32_t lostAt = 0;
        uint16_t losses = 0;
        uint16_t reconnects = 0;
        /** Vom Disconnect-Callback gesetzt; nur loop() darf aufraeumen. */
        volatile bool dropped = false;
    };

    using LinkEvent = void (*)(Role role, bool up);

    void begin(ConfigStore* cfg);
    void loop();

    /** Wird nach Discovering (up = true) bzw. nach Verlust gerufen. Laeuft im
     *  loop()-Kontext, nie im BLE-Callback — dort waere jede GATT-Operation
     *  ein Deadlock-Risiko. */
    void setLinkEvent(LinkEvent cb) { event_ = cb; }

    // ── Scan ────────────────────────────────────────────────────────────────
    bool startScan(uint16_t seconds = 8);
    void stopScan();
    bool scanning() const;
    uint8_t scanCount() const { return scanCount_; }
    void clearScan();
    void scanToJson(JsonArray arr) const;

    // ── Verbindung ──────────────────────────────────────────────────────────
    /** Verbindet und merkt das Geraet fuer die Rolle. `err` optional. */
    bool connectRole(Role role, const char* mac, int addrTypeHint, char* err, size_t errLen);
    /** Trennt und unterdrueckt den Reconnect bis zum naechsten connectRole. */
    void disconnectRole(Role role);
    /** Trennt und loescht die Merkung aus der Config. */
    void forgetRole(Role role);
    /** Verbindet das gemerkte Geraet, falls eines hinterlegt ist. */
    bool reconnectRole(Role role, char* err, size_t errLen);

    const Link& link(Role role) const { return links_[(uint8_t)role]; }
    bool ready(Role role) const { return links_[(uint8_t)role].state == LinkState::Ready; }
    NimBLEClient* client(Role role) const { return links_[(uint8_t)role].client; }
    /** Von FtmsClient/HrClient zu setzen, sobald die Abos stehen. */
    void markReady(Role role);

    uint8_t linkCount() const;
    void linksToJson(JsonObject obj) const;
    void appendStatusJson(JsonObject obj) const;

    /** Not-Trennung aller Links. Kein Stop-Kommando — das gehoert in
     *  FtmsClient, der weiss, ob ueberhaupt ein Control Point da ist. */
    void disconnectAll();

    // ── NimBLE-Callbacks (nicht selbst aufrufen) ────────────────────────────
    void onScanResult(NimBLEAdvertisedDevice* dev);
    void onClientDisconnect(NimBLEClient* c);

private:
    int findScan(const char* mac) const;
    uint8_t resolveAddrType(const char* mac) const;
    void release(Role role);
    /** Slot leeren. `memset` statt `Link{}`, weil `dropped` volatile ist und
     *  die Familie diesen Weg schon in der Sonde geht. */
    void clearLink(Role role, const char* keepMac = nullptr);
    void rememberRole(Role role, const Link& l);
    const char* memoMac(Role role) const;
    int8_t memoAddrType(Role role) const;

    ConfigStore* cfg_ = nullptr;
    LinkEvent event_ = nullptr;

    Link links_[kRoleCount];
    bool suppress_[kRoleCount] = {false, false};
    uint32_t nextRetryAt_[kRoleCount] = {0, 0};
    uint8_t retries_[kRoleCount] = {0, 0};
    /** Von loop() abzuarbeitende Ereignisse — im Callback nur gesetzt. */
    volatile bool pendingUp_[kRoleCount] = {false, false};

    ScanEntry scan_[ERGO_MAX_SCAN];
    uint8_t scanCount_ = 0;
    uint32_t scanUntil_ = 0;
};

}  // namespace ergo
