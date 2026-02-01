 #pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

bool loadDXM(FILE *f, uint32_t filesize);

#ifdef __cplusplus
}
#endif 