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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <conio.h>
#include <keys.h>
#include <pc.h>

#include "DOjS.h"
#include "dialog.h"
#include "edi_render.h"
#include "edit.h"
#include "zipfile.h"
#include "util.h"

/************
** defines **
************/
#define DIA_CONFIRM "<[y] or [n]>"        //!< text for confirm dialog
#define DIA_MESSAGE "<Press any key>"     //!< text for message dialog
#define DIA_TEXT "<UP/DOWN, ESC or DEL>"  //!< text for file dialog

/***********
** macros **
***********/
//! find start of next line in text buffer
#define DIA_LINE_DOWN()                            \
    {                                              \
        while (txt[start] && txt[start] != '\n') { \
            start++;                               \
        }                                          \
        if (txt[start]) {                          \
            start++;                               \
        }                                          \
    }

//! find start of previous line in text buffer
#define DIA_LINE_UP()                         \
    {                                         \
        if (start) {                          \
            start--;                          \
        }                                     \
        while (start && txt[start] != '\n') { \
            start--;                          \
        }                                     \
        if (start) {                          \
            start--;                          \
        }                                     \
        while (start && txt[start] != '\n') { \
            start--;                          \
        }                                     \
        if (txt[start] == '\n') {             \
            start++;                          \
        }                                     \
    }

/***********************
** exported functions **
***********************/
/**
 * @brief let the user enter a text.
 *
 * @param edi current editor.
 * @param buffer the buffer to store the text in.
 * @param allowed a string with allowed characters or NULL to allow everything.
 * @param msg message to show on lower border of dialog.
 *
 * @return true if the user used ENTER to exit the dialog.
 * @return false if the user used ESC to exit the dialog.
 */
bool dia_ask_text(edi_t* edi, char buffer[DIA_ASK_SIZE], char* allowed, char* msg) {
    edi->last_top = NULL;

    int w = edi->scr.screenwidth - 4;
    int h = 3;

    int startX = (edi->scr.screenwidth - w) / 2;
    int startY = (edi->scr.screenheight - h) / 2;
    while (true) {
        int endpos = 0;

        // upper border
        edi_textbackground(BLUE);
        edi_textcolor(WHITE);
        edi_gotoxy(edi, startX, startY);
        edi_putch(edi, EDI_UL_LINE);
        for (int x = 1; x < w + 1; x++) {
            edi_putch(edi, EDI_H_LINE);
        }
        edi_putch(edi, EDI_UR_LINE);

        edi_gotoxy(edi, startX, startY + 1);
        edi_putch(edi, EDI_V_LINE);
        edi_putch(edi, ' ');

        edi_textbackground(BLACK);
        edi_textcolor(WHITE);
        int pos = 0;
        while (pos < w - 2) {
            if (buffer[endpos]) {
                edi_putch(edi, buffer[endpos]);
                endpos++;
            } else {
                edi_putch(edi, ' ');
            }
            pos++;
        }

        edi_textbackground(BLUE);
        edi_textcolor(WHITE);
        edi_putch(edi, ' ');
        edi_putch(edi, EDI_V_LINE);

        // lower border
        edi_gotoxy(edi, startX, startY + 2);
        edi_putch(edi, EDI_LL_LINE);
        int x = 1;
        while (x < (w - strlen(msg)) / 2) {
            edi_putch(edi, EDI_H_LINE);
            x++;
        }
        edi_cputs(edi, msg);
        x += strlen(msg);
        while (x < w + 1) {
            edi_putch(edi, EDI_H_LINE);
            x++;
        }
        edi_putch(edi, EDI_LR_LINE);

        gotoxy(strlen(buffer) + startX + 2, startY + 1);
        edi_refresh_screen(edi);
        int ch = getxkey();
        if (ch == K_BackSpace) {
            if (endpos) {
                endpos--;
                buffer[endpos] = 0;
            }
        } else if (ch == K_Escape) {
            return false;
        } else if (ch == K_Return) {
            return true;
        } else {
            bool isin = false;
            if (allowed) {
                for (int i = 0; i < strlen(allowed); i++) {
                    if (ch == allowed[i]) {
                        isin = true;
                        break;
                    }
                }
            } else {
                isin = true;
            }
            if (isin) {
                if (endpos < w) {
                    buffer[endpos] = ch;
                    endpos++;
                    buffer[endpos] = 0;
                }
            }
        }
    }
}

