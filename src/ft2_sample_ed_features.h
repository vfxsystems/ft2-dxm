#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ft2_header.h"

// External variables
extern int32_t *sliceMarkers;
extern int32_t numSliceMarkers;
extern bool sliceMarkersVisible;
extern int32_t selectedSliceMarker;
extern bool draggingSliceMarker;

// Sample editor features
void pbSampleResample(void);
void pbSampleEcho(void);
void pbSampleMix(void);
void pbSampleVolume(void);
void pbSampleSlicer(void);
void handleEchoToolPanic(void);

// Slice marker functions
void drawSliceMarkers(void);
bool handleSliceMarkerMouseDown(int32_t mouseX, int32_t mouseY);
void handleSliceMarkerMouseDrag(int32_t mouseX);
void handleSliceMarkerMouseUp(void);
void handleSliceMarkerDelete(void);
bool getSliceMarkersVisible(void);
int32_t getNumSliceMarkers(void);
int32_t *getSliceMarkers(void);
void removeSliceMarker(int32_t index);

// Internal functions
void setupSlicerBoxWidgets(void);
