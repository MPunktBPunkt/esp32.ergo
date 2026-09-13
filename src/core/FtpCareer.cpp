#include "core/FtpCareer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "core/Progression.h"

namespace ergo {
namespace {

const FtpCareerStage kStages[kFtpCareerStages] = {
    {"ftp_warm", "FTP-Warmup", "Einstieg · 50→70 %"},
    {"ss_3x12", "Sweet Spot 3x12", "Basis · 3×12′ @90 %"},
    {"ss_2x20", "Sweet Spot 2x20", "FTP-effizient · 2×20′ @92 %"},
    {"th_4x8", "Threshold 4x8", "Schwelle · 4×8′ @102 %"},
    {"ftp_2x20", "FTP 2x20", "Klassiker · 2×20′ @98 %"},
    {"over_under", "Over/Under", "95↔105 % · Laktat"},
    {"vo2_5x4", "VO2 5x4", "Obergrenze · 5×4′ @112 %"},
    {"test_ramp", "Ramp-Test", "FTP messen · dann neu skalieren"},
};

}  // namespace

const FtpCareerStage* ftpCareerStage(uint8_t index) {
    return index < kFtpCareerStages ? &kStages[index] : nullptr;
}

uint8_t ftpCareerStageCount() { return kFtpCareerStages; }

uint8_t ftpCareerIndexOf(const char* workoutId) {
    if (!workoutId || !workoutId[0]) return 255;
    for (uint8_t i = 0; i < kFtpCareerStages; i++) {
        if (strcmp(kStages[i].id, workoutId) == 0) return i;
    }
    return 255;
}

bool ftpCareerIsUnlocked(const FtpCareerState& st, uint8_t index) {
    return index <= st.unlocked && index < kFtpCareerStages;
}

void ftpCareerOnSessionEnd(FtpCareerState& st, const SessionSummary& s) {
    st.offerPending = false;
    st.offerClean = false;
    st.offerReason[0] = 0;
    strncpy(st.lastWorkoutId, s.workoutId, sizeof(st.lastWorkoutId) - 1);
    st.lastWorkoutId[sizeof(st.lastWorkoutId) - 1] = 0;

    if (st.unlocked >= kFtpCareerStages - 1) {
        strncpy(st.offerReason, "Karriere komplett", sizeof(st.offerReason) - 1);
        return;
    }
    const FtpCareerStage* cur = ftpCareerStage(st.unlocked);
    if (!cur) return;
    if (strcmp(s.workoutId, cur->id) != 0) {
        return;  // anderes Workout — kein Angebot
    }
    char reason[56];
    const bool clean = progressionIsClean(s, reason, sizeof(reason));
    st.offerClean = clean;
    strncpy(st.offerReason, reason, sizeof(st.offerReason) - 1);
    if (clean) {
        st.offerPending = true;
        snprintf(st.offerReason, sizeof(st.offerReason), "Stufe %u freischalten",
                 (unsigned)(st.unlocked + 2));
    } else {
        st.offerPending = true;  // UI zeigt Grund, Accept gesperrt
    }
}

bool ftpCareerAccept(FtpCareerState& st) {
    if (!st.offerPending || !st.offerClean) return false;
    if (st.unlocked >= kFtpCareerStages - 1) return false;
    st.unlocked++;
    st.offerPending = false;
    st.offerClean = false;
    st.offerReason[0] = 0;
    return true;
}

void ftpCareerDecline(FtpCareerState& st) {
    st.offerPending = false;
}

bool ftpCareerSetUnlocked(FtpCareerState& st, uint8_t unlocked) {
    if (unlocked >= kFtpCareerStages) return false;
    st.unlocked = unlocked;
    st.offerPending = false;
    st.offerClean = false;
    st.offerReason[0] = 0;
    return true;
}

size_t ftpCareerWriteJson(const FtpCareerState& st, char* buf, size_t bufLen) {
    if (!buf || bufLen < 16) return 0;
    int n = snprintf(buf, bufLen, "{\"unlocked\":%u}", (unsigned)st.unlocked);
    if (n < 0 || (size_t)n >= bufLen) return 0;
    return (size_t)n;
}

bool ftpCareerParseJson(const char* json, FtpCareerState& out) {
    out = FtpCareerState{};
    if (!json) return false;
    const char* p = strstr(json, "\"unlocked\"");
    if (!p) return true;  // leeres/fehlendes → Stufe 0
    p = strchr(p, ':');
    if (!p) return false;
    p++;
    while (*p == ' ') p++;
    unsigned v = (unsigned)strtoul(p, nullptr, 10);
    if (v >= kFtpCareerStages) v = kFtpCareerStages - 1;
    out.unlocked = (uint8_t)v;
    return true;
}

}  // namespace ergo
