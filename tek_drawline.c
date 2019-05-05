/*  tek_drawline.c

Copyright (c) 2018, Ian Schofield

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the author shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the author.

*/


#include <math.h>
#include "tek_video.h"

typedef int                 BOOL;

#ifndef FALSE
#define FALSE               0
#endif

#ifndef TRUE
#define TRUE                1
#endif

static uint32 lptn[]={0xffffffff, 0x77777777, 0xfff00000, 0xff00ff, 0xff00ff};      // Line patterns
static uint32 ptn = 0xffffffff, setptn = 0xffffffff;                              // Pattern mask, default SOLID
static int pbuff[4096];                                                       // Pattern bufffer (Right!)
static int rtcnt = 0;                                                             // Rotate counter for reload
static int hldx, hldy;
extern unsigned char *pixels;                                                     // See tek_main.c
extern int pxptn[];                                                               // Blurred single pixel. See tek_display.c

#ifdef _WIN32                                                                     // WIN32 does not define these functions.

float fmaxf(float a, float b)
{
    if (a>b)
        return a;
    return b;
}

float fminf(float a, float b)
{
    if (a<b)
        return a;
    return b;
}

#endif

/*  The following code fragments are from Milo Yip's implementation of an anti aliased
line drawing procedure as outlined at https://github.com/miloyip/line

Copyright (C) 2017 Milo Yip. All rights reserved.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
* Neither the name of pngout nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


float capsuleSDF(float px, float py, float ax, float ay, float bx, float by, float r) {
    float pax, pay, bax, bay, h, dx, dy;
    pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
    h = fmaxf(fminf((pax * bax + pay * bay) / (bax * bax + bay * bay), 1.0f), 0.0f);
    dx = pax - bax * h, dy = pay - bay * h;
    return sqrtf(dx * dx + dy * dy) - r;
}

void alphablend(int x, int y, float alpha, float r, float g, float b) {
    unsigned char* p;

    if (y >= WINDOW_HEIGHT || y < 0 || x >= WINDOW_WIDTH || x < 0)
        return;

    p = pixels + (y * WINDOW_WIDTH + x) * 4;
    if (!wrthru || p[1] == BACKGROUND) {
        p[1] = (unsigned char)(p[1] * (1 - alpha) + g * alpha * 255);
        if (wrthru)
            p[3]=1;
        else {
            p[0] = (unsigned char)(p[0] * (1 - alpha) + r * alpha * 255);
            p[2] = (unsigned char)(p[2] * (1 - alpha) + b * alpha * 255);
            p[3] = 0;
        }
    }
}

void lineSDFAABB(float ax, float ay, float bx, float by, float r) {
    int x0, x1, y0, y1, x, y, xd, yd;
    int ptl,ptx;

    xd = (int)ax;
    yd = (int)ay;
    x0 = (int)floorf(fminf(ax, bx) - r);
    x1 = (int) ceilf(fmaxf(ax, bx) + r);
    y0 = (int)floorf(fminf(ay, by) - r);
    y1 = (int) ceilf(fmaxf(ay, by) + r);

    ptl=(int)sqrt((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0));
    for (x=0; x<ptl; x++) {
        pbuff[x]=ptn & 1;
        ptn = ptn >> 1;
        if (++rtcnt == 32) {
            rtcnt = 0;
            ptn = setptn;
        }
    }

    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            ptx=(int)sqrt((x-xd)*(x-xd) + (y-yd)*(y-yd));
            //if (pbuff[ptx] | pbuff[ptx+1] | pbuff[ptx-1])
            if (pbuff[ptx])
                alphablend(x, y, fmaxf(fminf(0.5f - capsuleSDF((float)x, (float)y, ax, ay, bx, by, r), 1.0f), 0.0f), 0.9f, 0.8f, 0.9f);
        }
}

void NewLinesAlpha(int x1,int y1,int x2,int y2)
{
    lineSDFAABB((float)x1*1.5f, (float)y1*1.5f, (float)x2*1.5f, (float)y2*1.5f, 0.9f);
}

void setlinetype(enum LineType ltype)
{
    ptn = setptn = lptn[ltype];
    rtcnt = 0;
}

void PlotPoint(int x, int y)
{
    int k,l,tm;
    unsigned char *p;

    if (y >= REAL_HEIGHT || y < 0)
        return;
    
    x=x*1.5;
    y=WINDOW_HEIGHT-y*1.5;
    for (k=0; k<3; k++)
        for (l=0; l<3; l++)
            if (y+l < WINDOW_HEIGHT) {
                p = pixels + (int)((y + l) * WINDOW_WIDTH + x + k) * 4;
                p[0]=0xff;
                tm = pxptn[k*3+l] + (int)(p[1]>>2);
                if (tm > 0xfc)
                    tm=0xfc;
                p[1]=tm;
                p[2]=0xff;
            }
}

void DrawGIN(int x1,int y1)
{
    int dx, dy;

    //Erase last GIN cursor if there is one on screen
    if (clrflag) {
        for (dx=0; dx<WINDOW_WIDTH; dx++)
            if (vid_getpixel(dx, hldy) & 1)
                vid_setpixel(dx,hldy,7,(BACKGROUND << 8));
        for (dy=0; dy<WINDOW_HEIGHT; dy++)
            if (vid_getpixel(hldx, dy) & 1)
                vid_setpixel(hldx,dy,7,(BACKGROUND << 8));
    }
    if (GINon == 0) {                   // GIN mode ended do not redraw cursor
        clrflag = GINon = 0;
        return;
    }
    for (dx=0; dx<WINDOW_WIDTH; dx++)
        if ((vid_getpixel(dx, y1) & 0xff00) == (BACKGROUND << 8))
            vid_setpixel(dx,y1,7,0xe001);
    for (dy=0; dy<WINDOW_HEIGHT; dy++)
        if ((vid_getpixel(x1,dy) & 0xff00) == (BACKGROUND << 8))
            vid_setpixel(x1,dy,7,0xe001);
    hldx = x1;
    hldy = y1;
    clrflag++;
}
