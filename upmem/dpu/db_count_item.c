#include <mram.h>

#include "param.h"
#if NR_DB_ITEMS <= 512
#include "./histogram/histogram_short.h"
#else
#include "./histogram/histogram_long.h"
#endif

#define MAX_BUF_ELEM    (MRAM_AVAILABLE >> 2)   // MRAM_AVAILABLE / sizeof(int32_t)
#define CACHE_ELEM      (BLOCK_SIZE >> 2)       // BLOCK_SIZE / sizeof(int32_t)

__host uint32_t count; // TODO: Consider copy this value before use (tasklet-safe?)

int main() {
#if NR_DB_ITEMS <= 512
    histogram_short(count);
#else
    histogram_long(count);
#endif

    return 0;
}