/**
 * @brief show a single line message on screen. returns after any keypress.
 *
 * @param edi current editor.
 * @param txt the message to display.
 */
void dia_show_message(edi_t* edi, const char* txt) {
    edi->last_top = NULL;

    int w = 0;
    int h = 3;

    // calculate width and height by checking all lines
    int line_length = 0;
    int pos = 0;
    while (txt[pos]) {
        if (txt[pos] == '\n') {
            w = max(4 + line_length, w);
            line_length = 0;
            h++;
        } else if (txt[pos] == '\t') {
            line_length += 2;
        } else {
            line_length++;
        }
        pos++;
    }
    w = max(4 + line_length, w);
    w = min(w, edi->width - 2);
    h = min(h, edi->height);

    // set minimal width because of key-press message
    if (w < strlen(DIA_MESSAGE) + 4) {
        w = strlen(DIA_MESSAGE) + 4;
    }

    int startX = (edi->scr.screenwidth - w) / 2;
    if (startX < 2) {
        startX = 2;
    }

    int startY = (edi->scr.screenheight - h) / 2;
    if (startY < 2) {
        startY = 2;
    }

    // upper border
    edi_textbackground(RED);
    edi_textcolor(YELLOW);
    edi_gotoxy(edi, startX, startY);
    edi_putch(edi, EDI_UL_LINE);
    for (int x = 1; x < w - 1; x++) {
        edi_putch(edi, EDI_H_LINE);
    }
    edi_putch(edi, EDI_UR_LINE);

    int current_line = startY + 1;
    edi_gotoxy(edi, startX, current_line);
    edi_putch(edi, EDI_V_LINE);
    edi_putch(edi, ' ');
    int x = 2;
    pos = 0;
    while (txt[pos] && (current_line <= startY + h)) {
        if ((txt[pos] == '\n') || (x > w - 2)) {
            while (x < w - 1) {
                edi_putch(edi, ' ');
                x++;
            }
            edi_putch(edi, EDI_V_LINE);
            current_line++;
            x = 2;
            edi_gotoxy(edi, startX, current_line);
            EDIF("goto(%d, %d)\n", startX, current_line)
            edi_putch(edi, EDI_V_LINE);
            edi_putch(edi, ' ');
        } else if (txt[pos] == '\t') {
            edi_putch(edi, ' ');
            edi_putch(edi, ' ');
            x += 2;
        } else {
            edi_putch(edi, txt[pos]);
            x++;
        }
        pos++;
    }
    while (x < w - 1) {
        edi_putch(edi, ' ');
        x++;
    }
    edi_putch(edi, EDI_V_LINE);

    // lower border
    edi_gotoxy(edi, startX, startY + h - 1);
    edi_putch(edi, EDI_LL_LINE);
    x = 1;
    while (x < (w - strlen(DIA_MESSAGE)) / 2) {
        edi_putch(edi, EDI_H_LINE);
        x++;
    }
    edi_cputs(edi, DIA_MESSAGE);
    x += strlen(DIA_MESSAGE);
    while (x < w - 1) {
        edi_putch(edi, EDI_H_LINE);
        x++;
    }
    edi_putch(edi, EDI_LR_LINE);
    edi_refresh_screen(edi);
    getxkey();
}

/**
 * @brief show a single line yes/no question on screen. returns after Y/N keypress.
 *
 * @param edi current editor.
 * @param txt the question to display.
 */
