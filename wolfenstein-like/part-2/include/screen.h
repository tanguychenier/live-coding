#ifndef SCREEN_H
#define SCREEN_H

#include <X11/Xlib.h>

// the world is drawn at 320x200 and blown up to fill the window: chunky
// pixels are not nostalgia, they are four times fewer rays to trace
#define VIEW_WIDTH  320
#define VIEW_HEIGHT 200
#define SCALE       4
#define WIN_WIDTH   (VIEW_WIDTH * SCALE)
#define WIN_HEIGHT  (VIEW_HEIGHT * SCALE)
#define HORIZON     (VIEW_HEIGHT / 2)

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
};

// the frame buffer belongs to the screen; everyone else just writes in it
extern unsigned int view[VIEW_WIDTH * VIEW_HEIGHT];

int screen_open(struct screen *screen);
int screen_resize(struct screen *screen, int width, int height);
void screen_toggle_fullscreen(struct screen *screen);
void screen_read_keys(struct screen *screen, struct keys *keys);
void screen_present(struct screen *screen);

#endif