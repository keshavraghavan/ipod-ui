/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Custom home screen (fork addition).
 *
 * A hand-drawn launcher that replaces the stock root list as the first
 * screen after boot. It bypasses the theme/skin engine and draws directly
 * through the screen API, then hands navigation back to root_menu.c by
 * returning a GO_TO_* value.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ****************************************************************************/
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "kernel.h"
#include "action.h"
#include "audio.h"
#include "metadata.h"
#include "font.h"
#include "lcd.h"
#include "misc.h"
#include "powermgmt.h"
#include "timefuncs.h"
#include "settings.h"
#include "screen_access.h"
#include "viewport.h"
#include "root_menu.h"
#include "home_screen.h"

/* ---- look & feel ------------------------------------------------------ */
#define HEADER_H      28
#define FOOTER_H      40
#define ROW_PAD       10
#define SIDE_PAD      12

#define COL_BG        LCD_RGBPACK(0xF4, 0xF1, 0xEA)  /* warm paper     */
#define COL_INK       LCD_RGBPACK(0x1C, 0x1C, 0x1C)  /* body text      */
#define COL_MUTED     LCD_RGBPACK(0x8A, 0x86, 0x7E)  /* chevrons, meta */
#define COL_RULE      LCD_RGBPACK(0xDD, 0xD8, 0xCE)  /* row dividers   */
#define COL_HEAD_TOP  LCD_RGBPACK(0x26, 0x26, 0x26)
#define COL_HEAD_BOT  LCD_RGBPACK(0x10, 0x10, 0x10)
#define COL_SEL_TOP   LCD_RGBPACK(0x14, 0x8F, 0x86)  /* teal highlight */
#define COL_SEL_BOT   LCD_RGBPACK(0x0B, 0x5E, 0x58)
#define COL_ACCENT    LCD_RGBPACK(0xF2, 0x8C, 0x28)  /* orange accent  */
#define COL_WHITE     LCD_RGBPACK(0xFF, 0xFF, 0xFF)

/* ---- menu model ------------------------------------------------------- */
struct home_item {
    const char *label;   /* NULL => label computed at draw time */
    int screen;          /* GO_TO_* value handed back to root_menu */
};

static const struct home_item home_items[] = {
#ifdef HAVE_TAGCACHE
    { "Music",        GO_TO_DBBROWSER },
#endif
    { "Files",        GO_TO_FILEBROWSER },
    { "Playlists",    GO_TO_PLAYLISTS_SCREEN },
    { NULL,           GO_TO_WPS },          /* Now Playing / Resume */
    { "Settings",     GO_TO_MAINMENU },
    { "Rockbox Menu", GO_TO_ROOT },
};
#define HOME_ITEM_COUNT ((int)(sizeof(home_items) / sizeof(home_items[0])))

static int selected = 0;   /* remembered across visits */

static bool is_playing(void)
{
    return (audio_status() & AUDIO_STATUS_PLAY) != 0;
}

static const char *item_label(int i)
{
    if (home_items[i].label)
        return home_items[i].label;
    return is_playing() ? "Now Playing" : "Resume";
}

/* ---- drawing ---------------------------------------------------------- */
static void draw_chevron(struct screen *d, int x, int cy, int h)
{
    int half = h / 2;
    d->drawline(x, cy - half, x + half, cy);
    d->drawline(x + half, cy, x, cy + half);
    d->drawline(x + 1, cy - half, x + half + 1, cy);   /* 2px stroke */
    d->drawline(x + half + 1, cy, x + 1, cy + half);
}

static void draw_header(struct screen *d, int font_h)
{
    char buf[24];
    int w, h, x;
    int ty = (HEADER_H - font_h) / 2;

    d->gradient_fillrect(0, 0, d->getwidth(), HEADER_H,
                         COL_HEAD_TOP, COL_HEAD_BOT);
    d->set_drawmode(DRMODE_FG);

    /* orange accent pip + title */
    d->set_foreground(COL_ACCENT);
    d->fillrect(SIDE_PAD, HEADER_H / 2 - 3, 6, 6);
    d->set_foreground(COL_WHITE);
    d->putsxy(SIDE_PAD + 12, ty, "Home");

    /* clock, right-aligned */
    x = d->getwidth() - SIDE_PAD;
    struct tm *tm = get_time();
    if (valid_time(tm))
    {
        int hour = tm->tm_hour;
        if (global_settings.timeformat == 1)
        {
            hour %= 12;
            if (hour == 0)
                hour = 12;
        }
        snprintf(buf, sizeof(buf), "%d:%02d", hour, tm->tm_min);
        d->getstringsize(buf, &w, &h);
        x -= w;
        d->putsxy(x, ty, buf);
        x -= 10;
    }

    /* battery: outline + fill + nub */
    int level = battery_level();
    int bw = 22, bh = 10, by = (HEADER_H - bh) / 2;
    x -= bw + 3;
    d->set_foreground(COL_MUTED);
    d->drawrect(x, by, bw, bh);
    d->fillrect(x + bw, by + 3, 2, bh - 6);
    if (level >= 0)
    {
        d->set_foreground(level <= 15 ? COL_ACCENT : COL_WHITE);
        d->fillrect(x + 2, by + 2, (bw - 4) * level / 100, bh - 4);
    }
    d->set_drawmode(DRMODE_SOLID);
}

