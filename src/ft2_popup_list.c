/* Generic scrollable pop-up list for ft2-dxm
 * Minimal implementation: fixed-width box, small font, mouse + kbd nav.
 */

#include <string.h>
#include "ft2_popup_list.h"
#include "ft2_gui.h"      /* textOut(), fillRect(), FRAMEWORK_TYPE1 */
#include "ft2_video.h"    /* screen buffer info */
#include "ft2_mouse.h"    /* mouse X/Y globals */
#include "ft2_header.h"   /* key repeat state */

#define LIST_ROW_HEIGHT 10  /* tiny font height */
#define LIST_VISIBLE_ROWS 10
#define LIST_WIDTH 140
#define LIST_BORDER 2

static struct
{
    bool visible;
    int16_t x, y;
    int32_t count;
    const char **items;
    int32_t sel;
    int32_t scroll;
    popupListCallback cb;
    void *ctx;
} pl;

static void redraw(void)
{
    /* background */
        // background
    fillRect(pl.x, pl.y, LIST_WIDTH, LIST_ROW_HEIGHT * LIST_VISIBLE_ROWS + LIST_BORDER * 2, PAL_DESKTOP);

    /* border */
    drawFramework(pl.x - 1, pl.y - 1, LIST_WIDTH + 2, LIST_ROW_HEIGHT * LIST_VISIBLE_ROWS + LIST_BORDER * 2 + 2, FRAMEWORK_TYPE1);

    /* visible rows */
    for (int i = 0; i < LIST_VISIBLE_ROWS; i++)
    {
        int32_t idx = pl.scroll + i;
        if (idx >= pl.count) break;

        uint8_t col = (idx == pl.sel) ? PAL_BTNTEXT : PAL_FORGRND; /* selected vs normal text */
        textOut(pl.x + LIST_BORDER, pl.y + LIST_BORDER + i * LIST_ROW_HEIGHT, col, pl.items[idx]);
    }
}

static void clampSel(void)
{
    if (pl.sel < 0) pl.sel = 0;
    if (pl.sel >= pl.count) pl.sel = pl.count - 1;

    /* adjust scroll */
    if (pl.sel < pl.scroll) pl.scroll = pl.sel;
    if (pl.sel >= pl.scroll + LIST_VISIBLE_ROWS) pl.scroll = pl.sel - LIST_VISIBLE_ROWS + 1;
}

bool popupListIsVisible(void) { return pl.visible; }

void popupListShow(int16_t x, int16_t y, const char **items, int32_t count, int32_t initialSel,
                   popupListCallback cb, void *ctx)
{
    if (count <= 0 || items == NULL) return;

    memset(&pl, 0, sizeof (pl));
    pl.visible = true;
    pl.x = x;
    pl.y = y;
    pl.items = items;
    pl.count = count;
    pl.sel = (initialSel >= 0 && initialSel < count) ? initialSel : 0;
    pl.scroll = 0;
    pl.cb = cb;
    pl.ctx = ctx;

    clampSel();
    redraw();
}

void popupListHide(void)
{
    if (!pl.visible) return;
    pl.visible = false;

    /* redraw area under popup by forcing GUI refresh; simplest is full screen flag */
    // full GUI redraw after hiding popup
    drawGUIOnRunTime(); /* uses global ui struct from ft2_gui */
}

static void acceptSel(bool canceled)
{
    if (pl.cb)
        pl.cb(canceled ? -1 : pl.sel, pl.ctx);
    popupListHide();
}

void popupListHandleKey(SDL_Scancode scancode, bool keyDown)
{
    if (!pl.visible || !keyDown) return;

    switch (scancode)
    {
        case SDL_SCANCODE_ESCAPE: acceptSel(true); break;
        case SDL_SCANCODE_RETURN: acceptSel(false); break;
        case SDL_SCANCODE_UP:     pl.sel--; clampSel(); redraw(); break;
        case SDL_SCANCODE_DOWN:   pl.sel++; clampSel(); redraw(); break;
        case SDL_SCANCODE_PAGEUP: pl.sel -= LIST_VISIBLE_ROWS; clampSel(); redraw(); break;
        case SDL_SCANCODE_PAGEDOWN: pl.sel += LIST_VISIBLE_ROWS; clampSel(); redraw(); break;
        default: break;
    }
}

void popupListHandleMouse(int16_t mouseX, int16_t mouseY, bool buttonDown)
{
    if (!pl.visible) return;

    if (!buttonDown) return;

    /* inside popup? */
    if (mouseX >= pl.x && mouseX < pl.x + LIST_WIDTH && mouseY >= pl.y && mouseY < pl.y + LIST_ROW_HEIGHT * LIST_VISIBLE_ROWS)
    {
        int row = (mouseY - pl.y) / LIST_ROW_HEIGHT;
        int idx = pl.scroll + row;
        if (idx < pl.count)
        {
            pl.sel = idx; redraw(); acceptSel(false);
        }
    }
    else
    {
        /* click outside cancels */
        acceptSel(true);
    }
}

void popupListDraw(void)
{
    if (pl.visible) redraw();
}
