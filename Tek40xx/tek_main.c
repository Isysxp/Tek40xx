/* tek_main.: TEKTRONIX 40xx display

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
#include "tek_video.h"
#include <math.h>
#if defined (__linux__) || defined (VMS) || defined (__APPLE__)
#include <sys/types.h>
typedef unsigned long DWORD;
typedef unsigned char CHAR;
#include <sys/socket.h>
#include <unistd.h>
#endif


#if defined (main)                                  /* Required for SDL */
#undef main
#endif

FILE __iob_func[3] = { NULL, NULL, NULL };
//FILE _iob[] = { NULL,NULL,NULL };
//extern FILE * __cdecl __iob_func(void) { return _iob; }

// Global video system flgs/variables

uint32 vid_mono_palette[2];                         /* Monochrome Color Map */
double *colmap;										/* Parameter used by Refresh and vid_setpixel in sim_video.c */
int32 pxval;										/* Also referenced in display.c */
int nostore=0;							            /* Enables storage display. Always defined. */
int alias;                                          /* Aliasing active flag. Required to set correct pixel intensity */
int toclose = 0;                                    /* Flag used in tek_display to terminate thread */

struct phosphor p29 = {{0.0,0.9,0.0},0xff00};       // P29 phosphor
struct phosphor p6 = {{0.75,0.8,0.75},0xffffff};      // White phosphor

char bfr[128];
DWORD bcnt, brd, blft;

// Only include SDL code if specified. See end of this module for stubbed code.

#if defined(HAVE_LIBSDL)

static int iwd,iht,told,tnew,tvl;
static unsigned int init_w = 0;
static unsigned int init_h = 0;
static int32 lcurr_x = 0;
static int32 lcurr_y = 0;
static char vid_title[128];
static uint32 lstst=0,lstcd=0;
unsigned char *pixels;
unsigned char lo;
int GINon, curflag, wrthru;
static int surlen;
SIM_MOUSE_EVENT *xmev = 0,*xhev = 0;
SIM_KEY_EVENT *xkev = 0;
enum vid_stat {STOPPED,WINDOW_OK,RUNNING,CLOSING,CLOSED} vid_init;
static int init_flags, upflag;
unsigned int pixelval;					// 24 bit RGB value
struct display *dp;
static int Refresh(void *info);
static int MLoop();
static void vid_close_window(void);
void vid_beep (void);
int tek_socket;


static SDL_Window *window = 0;                    // Declare some pointers
SDL_Surface *surface = 0;
static SDL_Cursor *cursor = 0;
Uint32 render_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
SDL_Texture* tex;
// creates a renderer to render our images
SDL_Renderer* rend;
int32 vid_flags;                                        /* Open Flags */

#ifdef _WIN32
void usleep(unsigned int usec)
{
    HANDLE timer;
    LARGE_INTEGER ft;

    ft.QuadPart = -(10 * (__int64)usec);

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
}
#endif

static int main_argc;
static char **main_argv;
static SDL_Thread *vid_main_thread_handle;

int main_thread (void *arg)
{
    int stat;

    stat = SDL_main (main_argc, main_argv);	// Actually calls the main(...) function in scp.c
    vid_init = CLOSING;								// Will close MLoop
    return stat;
}

// THis is the entry point called if the graphicsc system is enabled.

int main (int argc, char *argv[])
{
    int status=0;

    main_argc = argc;
    main_argv = argv;

    if (argc < 2 || argc >3)
    {
        printf("Use tek4010 hostname [port]\r\n");
        exit(-1);
    }

	//HWND hwnd = GetConsoleWindow();
	//ShowWindow(hwnd, 0);

    vid_main_thread_handle = SDL_CreateThread (main_thread , "tek-main", NULL);

    while (MLoop() != -1)
        SDL_Delay(1);								// Transiently de-schedule this thread.

    toclose++;                                      // Flag vid_main_thread and then wait for it to exit
    SDL_WaitThread (vid_main_thread_handle, &status);
    vid_close();
    SDL_Quit();
    return status;
}

void UpdateWindowSurface()
{
    SDL_UpdateTexture(tex, NULL, surface->pixels, surface->pitch);
    SDL_RenderCopy(rend, tex, NULL, NULL);
    SDL_RenderPresent(rend);
}

