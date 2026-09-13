#pragma once

#include <stddef.h>
#include <stdint.h>

#include "core/SessionSummary.h"

/**
 * FTP-Karriere: Stufen freischalten (Sweet Spot → Threshold → O/U → VO2 → Test).
 * Arduino-frei. Persistenz (JSON) macht die App.
 */
namespace ergo {

static constexpr uint8_t kFtpCareerStages = 8;

struct FtpCareerStage {
    const char* id;
    const char* name;
    const char* blurb;
};

struct FtpCareerState {
    /** Hoechste freigeschaltete Stufe (0 = nur erste). */
    uint8_t unlocked = 0;
    bool offerPending = false;
    bool offerClean = false;
    char offerReason[56] = {};
    char lastWorkoutId[24] = {};
};

const FtpCareerStage* ftpCareerStage(uint8_t index);
uint8_t ftpCareerStageCount();

/** Index der Stufe fuer workoutId, oder 255 wenn unbekannt. */
uint8_t ftpCareerIndexOf(const char* workoutId);

bool ftpCareerIsUnlocked(const FtpCareerState& st, uint8_t index);

/**
 * Nach Session-Ende: Angebot zum Freischalten der naechsten Stufe,
 * wenn die aktuelle Stufe sauber abgeschlossen wurde.
 */
void ftpCareerOnSessionEnd(FtpCareerState& st, const SessionSummary& s);

/** Angebot annehmen → unlocked++. */
bool ftpCareerAccept(FtpCareerState& st);

void ftpCareerDecline(FtpCareerState& st);

/** Manuell setzen (0…count-1). */
bool ftpCareerSetUnlocked(FtpCareerState& st, uint8_t unlocked);

/** JSON: {"unlocked":N} — return Bytes oder 0. */
size_t ftpCareerWriteJson(const FtpCareerState& st, char* buf, size_t bufLen);
bool ftpCareerParseJson(const char* json, FtpCareerState& out);

}  // namespace ergo
