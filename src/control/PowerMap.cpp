#include "PowerMap.h"

namespace ergo {

static constexpr uint8_t kMagic0 = 0x45;  // 'E'
static constexpr uint8_t kMagic1 = 0x4D;  // 'M'
static constexpr uint8_t kVersion = 1;

/** Obergrenze der Mittelung. Danach wird aus dem laufenden Mittelwert ein
 *  exponentieller — die Flaeche soll sich noch bewegen koennen, wenn sich das
 *  Geraet aendert (Riemen, Temperatur), statt in alten Werten festzufrieren. */
static constexpr uint16_t kMeanCap = 32;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void PowerMap::begin(uint8_t levelCount, int16_t levelMinTenths, uint16_t levelStepTenths) {
    if (levelStepTenths == 0) levelStepTenths = 10;
    const uint8_t stored = levelCount > kMapMaxLevels ? kMapMaxLevels : levelCount;
    // Ein geaenderter Bereich macht die alte Flaeche bedeutungslos: Stufe 8
    // von 16 ist nicht Stufe 8 von 24. Verglichen wird die ungeklemmte Zahl,
    // sonst sehen 16 und 24 Stufen gleich aus.
    if (reqLevels_ != levelCount || minTenths_ != levelMinTenths ||
        stepTenths_ != levelStepTenths) {
        clear();
    }
    levels_ = stored;
    reqLevels_ = levelCount;
    truncated_ = levelCount > kMapMaxLevels;
    minTenths_ = levelMinTenths;
    stepTenths_ = levelStepTenths;
}

void PowerMap::clear() {
    for (uint8_t l = 0; l < kMapMaxLevels; l++) {
        for (uint8_t b = 0; b < kCadBands; b++) cells_[l][b] = MapCell{};
    }
}

int8_t PowerMap::bandOf(float rpm) {
    if (rpm < kCadMin) return -1;
    const int idx = (int)((rpm - kCadMin) / kCadStep);
    if (idx < 0 || idx >= (int)kCadBands) return -1;
    return (int8_t)idx;
}

int8_t PowerMap::indexOf(int16_t levelTenths) const {
    if (!levels_ || stepTenths_ == 0) return -1;
    const int32_t off = (int32_t)levelTenths - (int32_t)minTenths_;
    if (off < 0) return -1;
    if (off % (int32_t)stepTenths_ != 0) return -1;
    const int32_t idx = off / (int32_t)stepTenths_;
    if (idx >= (int32_t)levels_) return -1;
    return (int8_t)idx;
}

int16_t PowerMap::tenthsOf(uint8_t levelIdx) const {
    return (int16_t)(minTenths_ + (int32_t)levelIdx * stepTenths_);
}

const MapCell& PowerMap::cell(uint8_t levelIdx, uint8_t bandIdx) const {
    static const MapCell empty;
    if (levelIdx >= kMapMaxLevels || bandIdx >= kCadBands) return empty;
    return cells_[levelIdx][bandIdx];
}

// ──────────────────────────────────────────────────────────────── Lernen

bool PowerMap::add(int16_t levelTenths, float rpm, float watt, uint32_t nowS, bool fromSweep) {
    if (!ready()) return false;
    const int8_t li = indexOf(levelTenths);
    if (li < 0) return false;
    const int8_t bi = bandOf(rpm);
    if (bi < 0) return false;
    if (watt < 0.0f || watt > 3000.0f) return false;

    MapCell& c = at((uint8_t)li, (uint8_t)bi);
    c.lastS = nowS;

    if (fromSweep) {
        if (!c.sweep) {
            // Ein gefuehrter Punkt ersetzt passiv Gelerntes. Er entstand unter
            // gehaltener Kadenz; das passive Mittel kann jede Streuung enthalten.
            c.watt = watt;
            c.samples = 1;
            c.sweep = true;
        } else {
            if (c.samples < 0xFFFF) c.samples++;
            const uint16_t n = c.samples < kMeanCap ? c.samples : kMeanCap;
            c.watt += (watt - c.watt) / (float)n;
        }
        return true;
    }

    if (c.samples == 0) {
        c.watt = watt;
        c.samples = 1;
        return true;
    }
    // Sweep-Punkte werden nur vorsichtig nachgezogen, damit eine unruhige
    // Fahrt eine sauber gemessene Stuetzstelle nicht verwaescht.
    const uint16_t n = c.samples < kMeanCap ? (uint16_t)(c.samples + 1) : kMeanCap;
    const float alpha = c.sweep ? (1.0f / (float)kMeanCap) : (1.0f / (float)n);
    c.watt += (watt - c.watt) * alpha;
    if (c.samples < 0xFFFF) c.samples++;
    return true;
}

// ───────────────────────────────────────────────────────────── Schaetzen

bool PowerMap::rowEstimate(uint8_t levelIdx, float rpm, float& out) const {
    if (levelIdx >= levels_) return false;
    const float r = clampf(rpm, kCadMin + 0.01f, kCadMin + kCadStep * kCadBands - 0.01f);

    // Nachbarn um die Kadenz suchen: die naechste belegte Zelle unterhalb und
    // oberhalb, dann linear zwischen den Bandmitten interpolieren.
    int lo = -1, hi = -1;
    for (int b = 0; b < (int)kCadBands; b++) {
        if (!cells_[levelIdx][b].known()) continue;
        const float c = bandCenter((uint8_t)b);
        if (c <= r) lo = b;
        if (c >= r && hi < 0) hi = b;
    }
    if (lo < 0 && hi < 0) return false;
    if (lo < 0) {
        out = cells_[levelIdx][hi].watt;
        return true;
    }
    if (hi < 0 || hi == lo) {
        out = cells_[levelIdx][lo].watt;
        return true;
    }
    const float cl = bandCenter((uint8_t)lo), ch = bandCenter((uint8_t)hi);
    const float t = (r - cl) / (ch - cl);
    out = cells_[levelIdx][lo].watt + t * (cells_[levelIdx][hi].watt - cells_[levelIdx][lo].watt);
    return true;
}

bool PowerMap::estimate(int16_t levelTenths, float rpm, float& wattOut) const {
    if (!ready()) return false;
    const int8_t li = indexOf(levelTenths);
    if (li < 0) return false;

    if (rowEstimate((uint8_t)li, rpm, wattOut)) return true;

    // Zeile leer: ueber die Nachbarstufen interpolieren. Das ist genau der
    // Fall, den passives Lernen erzeugt — man faehrt drei Stufen, nicht 16.
    int lo = -1, hi = -1;
    float wlo = 0.0f, whi = 0.0f;
    for (int l = li - 1; l >= 0; l--) {
        if (rowEstimate((uint8_t)l, rpm, wlo)) { lo = l; break; }
    }
    for (int l = li + 1; l < (int)levels_; l++) {
        if (rowEstimate((uint8_t)l, rpm, whi)) { hi = l; break; }
    }
    if (lo < 0 && hi < 0) return false;
    if (lo < 0) { wattOut = whi; return true; }
    if (hi < 0) { wattOut = wlo; return true; }
    const float t = (float)(li - lo) / (float)(hi - lo);
    wattOut = wlo + t * (whi - wlo);
    return true;
}

bool PowerMap::bestLevel(float targetWatt, float rpm, int16_t& tenthsOut, bool& ceiling) const {
    ceiling = false;
    if (!ready()) return false;
    int bestKnown = -1;
    for (uint8_t l = 0; l < levels_; l++) {
        float w = 0.0f;
        if (!estimate(tenthsOf(l), rpm, w)) continue;
        bestKnown = l;
        if (w >= targetWatt) {
            tenthsOut = tenthsOf(l);
            return true;
        }
    }
    if (bestKnown < 0) return false;
    // Selbst die hoechste bekannte Stufe reicht nicht. Der Aufrufer muss das
    // anzeigen, nicht verschweigen — Abnahmekriterium 8.
    tenthsOut = tenthsOf((uint8_t)bestKnown);
    ceiling = true;
    return true;
}

// ──────────────────────────────────────────────────────────────── Masse

float PowerMap::coverage() const {
    if (!ready()) return 0.0f;
    uint16_t n = 0;
    for (uint8_t l = 0; l < levels_; l++) {
        for (uint8_t b = 0; b < kCadBands; b++) {
            if (cells_[l][b].known()) n++;
        }
    }
    return (float)n / (float)((uint16_t)levels_ * kCadBands);
}

uint8_t PowerMap::levelsCovered() const {
    uint8_t n = 0;
    for (uint8_t l = 0; l < levels_; l++) {
        for (uint8_t b = 0; b < kCadBands; b++) {
            if (cells_[l][b].known()) { n++; break; }
        }
    }
    return n;
}

uint8_t PowerMap::bandsCovered() const {
    uint8_t n = 0;
    for (uint8_t b = 0; b < kCadBands; b++) {
        for (uint8_t l = 0; l < levels_; l++) {
            if (cells_[l][b].known()) { n++; break; }
        }
    }
    return n;
}

uint16_t PowerMap::pointCount() const {
    uint32_t n = 0;
    for (uint8_t l = 0; l < levels_; l++) {
        for (uint8_t b = 0; b < kCadBands; b++) n += cells_[l][b].samples;
    }
    return n > 0xFFFF ? (uint16_t)0xFFFF : (uint16_t)n;
}

uint16_t PowerMap::sweepCells() const {
    uint16_t n = 0;
    for (uint8_t l = 0; l < levels_; l++) {
        for (uint8_t b = 0; b < kCadBands; b++) {
            if (cells_[l][b].sweep) n++;
        }
    }
    return n;
}

// ────────────────────────────────────────────────────── Serialisierung

static void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}
static uint16_t get16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t get32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