t_stat vid_close (void)
{

    // Close and destroy the window if open
    if (vid_init != RUNNING)
        return 0;
    if (cursor)
        SDL_FreeCursor(cursor);
    vid_init = CLOSED;
    while (vid_init != STOPPED)
        SDL_Delay(100);			// Wait for MLoop to complete window close.

    // Clean up
    cursor = 0;
    nostore = 0;
    surface = 0;
    if (xmev)
    {
        free(xmev);
        free(xhev);
        free(xkev);
    }
    return 0;
}


/* Create a window that will report various events on the main thread ... required for OSX. */
/* For the QVSS display, set nostore such that the window will merely be refreshed */
/* Also, if QVSS, pixel data arrives from vax_vc.c already formatted. Using vid_draw instead. */
/* In the event that this function is called with a NULL title, storage mode (no fade) is assumed */
/* vid_init states as follows:
STOPPED. Initial state, no window, no message loop.
WINDOW_OK. Set by vid_open. MLoop will then call vid_create_window on main thread. -> RUNNING
RUNNING. Running state. Window open, message loop active.
CLOSING. Set by exit from main simh thread ... inititate shutdown. Exit from MLoop in main.
CLOSED. Set by vid_close. MLoop will close the window and move to state STOPPED.

At present init_flags may only contain SIM_VID_INPUTCATUED or SIM_OWN_CURSOR.

Comment: Some if the above could have been achieved using the SDL messaging system.
*/


t_stat vid_open (char *dptr, const char *title, uint32 width, uint32 height, int flags)
{
    init_w = width;
    init_h = height;
    init_flags = flags;

    vid_mono_palette[0] = 0xFF000000;                               /* Black */
    vid_mono_palette[1] = 0xFFFFFFFF;                               /* White */
    pxval = 0xff00;                                                  /* Green */
    xmev=(struct mouse_event *)calloc(1,sizeof(SIM_MOUSE_EVENT));
    xhev=(struct mouse_event *)calloc(1,sizeof(SIM_MOUSE_EVENT));
    xkev=(struct key_event *)calloc(1,sizeof(SIM_KEY_EVENT));
    colmap = p6.colr;
    vid_init = WINDOW_OK;										    // Flag such that MLoop can call vid_create_window
    alias = 0;                                                      // No aliasing.
    curflag = 0;                                                     // Cursor (GIN) off
    upflag = 1;                                                     // This is a marker flag that is cleared if no refresh required
    strcpy(vid_title, title);                                       // Set title

    return 0;
}

/*	This function creates the actual window and is called from MLoop on the main thread.
The expcted pixel format is 4x8 bytes. Any other format will result in an error exit.
This may seem rather limiting. However, modern hardware generally provides this format.
In the event that errors are reported, this function wil be updated.
NB. This function creates a renderer which is not used. The display system directly
manipulates the pixels in the surface buffer. This buffer is then written to the screen in Refresh().
*/


t_stat vid_create_window(void)
{
    SDL_DisplayMode mode;
    Uint32 flags = SDL_WINDOW_SHOWN;

    // Create an application window with the following settings:
    SDL_Init (SDL_INIT_VIDEO);

    if (SDL_GetCurrentDisplayMode(0, &mode)) {
        printf("SDL init fail: %s\r\n",SDL_GetError());
        exit(1);
    }

    if (mode.h == 768 && mode.w == 1024)
        flags |= SDL_WINDOW_FULLSCREEN;


    window = SDL_CreateWindow(
        vid_title,							// window title
        SDL_WINDOWPOS_UNDEFINED,			// initial x position
        SDL_WINDOWPOS_UNDEFINED,			// initial y position
        init_w,                             // width, in pixels
        init_h,                             // height, in pixels
        flags					// flags
        );

    // Check that the window was successfully created
    if (window == NULL)
    {
        // In the case that the window could not be made...
        printf("Could not create SDL window: %s\n", SDL_GetError());
        exit(-1);
    }
    //surface = SDL_GetWindowSurface(window);
    surface = SDL_CreateRGBSurfaceWithFormat(0, init_w, init_h, 32, SDL_PIXELFORMAT_RGB888);
    /* Check the bitdepth of the surface */
    if(surface->format->BitsPerPixel != 32)
    {
        fprintf(stderr, "Not an 32-bit SDL surface.\n");
        exit(-1);
    }
    /* Check the byes count of the surface */
    if(surface->format->BytesPerPixel != 4)
    {
        fprintf(stderr, "Invalid pixel format.\n");
        exit(-1);
    }

    rend = SDL_GetRenderer(window);
    if (rend)
        SDL_DestroyRenderer(rend);
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!rend)
        printf("%s\r\n", SDL_GetError());
    tex = SDL_CreateTextureFromSurface(rend, surface);
    if (!tex)
        printf("%s\r\n", SDL_GetError());


    //SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    pixels = (unsigned char *)surface->pixels;
    surlen = (init_h * surface->pitch);
    SDL_CreateThread(Refresh,"Refresh",(void *)NULL);                                                            /* for all other system that use their own cursor (see sim_ws.c:vid_open) */
    vid_init = RUNNING;									    /* Init OK continue to next state */

    return 1;
}

