#ifndef FT2_POPUP_LIST_H
#define FT2_POPUP_LIST_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL_scancode.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*popupListCallback)(int32_t index, void *ctx); /* -1 if canceled */

bool popupListIsVisible(void);
void popupListShow(int16_t x, int16_t y, const char **items, int32_t count, int32_t initialSel,
                   popupListCallback cb, void *ctx);
void popupListHide(void);

/* Input handling - call from the global key/mouse handlers when visible */
void popupListHandleKey(SDL_Scancode scancode, bool keyDown);
void popupListHandleMouse(int16_t mouseX, int16_t mouseY, bool buttonDown);

/* Frame draw hook */
void popupListDraw(void);

#ifdef __cplusplus
}
#endif

#endif /* FT2_POPUP_LIST_H */
