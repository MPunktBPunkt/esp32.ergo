#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/SessionSummary.h"

/**
 * Ring der letzten Sessions (RAM). Persistenz/FS macht die App.
 * Arduino-frei.
 */
namespace ergo {

class SessionStore {
public:
    static constexpr uint8_t kMax = 12;

    void clear();
    bool append(const SessionSummary& s);
    /** Ersetzt die neueste Session (z. B. RPE/Notiz nachträglich). */
    bool replaceNewest(const SessionSummary& s);
    uint8_t count() const { return count_; }
    /** 0 = neueste. */
    bool at(uint8_t newestIndex, SessionSummary& out) const;

    size_t writeJsonLine(const SessionSummary& s, char* buf, size_t bufLen);
    bool parseJsonLine(const char* line, SessionSummary& out);

private:
    SessionSummary ring_[kMax];
    uint8_t head_ = 0;  // nächste Schreibposition
    uint8_t count_ = 0;
};

}  // namespace ergo
