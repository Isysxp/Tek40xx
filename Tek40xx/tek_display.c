/* tek_display.c TEKTRONIX 4010/4 display

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tek_video.h"
#include <ctype.h>
#include <math.h>
//#include <math.h>
#if defined (__linux__) || defined (VMS) || defined (__APPLE__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <time.h>
const struct timespec ts = {0, 500000L };
#endif

char tline[128];

#ifdef _WIN32
extern void usleep(unsigned int usec);
#endif


/* Generic to compute font bit on/off for this font */
#define FONT_BIT(c, column, row) \
    ((font_data[(c)+(column)] & 1<<(7-(row))) != 0)

int pxptn[]={0x6c,0x6c,0x6c,0x8c,0xfc,0x8c,0x6c,0x8c,0x6c};

void tek_draw();


int count = 0;
int cx1,cy1,cx2,cy2,rx,ry,clrflag;
static int lox,hix,loy,hiy,*hp,sflg,pplt;  // Data pattern for graphics
enum TekState tekstate,lststate;
char *p=tline;
extern enum vid_stat {STOPPED,WINDOW_OK,RUNNING,CLOSING,CLOSED} vid_init;
int xcol;       // Margin

int step_x,loc_x;     // Char cell size and newline step
int step_y,loc_y;
int save_x,save_y;
int sppz,ccntr;
FILE *getData;
FILE *putKeys;

extern double sin(double val);
extern int toclose, upflag, nostore;
#ifdef _WIN32
/* Windows sleep .. not a great way to do this! */
BOOLEAN nanosleep(LONGLONG us){
    /* Declarations */
    LARGE_INTEGER li,lo,lx;	/* Time defintion */

    lx.QuadPart=us;
    QueryPerformanceCounter(&lo);
    while (1) {
        QueryPerformanceCounter(&li);
        if (li.QuadPart - lo.QuadPart  > lx.QuadPart)
            break;
    }
    return TRUE;
}
#endif
int tek_get()                // Fetch char from the input stream or tline
{
    int bcnt;
    int rc;
    unsigned char tm;

    if (toclose)
        exit(0);            // Exit this thread
#if defined (__linux__) || defined (VMS) || defined (__APPLE__)
    nanosleep(&ts, 0);
#else
#ifdef _WIN32
    if (wrthru)
        nanosleep(500);        // See implementation above
    else
        nanosleep(2000);        // In write through mode, reduce delay sucha that the glyph
                                // is written much quicker. (Allowed! see manual)
                                // See vid_update_screen()
#endif
#endif
    if(*p)
        return *p++;
    else
    {
#ifdef WIN32
        ioctlsocket(tek_socket,FIONREAD,&bcnt);
#else
        ioctl(tek_socket,FIONREAD,&bcnt);
#endif
        if (bcnt)
        {
            rc=recv(tek_socket,&tm,1,0);
            if (rc == 1)
            {
                return tm & 0x7f;
            }
            else
                return -1;
        }
    }
    return -1;
}
/* Draw a character from the 5x7 font tabel at loc_x,loc_y (9 pixels per bit) */

void tek_draw_char(char ch)
{
    int x , y, i, j, ndx=(ch & 0x7f) * 5;
    int k,l;

    for (i=0,x=cx1*1.5; i<5; i++,x+=3)
        for (j=0,y=cy1*1.5; j<8; j++,y+=3)
            if (FONT_BIT(ndx,i,j))
                for (k=0; k<3; k++)
                    for (l=0; l<3; l++)
                        if (y+l < WINDOW_HEIGHT)
                            vid_setpixel(x+k, WINDOW_HEIGHT - y+l, 7, 0xff00fc | (pxptn[k*3+l] << 8));
}

/* Draw a character from the 7x9 font tabel at loc_x,loc_y */
/*
void tek_draw_char(char ch)
{
int x , y=cy1, i, j, ndx=(ch & 0x7f) * 9;
int pxls, k, l;

for (i=8; 0 < i; i--, y+=2) {
pxls = console_font_7x9[ndx+i];
for (j = 512,x = cx1; j > 0; j >>=1, x+=2)
if (pxls & j)
for (k=0; k<2; k++)
for (l=0; l<2; l++)
vid_setpixel(x+k, WINDOW_HEIGHT - y-l, 7, 0xffffff);
}
}
*/
void save_state(enum TekState state)
{
    lststate = tekstate;
    tekstate = state;
}

void tek_erase()
{
    vid_erase_win();          // Erase screen
    lo = 230;                  // Initial flash and fade to 230
    save_x = xcol = cx1 = 0;
    save_y = cy1 = REAL_HEIGHT - step_y;
    tekstate = ALPHA;
    sppz = pplt = 0;
    clrflag = 0;            // Screen cleared 
}


// This entry point is called by SDL on a thread

int main(int argc, char* argv[])        // Local initialisation
{
    int port = 23;

    step_x  = REAL_WIDTH / 80;
    step_y  = REAL_HEIGHT / 35;
    cx1 = xcol = 0;
    cy1 = REAL_HEIGHT - step_y;            /* home position in screen units (tekpoints) */
    lststate = tekstate = ALPHA;
    wrthru = GINon = 0;
    setlinetype(SOLID);
    hix=lox=hiy=loy=0;         // Initial state
    hp = &hiy;                 // Expect hiy first
    sflg = pplt = 0;           // No draw
    if (argc == 3)
        port=atoi(argv[2]);
    vid_open("TEKTRONIX 4014","TEKTRONIX 4014",WINDOW_WIDTH,WINDOW_HEIGHT,0);
    SDL_Delay(200);
    while (vid_init != RUNNING)
        SDL_Delay(10);          // Wait for window
    tek_erase();
    tek_socket = telnet(argv[1], port);

    while(1)                  // Main idle loop
        tek_draw();

    return 0;
}


