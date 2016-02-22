#pragma once

#include <stdio.h>
#include "defs.h"

#ifdef __cplusplus
#define EXTC extern "C"
#else
#define EXTC
#endif

#define BINLNWIDTH 20
EXTC DLLEXPORT void
binlnshow(const char *prefix, FILE *wfp, unsigned off,
          const uint8_t *buf0, const uint8_t *buf1);
EXTC DLLEXPORT void
binshow(const char *prefix, FILE *wfp,
        const uint8_t *buf0, const uint8_t *buf1);
