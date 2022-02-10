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
Include('p5');
LoadLibrary('TTF');

/*
** This function is called once when the script is started.
*/
function setup() {
    pink = color(241, 66, 244); // define the color pink
    bitmap = new Bitmap(100, 100);
    font = new TTF('FIRAGO.TTF', 32, 32);
    glyph = 128;
}

/*
** This function is repeatedly until ESC is pressed or Stop() is called.
*/
function draw() {
    background(0);
    stroke(pink);
    fill(pink);

    draw_at_y = 10;

    for (i = 0; i < 8; i++) {
        max_height = 0;
        draw_at_x = 10;

        for (j = 0; j < 30; j++) {
            glyph = 128 + i * 30 + j;
            metrics = font.GetMetrics(glyph);
            font.RenderGlyph(0, 0, glyph, 0xffffffff, bitmap);

            bitmap.DrawAdvanced(
                0, 0,
                metrics.minWidth, metrics.minHeight,
                draw_at_x, draw_at_y + metrics.yOffset,
                metrics.minWidth, metrics.minHeight
            );

            draw_at_x += metrics.minWidth;
            max_height = Math.max(max_height, metrics.minHeight - metrics.yOffset);
        }

        draw_at_y += max_height;
    }

    stroke(EGA.LIGHT_BLUE);
    fill(EGA.LIGHT_BLUE);
    text("rate=" + getFrameRate(), 10, 10);
}
