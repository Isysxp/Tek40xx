/* tek_video.h

   Dr Ian S Schofield 2019

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

#ifndef TEK_VIDEO_H_
#define TEK_VIDEO_H_     0
#include "tek_defs.h"
#ifdef  __cplusplus
extern "C" {
#endif

#define SIM_KEYPRESS_DOWN      0                        /* key states */
#define SIM_KEYPRESS_UP        1
#define SIM_KEYPRESS_REPEAT    2

struct mouse_event
{
    int32 x_rel;                                          /* X axis relative motion */
    int32 y_rel;                                          /* Y axis relative motion */
    int32 x_pos;                                          /* X axis position */
    int32 y_pos;                                          /* Y axis position */
    t_bool b1_state;                                      /* state of button 1 */
    t_bool b2_state;                                      /* state of button 2 */
    t_bool b3_state;                                      /* state of button 3 */
};

struct key_event
{
    uint32 key;                                           /* key sym */
    uint32 state;                                         /* key state change */
    uint32 mod;										      /* Key modifier bits */
};

struct phosphor
{
    double colr[3];
    int32 level;           /* Inital RGB pixel value */
};

typedef struct mouse_event SIM_MOUSE_EVENT;
typedef struct key_event SIM_KEY_EVENT;

#define BACKGROUND 40                                       /* Erase to this luminance */
#define WINDOW_WIDTH 1536
#define WINDOW_HEIGHT 1170
#define REAL_HEIGHT 780
#define REAL_WIDTH 1024

#define SIM_VID_INPUTCAPTURED       1                       /* Mouse and Keyboard input captured (calling */
enum LineType {SOLID,DOTTED,DOTDASH,SHORTDASH,LONGDASH};
enum TekState {ALPHA,GRAPH,GIN,ESC,INC};

/* code responsible for cursor display in video)
   -> init_flags for QVSS only */
t_stat vid_close (void);
t_stat vid_poll_kb (SIM_KEY_EVENT *ev);
t_stat vid_poll_mouse (SIM_MOUSE_EVENT *ev);
void vid_draw (int32 x, int32 y, int32 w, int32 h, uint32 *buf);
void vid_beep (void);
void vid_refresh (void);
const char *vid_version (void);
const char *vid_key_name (int32 key);
t_stat vid_open (char *dptr, const char *title, uint32 width, uint32 height, int flags);
t_stat vid_setpixel(int ix,int iy,int level,uint32 color);
uint32 vid_getpixel(int ix,int iy);
t_stat vid_erase_win();
void vid_drawline( int pX1, int pY1, int pX2, int pY2);
int vid_setcursor();
void vid_update_screen();
void save_state(enum TekState state);
int vid_map_key (int key);
t_stat vid_lock_cursor(void);
t_stat vid_unlock_cursor(void);
void setlinetype (enum LineType ltype);
void DrawGIN(int x1,int y1);
void PlotPoint(int x, int y);
void NewLinesAlpha(int x1,int y1,int x2,int y2);
void tek_erase();
int telnet(char *hostname, int port);
extern int tek_socket;
extern uint32 vid_mono_palette[2];
extern int alias;
void vid_set_cursor_position (int32 x, int32 y);        /* cursor position (set by calling code) */
extern unsigned char lo;                                /* Screen decay lower limit */
extern int clrflag, GINon, curflag, wrthru;                                     /* Marker for clear screen */
extern unsigned char console_font_12x16[];
extern unsigned char console_font_5x8[];
extern unsigned char console_font_7x9[];
extern unsigned char font_data[];


#ifdef  __cplusplus
}
#endif

#if defined(HAVE_LIBSDL)
#include <SDL.h>
#endif /* HAVE_LIBSDL */

#endif

