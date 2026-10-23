#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Explizite PSRAM-Allokation — nicht `malloc` umbiegen.
 *
 * technology-audit `esp32-s3-silicon`: verbautes PSRAM ist ungenutzt, solange
 * `BOARD_HAS_PSRAM` + passendes `memory_type` fehlen. hu-probe hat gemessen:
 * Heap wird besser, wenn grosse Ringe in PSRAM liegen; BLE/WiFi/DMA bleiben
 * intern (`callocPreferPsram`, nicht `CONFIG_SPIRAM_USE_MALLOC`).
 *
 * Nur unter Arduino. Native Tests kompilieren diese Datei nicht.
 */
namespace ergo {

/** true, wenn der Treiber PSRAM gesehen hat (`ESP.getPsramSize() > 0`). */
bool psramReady();

size_t psramSize();
size_t freePsram();
size_t freeHeap();
size_t minFreeHeap();

/**
 * `calloc` zuerst in PSRAM, sonst interner Heap. `usedPsram` sagt, wo es
 * gelandet ist (false bei Fallback oder wenn PSRAM fehlt).
 */
void* callocPreferPsram(size_t count, size_t size, bool* usedPsram = nullptr);

void* reallocPreferPsram(void* ptr, size_t size);
void freePrefer(void* ptr);

/** true, wenn der Zeiger in externem RAM liegt (nach erfolgreichem PSRAM-calloc). */
bool ptrInPsram(const void* p);

}  // namespace ergo
