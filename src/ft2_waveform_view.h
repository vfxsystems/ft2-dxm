#ifndef FT2_WAVEFORM_VIEW_H
#define FT2_WAVEFORM_VIEW_H

#include <stdint.h>
#include <stdbool.h>

// Waveform view functions
void waveformViewInit(void);
void waveformViewShow(bool show);
bool waveformViewIsVisible(void);
bool waveformViewHandleMouse(int mouseX, int mouseY, bool mouseButtonDown);
void waveformViewUpdate(void);
void waveformViewRender(void);
void waveformViewSetActive(bool active);
void waveformViewGetBounds(int *x, int *y, int *w, int *h);
void waveformViewToggle(void);

// Legacy compatibility function
void ft2_waveform_view_draw(int x, int y, int width, int height);

#endif // FT2_WAVEFORM_VIEW_H 