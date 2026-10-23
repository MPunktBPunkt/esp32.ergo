#include "PsramAlloc.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <stdlib.h>

namespace ergo {

bool psramReady() { return ESP.getPsramSize() > 0; }

size_t psramSize() { return ESP.getPsramSize(); }

size_t freePsram() { return ESP.getFreePsram(); }

size_t freeHeap() { return ESP.getFreeHeap(); }

size_t minFreeHeap() { return ESP.getMinFreeHeap(); }

void* callocPreferPsram(size_t count, size_t size, bool* usedPsram) {
    if (usedPsram) *usedPsram = false;
    if (count == 0 || size == 0) return nullptr;
    const size_t bytes = count * size;
    if (psramReady()) {
        void* p = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM);
        if (p) {
            if (usedPsram) *usedPsram = true;
            return p;
        }
    }
    return calloc(count, size);
}

void* reallocPreferPsram(void* ptr, size_t size) {
    if (!ptr) return callocPreferPsram(1, size);
    if (size == 0) {
        freePrefer(ptr);
        return nullptr;
    }
    if (ptrInPsram(ptr) && psramReady()) {
        void* p = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM);
        if (p) return p;
    }
    return realloc(ptr, size);
}

void freePrefer(void* ptr) {
    if (ptr) free(ptr);
}

bool ptrInPsram(const void* p) { return p && esp_ptr_external_ram(p); }

}  // namespace ergo
