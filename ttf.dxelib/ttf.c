/*
MIT License

Copyright (c) 2019-2021 Andre Seidelt <superilu@yahoo.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <mujs.h>
#include <stdio.h>
#include <schrift.h>

#include "DOjS.h"
#include "bitmap.h"
#include "ttf.h"

void init_TTF(js_State *J);

static void TTF_Finalize(js_State *J, void *data) {
  SFT *sft = (SFT *)data;
  sft_freefont(sft->font);
  sft->font = NULL;
}

/**
 * @brief load a TTF font.
 *
 * var ttf = new TTF(filename, xScale, yScale);
 *
 * @param J the JS context.
 */
static void new_TTF(js_State *J) {
    NEW_OBJECT_PREP(J);

    const char *fname = js_tostring(J, 1);
    double xScale = js_tonumber(J, 2);
    double yScale = js_tonumber(J, 3);

    SFT *sft = calloc(sizeof(SFT), 1);

    sft->xScale = xScale;
    sft->yScale = yScale;
    sft->flags  = SFT_DOWNWARD_Y;

    sft->font = sft_loadfile(fname);

    if (sft->font == NULL) {
        free(sft);
        js_error(J, "Can't load ttf file '%s'", fname);
        return;
    }

    js_currentfunction(J);
    js_getproperty(J, -1, "prototype");
    js_newuserdata(J, TAG_TTF, sft, TTF_Finalize);

    js_pushstring(J, fname);
    js_defproperty(J, -2, "filename", JS_READONLY | JS_DONTCONF);
}

/**
 * @brief Close a font.
 *
 * ttf.Close();
 *
 * @param J the JS context.
 */
static void TTF_Close(js_State *J) {
    SFT *sft = js_touserdata(J, 0, TAG_TTF);
    sft_freefont(sft->font);
    sft->font = NULL;
}

/**
 * @brief get glyph metrics.
 *
 * var {advanceWidth,leftSideBearing,yOffset,minWidth,minHeight} =
 *   ttf.GetMetrics(0x2615);
 *
 * @param J the JS context.
 */
static void TTF_GetMetrics(js_State *J) {
    SFT *sft = (SFT *)js_touserdata(J, 0, TAG_TTF);
    long codepoint = js_tonumber(J, 1);

    SFT_Glyph gid;
    if (sft_lookup(sft, codepoint, &gid) < 0) {
        js_error(J, "no glyph found: %lx", codepoint);
        return;
    }

    SFT_GMetrics mtx;
    if (sft_gmetrics(sft, gid, &mtx) < 0) {
        js_error(J, "could not get metrics for %lx", codepoint);
        return;
    }

    js_newobject(J);
    {
        js_pushnumber(J, mtx.advanceWidth);
        js_defproperty(J, -2, "advanceWidth", JS_READONLY | JS_DONTCONF);

        js_pushnumber(J, mtx.leftSideBearing);
        js_defproperty(J, -2, "leftSideBearing", JS_READONLY | JS_DONTCONF);

        js_pushnumber(J, mtx.minWidth);
        js_defproperty(J, -2, "minWidth", JS_READONLY | JS_DONTCONF);

        js_pushnumber(J, mtx.minHeight);
        js_defproperty(J, -2, "minHeight", JS_READONLY | JS_DONTCONF);

        js_pushnumber(J, mtx.yOffset);
        js_defproperty(J, -2, "yOffset", JS_READONLY | JS_DONTCONF);
    }
    /* Object is returned. */
}

/**
 * @brief get glyph bitmap.
 *
 * var bitmap = new BITMAP(16, 16);
 * var color = 0xff00ff00; // Render green
 * ttf.RenderGlyph(x, y, 0x2615, color, bitmap);
 *
 * @param J the JS context.
 */
static void TTF_RenderGlyph(js_State *J) {
    SFT *sft = (SFT *)js_touserdata(J, 0, TAG_TTF);
    int offset_x = js_tonumber(J, 1);
    int offset_y = js_tonumber(J, 2);
    long codepoint = js_tonumber(J, 3);
    uint32_t color = js_tonumber(J, 4);

    BITMAP *bm = (BITMAP *)js_touserdata(J, 5, TAG_BITMAP);

    /* Cribbed from demo.c */
    SFT_Glyph gid;
    if (sft_lookup(sft, codepoint, &gid) < 0) {
        js_error(J, "no glyph found: %lx", codepoint);
        return;
    }

    SFT_GMetrics mtx;
    if (sft_gmetrics(sft, gid, &mtx) < 0) {
        js_error(J, "could not get metrics for codepoint %lx", codepoint);
        return;
    }

    SFT_Image img = {
        .width = (mtx.minWidth + 3) & ~3,
        .height = mtx.minHeight
    };

    char *pixels = malloc(img.width * img.height);
    if (!pixels) {
        JS_ENOMEM(J);
        return;
    }

    img.pixels = pixels;

    int bpp = bitmap_color_depth(bm);
    int bytes_per_pixel = bpp / 8;

    if (sft_render(sft, gid, img) < 0) {
        js_error(J, "could not render codepoint %lx", codepoint);
        free(pixels);
        return;
    }

    int min_j = 0, max_j = img.width;
    if (offset_x < 0) {
        min_j = -offset_x;
    }
    if (offset_x > bm->w - img.width) {
        max_j = bm->w - img.width;
    }

    for (int i = 0; i < img.height; i++) {
        int bm_line = offset_y + i;
        if (bm_line < 0 || bm_line >= bm->h) {
            continue;
        }

        for (int j = min_j; j < max_j; j++) {
            int tj = j * bytes_per_pixel;
            if (bpp == 8) {
                if (pixels[i * img.width + j]) {
                    bm->line[i][tj] = color;
                } else {
                    bm->line[i][tj] = 0;
                }
            } else {
                if (pixels[i * img.width + j]) {
                    bm->line[i][tj] = color;
                    bm->line[i][tj + 1] = color >> 8;
                    bm->line[i][tj + 2] = color >> 16;
                    if (bpp > 24) {
                        bm->line[i][tj + 3] = color >> 24;
                    }
                } else {
                    memset(&bm->line[i][tj], 0, bytes_per_pixel);
                }
            }
        }
    }

    free(pixels);
}

/**
 * @brief initialize TTF loading.
 *
 * @param J VM state.
 */
void init_TTF(js_State *J) {
    LOGF("%s\n", __PRETTY_FUNCTION__);

    js_newobject(J);
    {
        NPROTDEF(J, TTF, Close, 0);
        NPROTDEF(J, TTF, GetMetrics, 1);
        NPROTDEF(J, TTF, RenderGlyph, 5);
    }
    CTORDEF(J, new_TTF, TAG_TTF, 2);
}
