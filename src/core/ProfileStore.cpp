#include "core/Profile.h"

namespace ergo {

const Profile* ProfileStore::active() const {
    if (!activeId_[0]) return nullptr;
    const int i = findIndex(activeId_);
    return i >= 0 ? &items_[static_cast<uint8_t>(i)] : nullptr;
}

int ProfileStore::findIndex(const char* id) const {
    if (!id || !id[0]) return -1;
    for (uint8_t i = 0; i < count_; ++i) {
        if (strcmp(items_[i].id, id) == 0) return static_cast<int>(i);
    }
    return -1;
}

bool ProfileStore::sanitize(Profile& p) {
    if (!p.id[0] || !p.name[0]) return false;
    // id: nur druckbare ASCII ohne Leerzeichen am Anfang
    for (char* c = p.id; *c; ++c) {
        if (*c < 33 || *c > 126) return false;
    }
    if (p.maxHr > kHrCeilingMax) p.maxHr = kHrCeilingMax;
    if (p.hrMax > kHrCeilingMax) p.hrMax = kHrCeilingMax;
    if (p.maxPowerW < 0) p.maxPowerW = 0;
    if (p.maxLevelTenths < 0) p.maxLevelTenths = 0;
    return true;
}

bool ProfileStore::put(const Profile& in) {
    Profile p = in;
    if (!sanitize(p)) return false;

    const int i = findIndex(p.id);
    if (i >= 0) {
        items_[static_cast<uint8_t>(i)] = p;
        return true;
    }
    if (count_ >= kMaxProfiles) return false;
    items_[count_++] = p;
    return true;
}

bool ProfileStore::get(const char* id, Profile& out) const {
    const int i = findIndex(id);
    if (i < 0) return false;
    out = items_[static_cast<uint8_t>(i)];
    return true;
}

bool ProfileStore::remove(const char* id) {
    const int i = findIndex(id);
    if (i < 0) return false;
    if (activeId_[0] && strcmp(activeId_, id) == 0) activeId_[0] = '\0';
    for (uint8_t j = static_cast<uint8_t>(i); j + 1 < count_; ++j) items_[j] = items_[j + 1];
    --count_;
    return true;
}

bool ProfileStore::select(const char* id, bool sessionLocked) {
    if (sessionLocked) return false;
    if (!id || !id[0]) {
        activeId_[0] = '\0';
        return true;
    }
    if (findIndex(id) < 0) return false;
    profileCopyId(activeId_, sizeof(activeId_), id);
    return true;
}

void ProfileStore::clearActive() { activeId_[0] = '\0'; }

void ProfileStore::applyTo(LimiterConfig& lc) const {
    lc.profileMaxLevelTenths = 0;
    lc.profileMaxPowerW = 0;
    const Profile* a = active();
    if (!a) return;
    lc.profileMaxLevelTenths = a->maxLevelTenths;
    lc.profileMaxPowerW = a->maxPowerW;
}

}  // namespace ergo