#if defined(HAVE_LIBPNG)

/*
This function coverts the default ARGB surface pixel format to ABGR (RGBA) which is the
only 32 bit format that is managed by libpng. Not very tidy!
Also, set A to 0xff.
*/

t_stat write_png_file(char *filename)
{
    int i;
    Uint8 *p,*buffer,tm;
    png_bytep *row_pointers;
    Uint8 *pixels;

    FILE *fp = fopen(filename, "wb");

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    png_infop info = png_create_info_struct(png);

    png_init_io(png, fp);

    buffer = (Uint8 *)malloc(surlen);
    pixels = (Uint8 *)surface->pixels;
    memcpy(buffer, pixels, surlen);
    for (i=0,p=buffer; i<surlen/4; i++,p+=4)
    {
        tm = p[0];
        p[0] = p[2];
        p[2] = tm;
        p[3] = 0xff;
    }

    row_pointers = (png_bytep *)malloc(sizeof(png_bytep)*init_h);
    for (i = 0; i < surface->h; i++)
        row_pointers[i] = (png_bytep)(Uint8 *)buffer + i * surface->pitch;

    // Output is 8bit depth, RGBA format.

    png_set_IHDR(
        png,
        info,
        init_w, init_h,
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
        );
    png_write_info(png, info);

    png_write_image(png, row_pointers);
    png_write_end(png, NULL);

    free(row_pointers);

    fclose(fp);
    return 0;
}
#else
t_stat write_png_file(char *filename)
{
    printf("PNG library not available\n");
    return 1;								/* Always return an error */
}
#endif

/*  This is a simple line drawing function */

void vid_drawline( int pX1, int pY1, int pX2, int pY2)
{
    int lenX = pX2 - pX1, i;
    int lenY = pY2 - pY1;
    float inclineX, inclineY, x, y;
    float vectorLen = sqrtf(lenX * lenX + lenY * lenY);
    if(vectorLen == 0)
    {
        //vid_setpixel(pX1, init_h-pY1, 7, 0xffffff);
        return;
    }

    inclineX = lenX / vectorLen;
    inclineY = lenY / vectorLen;

    x = (float)pX1;
    y = (float)pY1;

    for(i = 0; i < (int)vectorLen; ++i)
    {
        vid_setpixel((int)(x+0.5), init_h-(int)(y+0.5), 7, 0xffffff);
        x += inclineX;
        y += inclineY;
    }
}


void vid_set_cursor_position (int32 x, int32 y)
{

    lcurr_x = x;
    lcurr_y = y;
}


t_stat vid_set_cursor (t_bool visible, uint32 width, uint32 height, uint8 *data, uint8 *mask, uint32 hot_x, uint32 hot_y)
{

    if (!cursor)
    {
        while (vid_init != RUNNING)         /* Wait for Window to be created */
            SDL_Delay(10);
        cursor = SDL_CreateCursor (data, mask, width, height, hot_x, hot_y);
        SDL_SetCursor(cursor);
        SDL_ShowCursor(SDL_ENABLE);			/* Make new cursor visible */
        return 1;
    }
    return 0;
}


t_stat vid_erase_win()
{
    int i;

    SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 0, 200, 0));
    lo = BACKGROUND;                                        /* Fade 40  to look like real screen*/
     for (i=0;i<3;i++) {
    upflag++;
    nostore = 0;
    while (upflag)
        SDL_Delay(1);                               /* Wait for refresh to complete */
     }
    return 0;                                       /* This will rapidly fade like a flood erase */

}