void tek_newline()
{
    cy1 -= step_y;
    if (cy1 < 0)
    {
        cy1 = REAL_HEIGHT - step_y;
        if (xcol) xcol = 0;
        else xcol = REAL_WIDTH / 2;
    }
    cx1 = xcol;
}


void tek_draw()
{
    int ch,flg;

    while (1)
    {
        if ((ch = tek_get()) < 1)      // Ignore no data (-1) and nulls
            break;
        vid_setcursor(0);
        switch (tekstate)
        {
        case ALPHA:
            switch (ch)
            {
            case 0:
                break;
            case EOF:
                break;
            case 7:     // Beep!
                SDL_Delay(10);
                break;
            case 8:     // backspace
                cx1 -= step_x;
                if (cx1 < xcol) cx1 = xcol;
                break;
            case 9:     // tab stops at 8th char
                cx1 = ((cx1/step_x & ~7) + 8) * step_x;
                break;
            case 10:    // new line
                tek_newline();
                break;
            case 11:    // VT, move one line up
                cy1 += step_y;
                break;
			case 12:	// FF
				tek_erase();
				break;
            case 13:    // return
                cx1 = xcol;
                break;
            case 27:    // escape
                save_state(ESC);
                break;
            case 28:                // FS Point plot mode
                save_state(GRAPH);
                pplt = 1;
                break;
            case 29:                // Graph mode
                save_state(GRAPH);
                sflg = 0;
                cx2 = cx1 = save_x;
                cy2 = cy1 = save_y;
                break;
            case 30:                // Incremental plot mode.
                save_state(INC);
                rx = ry = 0;
                break;
            default:
                if ((ch >= 32) && (ch < 127))   // printable character
                {
                    if (cy1 < 8) cy1 = 8;
                    tek_draw_char(ch);
                    cx1 += step_x;
                    if (cx1 > REAL_WIDTH)
                        tek_newline();
                }
                break;
            }
            break;
        case GRAPH:
            // end of sequence is lox (flg==64)
            flg = ch & 96;      // Upper 2 bits of graph data
            switch (flg)
            {
            case 32:
                *hp = ch & 31;  // Might be hiy or hix
                break;
            case 64:
                cx2 = (hix << 5) + (ch & 31);
                cy2 = (hiy << 5) + loy;
                if (sflg && !pplt)
                    NewLinesAlpha(cx1, REAL_HEIGHT - cy1 - 1, cx2, REAL_HEIGHT - cy2 - 1);
                if (pplt)
                    PlotPoint(cx1, cy1);
                cx1 = cx2;
                cy1 = cy2;
                hp = &hiy;
                sflg = 1;      // Next call will draw rather then move
                break;
            case 96:
                loy = ch & 31;
                hp = &hix;             // Next data is hix
                break;
            case 0:                    // All control chars exit graph mode
                switch (ch)
                {
                case 27:
                    save_state(ESC);
                    break;
                case 29:
                    save_state(GRAPH);
                    sflg = 0;           // Set dark vector
                    hp = &hiy;
                    break;
                case 30:                // RS Incremental plot mode
                    save_state(INC);
                    rx = ry = 0;
                    break;
                case 28:                // FS Special Point plot mode
                    save_state(GRAPH);
                    pplt = 1;
                    break;
                case 22:                // SYN ??
                case 26:                // SUB ??
                    break;
                case 31:                // US or CR leave graphic
                case 13:
                    save_state(ALPHA);
                    pplt = 0;
                    save_x = cx1;
                    save_y = cy1;
                    break;
                default:
                    break;
                }
                break;
            }
            break;
        case INC:
            if (ch == 31)               // End of inc sequence
            {
                save_state(ALPHA);
                cx1 += rx/4;
                cy1 += ry/4;
                cx2 = cx1;
                cy2 = cy1;
                break;
            }
            if (!(ch & 15))
            {
                sflg = (ch & 16)?1:0;
                break;
            }
            if (ch & 1) rx++;
            if (ch & 2) rx--;
            if (ch & 4) ry++;
            if (ch & 8) ry--;
            break;
        case GIN:                       // Not implemented.
            break;
        case ESC:
            switch (ch)
            {
            case 48:                     // Deal with <esc>0n
                while (tek_get() != -1);
                break;
            case 28:                    // Special point plot mode
                save_state(lststate);
                while ((sppz = tek_get()) == -1);
                break;
            case 12:
                tek_erase();
                break;
            case ']':       // Trap window ident sequence see https://www.xfree86.org/current/ctlseqs.html
                while (1)
                {
                    if (tek_get() == 7)
                        break;
                }
                save_state(ALPHA);
                break;
            case '[':               // a second escape code follows: These sequences terminate
                                    // with a char in the range 0x40-0x7E
                while (1)
                {
                    ch=tek_get();
                    if (ch > 0x3f && ch < 0x80)
                        break;
                }
                save_state(ALPHA);
                break;
            case 26:            // SUB
                save_state(GIN);
                curflag++;
                GINon++;
                break;
            default:
                if ((ch & 96) == 96) {
                    if (ch & 16)
                        wrthru++;
                    else
                        wrthru = 0;
                    save_state(lststate);
                    switch (ch & 7) {
            case 0:
                setlinetype(SOLID);
                break;
            case 1:
                setlinetype(DOTTED);
                break;
            case 2:
                setlinetype(DOTDASH);
                break;
            case 3:
                setlinetype(SHORTDASH);
                break;
            case 4:
                setlinetype(LONGDASH);
                break;
                    }
                }
                else
                    save_state(ALPHA);
                break;
            }
            break;
        }
    }
}
