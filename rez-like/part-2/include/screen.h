#ifndef SCREEN_H
#define SCREEN_H

#include <X11/Xlib.h>

// the picture is the size of the window, so that a line is a line of the
// screen's own pixels and never a block of them. the game was laid out at
// 640 by 400, the sight and the numbers are scaled up from there
#define VIEW_BASE_WIDTH   640
#define VIEW_BASE_HEIGHT  400
#define VIEW_PIXELS_MAX   2200000
#define VIEW_MIN_WIDTH    320
#define VIEW_MIN_HEIGHT   200
// the window opens on the whole screen. TEC_SCALE opens it at so many
// times the base size instead, and F11 goes between the two
#define WINDOW_SCALE 2
#define WINDOW_BG    0
// the stretch mixes four picture pixels for each window pixel, with
// weights in this many steps, a power of two so that a shift divides
#define STRETCH_SHIFT 8
#define STRETCH_STEPS (1 << STRETCH_SHIFT)

// a pixel is three bytes packed in an int, the way X11 wants them
static inline unsigned int rgb(int r, int g, int b)
{
	return (unsigned int)((r << 16) | (g << 8) | b);
}

struct keys {
	int quit, fire, up, down, left, right;
	int enter;        // a press on enter, waiting to be read
	int mute;         // a press on n, waiting to be read
	int any;          // somebody pressed a key or clicked since last read
	int mouse_x, mouse_y, mouse_down;
	char typed;       // a letter or a digit pressed, waiting to be read
	int erase;        // backspace, waiting to be read
};

struct screen {
	Display *display;
	Window window;
	GC gc;
	XImage *image;
	unsigned int *pixels;        // the window's own buffer, window sized
	int width, height;           // the window
	int left, top, wide, tall;   // where the picture lands in it
	int full;                    // filling the whole screen
	int direct;                  // the image is the picture itself, no copy
	Atom close_message;
	// the stretch, for each window column and row, the two picture columns
	// or rows it mixes and the weight of the second
	int *col0, *col1, *col_mix;
	int *row0, *row1, *row_mix;
	unsigned int *strips;        // the picture rows stretched along, window wide
};

// the picture, and its size at this instant
extern unsigned int *view;
extern int view_width, view_height;

int  screen_open(struct screen *screen, const char *title);
void screen_close(struct screen *screen);
void screen_read_keys(struct screen *screen, struct keys *keys);
void screen_toggle_fullscreen(struct screen *screen);
void screen_present(struct screen *screen);

#endif