size_t PowerMap::save(uint8_t* buf, size_t cap) const {
    if (!ready() || !buf) return 0;
    const size_t need = byteSize();
    if (cap < need) return 0;

    buf[0] = kMagic0;
    buf[1] = kMagic1;
    buf[2] = kVersion;
    buf[3] = levels_;
    put16(buf + 4, (uint16_t)minTenths_);
    put16(buf + 6, stepTenths_);

    uint8_t* p = buf + kHeaderSize;
    for (uint8_t l = 0; l < levels_; l++) {
        for (uint8_t b = 0; b < kCadBands; b++) {
            const MapCell& c = cells_[l][b];
            // Zehntelwatt als uint16: bis 6553,5 W, weit jenseits allem, was
            // ein Ergometer meldet, und halb so gross wie ein float.
            float w = c.watt * 10.0f;
            if (w < 0.0f) w = 0.0f;
            if (w > 65535.0f) w = 65535.0f;
            put16(p, (uint16_t)(w + 0.5f));
            put16(p + 2, c.samples);
            put32(p + 4, c.lastS);
            p[8] = c.sweep ? 1 : 0;
            p += kCellSize;
        }
    }
    return need;
}

bool PowerMap::load(const uint8_t* buf, size_t len) {
    if (!buf || len < kHeaderSize) return false;
    if (buf[0] != kMagic0 || buf[1] != kMagic1 || buf[2] != kVersion) return false;
    const uint8_t levels = buf[3];
    if (levels == 0 || levels > kMapMaxLevels) return false;
    const uint16_t step = get16(buf + 6);
    if (step == 0) return false;
    const size_t need = kHeaderSize + (size_t)levels * kCadBands * kCellSize;
    if (len < need) return false;

    clear();
    levels_ = levels;
    // Die gemeldete Stufenzahl steht nicht im Blob. Sie auf das Gespeicherte
    // zu setzen ist die Annahme, dass es dasselbe Geraet ist — und genau die
    // prueft das naechste `begin()` gegen die echten Faehigkeiten.
    reqLevels_ = levels;
    truncated_ = false;
    minTenths_ = (int16_t)get16(buf + 4);
    stepTenths_ = step;

    const uint8_t* p = buf + kHeaderSize;
    for (uint8_t l = 0; l < levels_; l++) {
        for (uint8_t b = 0; b < kCadBands; b++) {
            MapCell& c = cells_[l][b];
            c.watt = (float)get16(p) / 10.0f;
            c.samples = get16(p + 2);
            c.lastS = get32(p + 4);
            c.sweep = p[8] != 0;
            p += kCellSize;
        }
    }
    return true;
}

}  // namespace ergo