/*
vid_draw. Similar to bitblt (Win.GDI) simply copies a pre-formatted buffer into the current surface.
The surface is then copied to the screen every 20 mS. in Refresh() as below.
*/

void vid_draw (int32 x, int32 y, int32 w, int32 h, uint32 *buf)
{

    int32 i;
    uint32* pixels;

    if (vid_init != RUNNING)
        return;

    pixels = (uint32 *)surface->pixels;
    for (i = 0; i < h; i++)
        memcpy (pixels + ((i + y) * init_w) + x, buf + w*i, w*sizeof(*pixels));

    return;
}

/* Update for this version to create a Tektronix 40xx display
*  Create flickering box cursor at current coordinates obatained from tek_display.c
*/

extern int cx1,cy1;         // Current location
extern enum TekState tekstate;

int vid_setcursor(uint32 mode)
{
    int ix,iy;

    if ((unsigned int)cy1*1.5 + 14 > init_h || tekstate != ALPHA || wrthru)
        return 1;

    for (ix=0;ix<10;ix+=2)
        for (iy=0;iy<14;iy+=2)
            if (mode)
                vid_setpixel(cx1*1.5+ix, init_h - cy1*1.5 - iy -1, 7, 0xa000);
            else
                vid_setpixel(cx1*1.5+ix, init_h - cy1*1.5 - iy - 1, 7, 0x2000);
    return 0;
}
/*
The function is central to all display system. Firstly, the current surface (effectively a back buffer)
is copied to the screen surface by SDL_UpdateWindowSurface. This function (hopefully) calls a GPU
bitblt process and with modern hardware is extremely fast (1 mS.).
The next section su responsible for gradually fading the image in the window. This is achieved using an RGB time
constant sequence in colmap such that the phosphor fade constant for each color can be different. This loop is also very fast
typically 10-15 mS. on recent X86 family processors clocked at >1.5 GHz. The final part of the loop ensure that the
entire cycle takes almost 20 mS. with due allowance for scheduling  etc. In this case, the fade constants can be preset and
no matter what is on screen, the fading will be constant.
As the fade constants are set by color, some interesting effects can be obtained. See display.c for examples where the pixels
can start of as blue/white and then have a yellow fade effect. Eg P40 used in the VS60 system.
Howvwer, for some displays eg QVSS, a fade is not required so this function is disabled by nostore. In this case, the system
emeulates a standard RGB bitmap display in framebuffer mode.
The nostore function can also be used for a storage scope display eg the Tektronix 611 XY display. (See pdp8_vc8.c).
*/

static int Refresh(void *info)
{
    int i,j, mode = 1, ccntr = 4;
    unsigned char *p, *q;
    uint32 *z;
    double *d;
    unsigned char lox[]= {2,0,0};                               // Lo limit of decay for R/G/B (Blue is color key for GIN cursor)
    unsigned char lon[]= {2,BACKGROUND,0};                      // Setting for writethrough
    register int update = 0;                                    // This is a flag to indiacte that a refresh is no required


    while (window && vid_init != STOPPED)
    {
        lox[1]=lo;                                          // Decay green to lo R/B to 0,2
        told=SDL_GetTicks();
        if (vid_init == RUNNING)  				            // If halted ... freeze display and, display valid
        {
            if (ccntr-- == 0) {
                vid_setcursor(mode);
                mode = !mode;
                ccntr = 4;
                UpdateWindowSurface();				// Write the surface to the host system window
            }


            if (curflag)
                DrawGIN(xmev->x_pos , xmev->y_pos);

            update = 0;
            if (!nostore)                 // Only decay the pixels in store mode and if the simulator is running
                for (i = 0,p=(unsigned char *)pixels, z=(uint32 *)p; i < surlen/4; i++, p++, z++) {
                    q = (*z & 0xff000000)?lon:lox;
                    for (j = 0,d = colmap; j < 3; j++, p++, d++, q++)
                        if (*p > *q) {
                            *p = (unsigned char)(*p * (*d));	// Decay red/blue to 0 and, green pixels if >lo only
                            update++;
                        }
                }
        }

        if (update + nostore == 0) {
            nostore++;
            upflag = 0;
        }
        else
            UpdateWindowSurface();				// Write the surface to the host system window

        tnew = SDL_GetTicks();
        tvl = 20 - tnew + told;				// Calculate delay required for a constant update time of 20mSec.
        if (tvl < 0)						// System not fast enough so just continue with no delay.
            tvl = 0;
        if (tvl)
            SDL_Delay(tvl);
    }
    return 0;							// The window has been closed by vid_close ... exit thread
}


