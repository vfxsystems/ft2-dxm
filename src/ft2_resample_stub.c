#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "ft2_wav_renderer.h"
#include "ft2_structs.h"
#include "ft2_sample_ed.h"

/*
 * Temporary stub implementations to satisfy the linker until the
 * real multi-channel resample-to-sample-slot logic is finished.
 *
 * NOTE: These functions merely act as placeholders so that we can
 *       keep the application building and test the surrounding UI.
 *       They should be replaced with full implementations later.
 */

#if 0 /* disabled now that real implementation exists */
bool renderSelectionToSlot(uint32_t channelMask, uint16_t startPos, uint16_t stopPos, uint8_t bitDepth)
{
    (void)channelMask;
    (void)startPos;
    (void)stopPos;
    (void)bitDepth;
    
    // TODO: implement real rendering logic
    fprintf(stderr, "[STUB] renderSelectionToSlot() called – not yet implemented.\n");
    return false;
}
#endif
