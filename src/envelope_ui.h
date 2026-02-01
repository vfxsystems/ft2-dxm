#pragma once

#include <stdbool.h>

// Envelope UI drawing and input handling
// env_handleRedraw: draw envelope graphs (volume, panning, or synth-specific) in their frames
// env_handleMouse: handle mouse input for envelope editing; returns true if swallowed
void env_handleRedraw(void);
bool env_handleMouse(bool mouseB3332uttonDown);
