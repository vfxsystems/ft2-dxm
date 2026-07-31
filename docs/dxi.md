# DXI Patch Workflow

DXI files are standalone synth patch files for the current instrument slot. They are part of the Disk Op. instrument workflow, not the Instrument Editor button layout.

## Loading

1. Open Disk Op.
2. Select `Instr.` in the item list.
3. Select `DXI` under `Save as:`.
4. Choose a `.dxi` file from the file list.

The selected patch is loaded into the current instrument. Instrument slot `00` is not a valid target for instrument data.

## Saving

1. Open Disk Op.
2. Select `Instr.` in the item list.
3. Select `DXI` under `Save as:`.
4. Enter a file name and press `Save`.

The filename is normalized to the `.dxi` extension before writing. Use `XI` in the same Disk Op. instrument screen when saving classic sample-based Fasttracker II instruments.
