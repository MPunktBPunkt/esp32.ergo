#include "control/WorkoutEngine.h"

namespace ergo {

const char* WorkoutEngine::stateName(WorkoutState s) {
    switch (s) {
        case WorkoutState::Idle: return "IDLE";
        case WorkoutState::Running: return "RUNNING";
        case WorkoutState::Paused: return "PAUSED";
        case WorkoutState::Done: return "DONE";
        default: return "?";
    }
}

void WorkoutEngine::reset() {
    stepCount_ = 0;
    stepIndex_ = 0;
    state_ = WorkoutState::Idle;
    name_[0] = 0;
    stepArmMs_ = 0;
    pauseAccumMs_ = 0;
    pauseAtMs_ = 0;
    sessionArmMs_ = 0;
    justAdvanced_ = false;
    for (uint8_t i = 0; i < kMaxSteps; i++) steps_[i] = WorkoutStep{};
}

bool WorkoutEngine::loadSteps(const WorkoutStep* steps, uint8_t count, const char* name) {
    if (!steps || count == 0 || count > kMaxSteps) return false;
    reset();
    stepCount_ = count;
    for (uint8_t i = 0; i < count; i++) {
        steps_[i] = steps[i];
        if (steps_[i].durationS == 0) steps_[i].durationS = 1;
        if (steps_[i].hrMax > 0 && steps_[i].hrSoft == 0) {
            steps_[i].hrSoft =
                (steps_[i].hrMax > 5) ? (uint8_t)(steps_[i].hrMax - 5) : steps_[i].hrMax;
        }
    }
    if (name && name[0]) {
        strncpy(name_, name, sizeof(name_) - 1);
        name_[sizeof(name_) - 1] = 0;
    } else {
        strncpy(name_, "custom", sizeof(name_) - 1);
    }
    return true;
}

bool WorkoutEngine::loadBuiltinPhysio(float scale) {
    if (scale < 0.05f) scale = 0.05f;
    if (scale > 2.0f) scale = 2.0f;
    WorkoutStep s[3];
    strncpy(s[0].label, "Einfahren", sizeof(s[0].label) - 1);
    s[0].durationS = (uint32_t)(120.0f * scale + 0.5f);
    s[0].powerW = 40.0f;
    s[0].hrMax = 120;
    s[0].hrSoft = 115;
    strncpy(s[1].label, "Hauptteil", sizeof(s[1].label) - 1);
    s[1].durationS = (uint32_t)(600.0f * scale + 0.5f);
    s[1].powerW = 60.0f;
    s[1].hrMax = 120;
    s[1].hrSoft = 115;
    strncpy(s[2].label, "Ausfahren", sizeof(s[2].label) - 1);
    s[2].durationS = (uint32_t)(120.0f * scale + 0.5f);
    s[2].powerW = 35.0f;
    s[2].hrMax = 120;
    s[2].hrSoft = 115;
    return loadSteps(s, 3, "Physio Grundlage");
}

float WorkoutEngine::resolvePower(const WorkoutStep& st) const {
    if (st.powerW > 0.0f) return st.powerW;
    if (st.ftpPct > 0.0f && ftpW_ > 0) return (float)ftpW_ * st.ftpPct / 100.0f;
    return 0.0f;
}

uint32_t WorkoutEngine::remainingFrom(uint8_t fromStep, uint32_t stepElapsedS) const {
    if (fromStep >= stepCount_) return 0;
    uint32_t rem = 0;
    for (uint8_t i = fromStep; i < stepCount_; i++) {
        if (i == fromStep) {
            const uint32_t d = steps_[i].durationS;
            rem += (stepElapsedS < d) ? (d - stepElapsedS) : 0;
        } else {
            rem += steps_[i].durationS;
        }
    }
    return rem;
}

void WorkoutEngine::enterStep(uint8_t idx, uint32_t nowMs) {
    stepIndex_ = idx;
    stepArmMs_ = nowMs ? nowMs : 1;
    pauseAccumMs_ = 0;
    pauseAtMs_ = 0;
    justAdvanced_ = true;
}

bool WorkoutEngine::start(uint32_t nowMs) {
    if (stepCount_ == 0) return false;
    state_ = WorkoutState::Running;
    sessionArmMs_ = nowMs ? nowMs : 1;
    enterStep(0, nowMs);
    return true;
}

void WorkoutEngine::pause(uint32_t nowMs) {
    if (state_ != WorkoutState::Running) return;
    state_ = WorkoutState::Paused;
    pauseAtMs_ = nowMs ? nowMs : 1;
}

void WorkoutEngine::resume(uint32_t nowMs) {
    if (state_ != WorkoutState::Paused) return;
    if (pauseAtMs_ > 0 && nowMs >= pauseAtMs_) pauseAccumMs_ += (nowMs - pauseAtMs_);
    pauseAtMs_ = 0;
    state_ = WorkoutState::Running;
}

bool WorkoutEngine::skip(uint32_t nowMs) {
    if (state_ != WorkoutState::Running && state_ != WorkoutState::Paused) return false;
    if (stepIndex_ + 1 >= stepCount_) {
        state_ = WorkoutState::Done;
        justAdvanced_ = false;
        return true;
    }
    state_ = WorkoutState::Running;
    pauseAtMs_ = 0;
    enterStep((uint8_t)(stepIndex_ + 1), nowMs);
    return true;
}

void WorkoutEngine::stop() {
    state_ = WorkoutState::Idle;
    stepIndex_ = 0;
    stepArmMs_ = 0;
    pauseAccumMs_ = 0;
    pauseAtMs_ = 0;
    justAdvanced_ = false;
}

WorkoutEngine::Tick WorkoutEngine::tick(uint32_t nowMs) {
    Tick t;
    t.state = state_;
    t.stepCount = stepCount_;
    t.stepIndex = stepIndex_;
    t.finished = (state_ == WorkoutState::Done);

    if (state_ == WorkoutState::Idle || stepCount_ == 0) return t;

    if (state_ == WorkoutState::Done) {
        t.finished = true;
        return t;
    }

    const WorkoutStep& st = steps_[stepIndex_];
    t.label = st.label;
    t.desiredW = resolvePower(st);
    t.hrMax = st.hrMax;
    t.hrSoft = st.hrSoft;

    uint32_t elapsedMs = 0;
    if (state_ == WorkoutState::Paused) {
        const uint32_t end = pauseAtMs_ ? pauseAtMs_ : nowMs;
        if (end >= stepArmMs_) elapsedMs = (end - stepArmMs_) - pauseAccumMs_;
    } else {
        if (nowMs >= stepArmMs_) elapsedMs = (nowMs - stepArmMs_) - pauseAccumMs_;
    }
    uint32_t stepElapsedS = elapsedMs / 1000;
    if (stepElapsedS > st.durationS) stepElapsedS = st.durationS;

    t.stepRemainingS = st.durationS - stepElapsedS;
    t.totalRemainingS = remainingFrom(stepIndex_, stepElapsedS);
    if (sessionArmMs_ > 0 && nowMs >= sessionArmMs_)
        t.elapsedS = (nowMs - sessionArmMs_) / 1000;

    t.justAdvanced = justAdvanced_;
    justAdvanced_ = false;

    if (state_ == WorkoutState::Running && stepElapsedS >= st.durationS) {
        if (stepIndex_ + 1 >= stepCount_) {
            state_ = WorkoutState::Done;
            t.state = WorkoutState::Done;
            t.finished = true;
            t.stepRemainingS = 0;
            t.totalRemainingS = 0;
            return t;
        }
        enterStep((uint8_t)(stepIndex_ + 1), nowMs);
        const WorkoutStep& ns = steps_[stepIndex_];
        t.stepIndex = stepIndex_;
        t.label = ns.label;
        t.desiredW = resolvePower(ns);
        t.hrMax = ns.hrMax;
        t.hrSoft = ns.hrSoft;
        t.stepRemainingS = ns.durationS;
        t.totalRemainingS = remainingFrom(stepIndex_, 0);
        t.justAdvanced = true;
        justAdvanced_ = false;
        t.state = state_;
    }

    return t;
}

}  // namespace ergo