/*	The Mouse system.

The following function is central to the mouse implementation for the QVSS (VCB01) Q-bus card.
The intention of this function is to provide seamless enrty and exit of the mouse pointer
from within the host window system. The first 6 lines of code relate to window initialisation.
This could equally have been achived using messages but a state machine is more convenient.
The next 6 lines constitute a system that generates the required relative motion data so that the
QVSS mouse location generated by the OS ruunning on the simh client eg VMS is matched to the
location of the (actaully invisible) cursor managed by the host OS. The is achived by calculating
the X and Y pointing errors and generating a sequence of relative movements until the location of
the client cursor matches that of the host. The conditional expression reduces the 'gain' of this
servo loop for relative errors > 8. This is required because the client OS response to a given
relative move is non-linear due to a degree of mouse acceleration implemented by the client. THis
non linear response is implemented in both DEC Windows under VMS and Ultrix.
The overall efect of this process is that when the host cursor enter the QVSS window, the client
OS is sent a number of move messages to move the client cursor to the same location as the
now invisible host cursor.
This interface is also used by all systems with a light pen (VT11/VT60/PDP1/TX-0).
Similarly, the keyboard data is made available to all systems (QVSS only at present).

*/

static void vid_close_window(void)
{
    vid_init = STOPPED;							// Reset vid_init state
    SDL_DestroyWindow(window);					// Close this window and renderer.
    window = 0;									// Invalidate handle
}

static int MLoop()
{
    SDL_Event event;
    int rel_x,rel_y;

    switch (vid_init)
    {
    case STOPPED:
        return 0;				// Wait until window has been initialised
    case WINDOW_OK:
        vid_create_window();	// Create window and begin receiving events
        break;
    case RUNNING:               // No action. Poll tek 4010 interface.
        break;
    case CLOSING:
        nostore++;               // Disable any refresh
        return -1;				// Exit message loop and start shutdown
    case CLOSED:
        vid_close_window();		// Close window continue receiving events until SDL_Quit
        break;
    }

    rel_x = xmev->x_pos - lcurr_x;
    rel_y = xmev->y_pos - lcurr_y;
    rel_x = (abs(rel_x) > 9)?rel_x/2:rel_x;
    rel_y = (abs(rel_y) > 9)?rel_y/2:rel_y;

    xmev->x_rel = rel_x;
    xmev->y_rel = rel_y;

    //    PeekNamedPipe(fdHnd, bfr, 128, &brd, &bcnt, &blft);
    //    if (bcnt)
    //        bcnt++;

    if (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            return -1;
        case SDL_MOUSEMOTION:
            xmev->x_pos = event.motion.x-iwd;
            xmev->y_pos = event.motion.y-iht;
            break;
        case SDL_MOUSEBUTTONDOWN:
            xmev->b1_state = (event.button.button==SDL_BUTTON_LEFT);
            xmev->b2_state = (event.button.button==SDL_BUTTON_RIGHT);
            xmev->b3_state = (event.button.button==SDL_BUTTON_MIDDLE);
            xmev->x_pos = event.button.x-iwd;
            xmev->y_pos = event.button.y-iht;
            curflag++;
            GINon++;
            break;
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) xmev->b1_state = 0;
            if (event.button.button == SDL_BUTTON_RIGHT) xmev->b2_state = 0;
            if (event.button.button == SDL_BUTTON_MIDDLE) xmev->b3_state = 0;
            xmev->x_pos = event.button.x - iwd;
            xmev->y_pos = event.button.y - iht;
            save_state(ALPHA);
            GINon = 0;
            break;
        case SDL_KEYDOWN:
            xkev->key = event.key.keysym.sym;
            xkev->state = SIM_KEYPRESS_DOWN;
            xkev->mod = event.key.keysym.mod;
            if (event.key.keysym.scancode == SDL_SCANCODE_END)
            {
#ifdef _WIN32
                closesocket(tek_socket);
#else
                close(tek_socket);
#endif
                vid_init = CLOSING;                         // Start orderly shutdown
                break;
            }
            if (event.key.keysym.scancode == SDL_SCANCODE_HOME)
            {
                tek_erase();
                break;
            }
            xkev->key = (xkev->mod & KMOD_CTRL)? xkev->key & 31:xkev->key;
            if (tek_socket && xkev->key < 32 && xkev->key)
                send(tek_socket, (const char *)&xkev->key, 1, 0);
            break;
        case SDL_KEYUP:
            xkev->key = event.key.keysym.sym;
            xkev->state = SIM_KEYPRESS_UP;
            xkev->mod = event.key.keysym.mod;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                vid_init = CLOSING;
            break;
        case SDL_TEXTINPUT:
            if (tek_socket)
                send(tek_socket, event.text.text, 1, 0);
            break;
        case SDL_USEREVENT:
            break;
            break;

        }
    }
    return 0;
}

