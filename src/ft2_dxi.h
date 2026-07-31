#ifndef FT2_DXI_H
#define FT2_DXI_H

/* "DXI" (DXM Instrument) - a standalone single-instrument synth patch file.
** Container format (little-endian, 44-byte header + payload):
**   offset  size  field
**   0       4     magic "DXI0"
**   4       2     u16 version (currently 1)
**   6       1     u8  engine tag (0=TF4, 1=Dexed, 2=V2, 3=OsTIrus)
**   7       1     u8  reserved (0)
**   8       32    char name[32] (NUL-padded)
**   40      4     u32 payload length
**   44      N     payload - byte-identical to the corresponding DXM module chunk's
**                 per-instrument payload for that engine, so patches round-trip
**                 losslessly between standalone .dxi files and embedded DXM state.
*/

#include <stdbool.h>
#include "ft2_unicode.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Saves the given instrument's synth patch (whichever engine its use* flag selects)
** to a standalone .dxi file. Returns false (and shows an error dialog) on failure,
** including when the instrument isn't using any synth engine.
*/
bool ft2_dxi_save_instrument(UNICHAR *filenameU, int instrID);

/* Loads a .dxi file into the given instrument: sets the matching use* flag (clearing
** the others), restores the name, and applies the patch to a live engine instance.
** Returns false (and shows an error dialog) on failure (bad file, corrupt header,
** unsupported version/engine tag).
*/
bool ft2_dxi_load_instrument(UNICHAR *filenameU, int instrID);

#ifdef __cplusplus
}
#endif

#endif /* FT2_DXI_H */
