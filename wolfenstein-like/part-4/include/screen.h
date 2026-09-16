#ifndef SCREEN_H
#define SCREEN_H

#include <X11/Xlib.h>

// one view pixel is a block of PIXEL by PIXEL pixels: two keeps the edges
// without costing four times as many rays
#define PIXEL       2
// the size the window opens at. after that a wider window shows more world,
// it does not blow up what was already there
#define WIN_WIDTH   1280
#define WIN_HEIGHT  800
// never less than this, whatever the player does to the window
#define VIEW_MIN_W  160
#define VIEW_MIN_H  100

// the picture, and its size at this instant. it changes with the window, so
// it can no longer be an array whose size is known at compile time.
extern unsigned int *view;
extern int view_width, view_height;

#define HORIZON     (view_height / 2)

// XCreateImage wants to know how each row of pixels is padded, in bits. this
// is NOT the colour depth: 32 is what a modern display expects
#define SCANLINE_PAD 32

#define GAME_NAME  "The Keep"
#define WINDOW_BG  0x182636

struct screen {
	Display *display;
	Window   window;
	GC       gc;
	XImage  *image;
	Atom     close_message;
	unsigned int *pixels;
	// the window is the player's to resize; the picture keeps its own size
	Visual *visual;
	int depth;
	int width, height;
	int scale, left, top;
	int full;                   // are we filling the screen?
};

struct keys {
	int forward, back, left, right, strafe_left, strafe_right, quit;
	int push;        // a press on space, waiting to be read
	int map;         // is the map on screen? M takes it away
	int hit;         // a press on ctrl: swing
	int weapon;      // 1 or 2: the bar or the sidearm
	int any;         // somebody is at the keys
};

// the frame buffer belongs to the screen; everyone else just writes in it
int screen_open(struct screen *screen);
int screen_resize(struct screen *screen, int width, int height);
void screen_toggle_fullscreen(struct screen *screen);
void screen_read_keys(struct screen *screen, struct keys *keys);
void screen_present(struct screen *screen);

#endif
