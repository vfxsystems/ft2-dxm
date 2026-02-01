#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_unicode.h"

// DXM is now the default and only module format
void saveMusic(UNICHAR *filenameU);
bool saveDXM(UNICHAR *filenameU);