/* Only return 1 if there is a change in the mouse or keyboard data */

int vid_poll_mouse (SIM_MOUSE_EVENT *mev)
{

    if (!xmev)
        return 0;
    memcpy(mev,xmev,sizeof(SIM_MOUSE_EVENT));
    if ( !memcmp(xhev,xmev,sizeof(SIM_MOUSE_EVENT)) && !(mev->x_rel | xmev->y_rel) )
        return 0;							/* Only report changes */
    memcpy(xhev,xmev,sizeof(SIM_MOUSE_EVENT));		/* Keep local copy */

    return 1;
}

t_stat vid_poll_kb (SIM_KEY_EVENT *ev)
{


    if (!xkev)
        return 0;
    if (xkev->state == lstst && lstcd == xkev->key)
        return 0;								/* Single events only */
    memcpy(ev,xkev,sizeof(SIM_KEY_EVENT));
    lstst = xkev->state;
    lstcd = xkev->key;
    return 1;
}

/*
This function draws a pixel on the current surface if the graphics system is in state 2.
The surface is written to the screen (blit) in Refresh() above. ALL display systems use this
function. At present, level and color are ignored.
*/

t_stat vid_setpixel(int ix,int iy,int level,Uint32 color)
{
    Uint32 *p,q = 0xffffff;

    if (ix < 0 || ix > WINDOW_WIDTH - 2) 
        return 0;
    if (iy < 0 || iy > WINDOW_HEIGHT - 2) 
        return 0;


    if (vid_init == RUNNING)
    {
        p=(Uint32 *)(pixels + (iy * surface->pitch) + (ix * sizeof(Uint32)));
        if (wrthru) {
            q=*p;
            if ((q & 0xff00) == (BACKGROUND << 8))      // Only write to 'black' pixels
                *p = 0xff00e001;                            // Writethrough color. Set Alpha bits to competely fade this pixel
        }
        else
            *p = color;
    }
    nostore = 0;
    return 1;
}

Uint32 vid_getpixel(int ix,int iy)
{
    Uint32 *p;

    if (vid_init == RUNNING)
    {
        p=(Uint32 *)(pixels + (iy * surface->pitch) + (ix * sizeof(Uint32)));
        return (*p);
    }
    return 0xffffffff;
}
t_stat vid_lock_cursor()
{
    SDL_ShowCursor(SDL_DISABLE);		   /* Make host OS cursor invisible in non-capture/default mode */
    SDL_SetWindowGrab(window, SDL_TRUE);   /* Lock mouse to this window */
#ifdef __APPLE__
    SDL_SetRelativeMouseMode(SDL_TRUE);
#endif
    return 1;
}

t_stat vid_unlock_cursor()
{
    if (SDL_GetWindowGrab(window) == SDL_TRUE)
    {
        SDL_SetWindowGrab(window, SDL_FALSE);							/* Unlock mouse for this window */
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(SDL_ENABLE);										/* Make host OS cursor visible in non-capture/default mode */
        SDL_WarpMouseInWindow(window, xmev->x_pos + 1, xmev->y_pos );	/* This is required to redraw the host cursor */
        return 1;
    }
    return 0;
}
#endif