bool dia_show_confirm(edi_t* edi, const char* txt) {
    edi->last_top = NULL;

    int w = strlen(txt) + 4;
    int h = 3;

    int startX = (edi->scr.screenwidth - w) / 2;
    int startY = (edi->scr.screenheight - h) / 2;

    // upper border
    edi_textbackground(BLUE);
    edi_textcolor(WHITE);
    edi_gotoxy(edi, startX, startY);
    edi_putch(edi, EDI_UL_LINE);
    for (int x = 1; x < w - 1; x++) {
        edi_putch(edi, EDI_H_LINE);
    }
    edi_putch(edi, EDI_UR_LINE);

    edi_gotoxy(edi, startX, startY + 1);
    edi_putch(edi, EDI_V_LINE);
    edi_putch(edi, ' ');
    edi_cputs(edi, txt);
    edi_putch(edi, ' ');
    edi_putch(edi, EDI_V_LINE);

    // lower border
    edi_gotoxy(edi, startX, startY + 2);
    edi_putch(edi, EDI_LL_LINE);
    int x = 1;
    while (x < (w - strlen(DIA_CONFIRM)) / 2) {
        edi_putch(edi, EDI_H_LINE);
        x++;
    }
    edi_cputs(edi, DIA_CONFIRM);
    x += strlen(DIA_CONFIRM);
    while (x < w - 1) {
        edi_putch(edi, EDI_H_LINE);
        x++;
    }
    edi_putch(edi, EDI_LR_LINE);
    edi_refresh_screen(edi);
    while (true) {
        int ch = getxkey();
        if (ch == 'y' || ch == 'Y') {
            return true;
        } else if (ch == 'n' || ch == 'N' || ch == K_Escape) {
            return false;
        }
    }
}

/**
 * @brief load a text file and display it on screen.
 *
 * @param edi current editor.
 * @param fname name of the file.
 * @param pos (last) shown position.
 * @param deletable TRUE if the DEL-key shall delete the file after displaying.
 * @param ctx help context or NULL if none.
 */
void dia_show_file(edi_t* edi, const char* fname, int* pos, bool deletable, char* ctx) {
    edi->last_top = NULL;

    char* file_data;
    size_t size;
    if (!read_zipfile1(fname, (void**)&file_data, &size)) {
        if (!ut_read_file(fname, (void**)&file_data, &size)) {
            char buff[1024];
            snprintf(buff, sizeof(buff), "cannot open file '%s': %s", fname, strerror(errno));
            dia_show_message(edi, buff);
            return;
        }
    }

    // check if pos is in range for file
    if (pos) {
        // first try context search, if that fails try last position
        if (ctx) {
            int ctx_len = strlen(ctx);
            int find_pos = 0;
            while (find_pos + 5 + ctx_len < size) {
                if (file_data[find_pos + 0] == '\n' && file_data[find_pos + 1] == '#' && file_data[find_pos + 2] == '#' && file_data[find_pos + 3] == '#' &&
                    file_data[find_pos + 4] == ' ' && memcmp(ctx, &file_data[find_pos + 5], ctx_len) == 0) {
                    *pos = find_pos;
                    break;
                }
                find_pos++;
            }
        }

        if (*pos > size) {
            *pos = 0;
        }
    }

    bool del = dia_show_text(edi, file_data, pos);
    free(file_data);
    if (deletable && del) {
        FILE* file = fopen(fname, "w");
        fflush(file);
        fclose(file);
    }
}

/**
 * @brief show multi-line text on screen.
 *
 * @param edi current editor.
 * @param txt pointer to null terminated text.
 * @param pos (last) shown position.
 *
 * @return true the user exited via DEL key.
 * @return false the user exited via any other key.
 */