static void draw_footer(struct screen *d, int y, int font_h)
{
    struct mp3entry *id3 = audio_current_track();
    int width = d->getwidth();

    d->set_foreground(COL_RULE);
    d->hline(0, width - 1, y);

    if (!is_playing() || !id3)
        return;

    /* progress rail */
    int rail_y = y + 6;
    int rail_w = width - 2 * SIDE_PAD;
    d->fillrect(SIDE_PAD, rail_y, rail_w, 3);
    if (id3->length > 0)
    {
        d->set_foreground(COL_ACCENT);
        d->fillrect(SIDE_PAD, rail_y,
                    (int)((long long)rail_w * id3->elapsed / id3->length), 3);
    }

    /* "Title · Artist" */
    char line[100];
    const char *title = id3->title ? id3->title : id3->path;
    if (id3->artist)
        snprintf(line, sizeof(line), "%.60s  \xC2\xB7  %.30s", title, id3->artist);
    else
        snprintf(line, sizeof(line), "%.90s", title);

    d->set_drawmode(DRMODE_FG);
    d->set_foreground(COL_INK);
    d->putsxy(SIDE_PAD, rail_y + 3 + (FOOTER_H - 9 - font_h) / 2, line);
    d->set_drawmode(DRMODE_SOLID);
}

static void draw(struct screen *d, struct viewport *vp, int *top_row)
{
    int font_h = font_get(vp->font)->height;
    int row_h = font_h + ROW_PAD;
    int width = d->getwidth();
    int list_y = HEADER_H;
    int list_h = d->getheight() - HEADER_H - (is_playing() ? FOOTER_H : 0);
    int rows = list_h / row_h;

    /* keep the selection on screen */
    if (selected < *top_row)
        *top_row = selected;
    else if (selected >= *top_row + rows)
        *top_row = selected - rows + 1;

    d->set_background(COL_BG);
    d->clear_viewport();
    draw_header(d, font_h);

    for (int r = 0; r < rows && *top_row + r < HOME_ITEM_COUNT; r++)
    {
        int i = *top_row + r;
        int y = list_y + r * row_h;
        int ty = y + (row_h - font_h) / 2;
        bool sel = (i == selected);

        if (sel)
            d->gradient_fillrect(0, y, width, row_h, COL_SEL_TOP, COL_SEL_BOT);
        else
        {
            d->set_foreground(COL_RULE);
            d->hline(SIDE_PAD, width - 1, y + row_h - 1);
        }

        d->set_drawmode(DRMODE_FG);
        d->set_foreground(sel ? COL_WHITE : COL_INK);
        d->putsxy(SIDE_PAD, ty, item_label(i));
        d->set_drawmode(DRMODE_SOLID);

        d->set_foreground(sel ? COL_WHITE : COL_MUTED);
        draw_chevron(d, width - SIDE_PAD - 6, y + row_h / 2, 8);
    }

    if (is_playing())
        draw_footer(d, d->getheight() - FOOTER_H, font_h);

    d->update();
}

/* ---- entry point ------------------------------------------------------ */
int home_screen(void *param)
{
    (void)param;
    struct screen *d = &screens[SCREEN_MAIN];
    struct viewport vp;
    int top_row = 0;
    int ret = GO_TO_ROOT;

    viewportmanager_theme_enable(SCREEN_MAIN, false, &vp);
    struct viewport *last_vp = d->set_viewport(&vp);
    unsigned old_fg = d->get_foreground();
    unsigned old_bg = d->get_background();

    if (selected >= HOME_ITEM_COUNT)
        selected = 0;

    while (true)
    {
        draw(d, &vp, &top_row);

        /* half-second timeout keeps the clock and progress rail live */
        int action = get_action(CONTEXT_STD, HZ / 2);
        switch (action)
        {
            case ACTION_STD_NEXT:
            case ACTION_STD_NEXTREPEAT:
                if (selected < HOME_ITEM_COUNT - 1)
                    selected++;
                break;

            case ACTION_STD_PREV:
            case ACTION_STD_PREVREPEAT:
                if (selected > 0)
                    selected--;
                break;

            case ACTION_STD_OK:
                ret = home_items[selected].screen;
                goto out;

            case ACTION_STD_CANCEL:
            case ACTION_STD_MENU:
                ret = GO_TO_ROOT;
                goto out;

            case ACTION_NONE:
                break;

            default:
                if (default_event_handler(action) == SYS_USB_CONNECTED)
                {
                    ret = GO_TO_ROOT;
                    goto out;
                }
                break;
        }
    }

out:
    d->set_foreground(old_fg);
    d->set_background(old_bg);
    d->set_viewport(last_vp);
    viewportmanager_theme_undo(SCREEN_MAIN, false);
    return ret;
}