bool dia_show_text(edi_t* edi, const char* txt, int* pos) {
    edi->last_top = NULL;

    int start = 0;
    int lastStart = -1;
    bool end = false;

    if (pos && *pos < strlen(txt)) {
        start = *pos;
    }

    while (true) {
        int color = WHITE;
        int w = edi->scr.screenwidth - 2;
        int h = edi->height;
        if (start != lastStart) {
            int startX = 2;
            int startY = 2;

            // upper border
            edi_textbackground(CYAN);
            edi_textcolor(WHITE);
            edi_gotoxy(edi, startX, startY);
            edi_putch(edi, EDI_UL_LINE);
            for (int x = 1; x < w - 2; x++) {
                edi_putch(edi, EDI_H_LINE);
            }
            if (start == 0) {
                edi_putch(edi, EDI_H_LINE);
            } else {
                edi_putch(edi, '^');
            }
            edi_putch(edi, EDI_UR_LINE);

            int idx = start;
            int y = startY + 1;
            if (txt[idx] == '#' && txt[idx + 1] == '#') {  // this is possible because idx+1 is always either a character or the terminating '\0'
                color = RED;
            } else if (txt[idx] == '#') {
                color = YELLOW;
            }
            while (y < h) {
                edi_textcolor(WHITE);
                edi_gotoxy(edi, startX, y);
                edi_putch(edi, EDI_V_LINE);
                edi_putch(edi, ' ');
                edi_textcolor(color);
                while (true) {  // line loop
                    if (!txt[idx]) {
                        end = true;
                        break;
                    } else if (txt[idx] == '\r') {
                        // ignored
                        idx++;
                    } else if (txt[idx] == '\n') {
                        // newline
                        color = WHITE;
                        idx++;
                        // this is possible because idx+1 is always either a character or the terminating '\0'
                        if (txt[idx] == '#' && txt[idx + 1] == '#' && txt[idx + 2] == '#') {
                            color = MAGENTA;
                        } else if (txt[idx] == '#' && txt[idx + 1] == '#') {
                            color = RED;
                        } else if (txt[idx] == '#') {
                            color = YELLOW;
                        }
                        break;
                    } else if (!iscntrl(txt[idx])) {
                        // print char
                        end = false;
                        edi_putch(edi, txt[idx]);
                        idx++;
                    } else {
                        idx++;
                    }
                    if (edi_wherex(edi) > w - 2) {  // line wrap
                        break;
                    }
                }

                while (edi_wherex(edi) <= w) {  // fill line with spaces and append |
                    edi_putch(edi, ' ');
                }
                edi_textcolor(WHITE);
                edi_putch(edi, EDI_V_LINE);
                y++;
            }

            // lower border
            edi_gotoxy(edi, startX, y);
            edi_putch(edi, EDI_LL_LINE);
            int x = 1;
            while (x < (w - strlen(DIA_TEXT)) / 2) {
                edi_putch(edi, EDI_H_LINE);
                x++;
            }
            edi_cputs(edi, DIA_TEXT);
            x += strlen(DIA_TEXT);
            while (x < w - 2) {
                edi_putch(edi, EDI_H_LINE);
                x++;
            }
            if (end) {
                edi_putch(edi, EDI_H_LINE);
            } else {
                edi_putch(edi, 'v');
            }
            edi_putch(edi, EDI_LR_LINE);
            edi_gotoxy(edi, 1, 1);

            lastStart = start;
            edi_refresh_screen(edi);
        }

        int ch = getxkey();
        if (ch == K_Up || ch == K_EUp) {
            DIA_LINE_UP();
        } else if (!end && (ch == K_Down || ch == K_EDown)) {
            DIA_LINE_DOWN();
        } else if (ch == K_PageUp || ch == K_EPageUp) {
            int num = h - 4;
            while (num > 0) {
                DIA_LINE_UP();
                num--;
            }
        } else if (!end && (ch == K_PageDown || ch == K_EPageDown)) {
            int num = h - 4;
            while (num > 0) {
                DIA_LINE_DOWN();
                num--;
            }
        } else if (ch == K_Home || ch == K_EHome) {
            start = 0;
        } else if (ch == K_Escape || ch == K_F9 || ch == K_F1) {
            return false;
        } else if (ch == K_Delete || ch == K_EDelete) {
            return true;
        }
        if (pos) {
            *pos = start;
        }
    }
}
