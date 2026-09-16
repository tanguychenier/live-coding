#ifndef SCREEN_H
#define SCREEN_H

#include <X11/Xlib.h>

// the size of a view pixel should not depend on the window. with a fixed
// block of two, a small window renders very few rays and a wide one renders a
// very fine picture, so the game looks different at every size. instead we
// aim for a rendered height, and the block is whatever gets us closest to it.
#define VIEW_TARGET_H 400
#define PIXEL_MAX     4
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
// is not the colour depth: 32 is what a modern display expects
#define SCANLINE_PAD 32

// how many radians one pixel of mouse movement is worth
#define MOUSE_SPEED 0.0032

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
	// the mouse turns the head, because a first person view turned with the
	// arrow keys plays like 1992. every frame we read how far the pointer
	// moved, then we pull it back to the centre. the game only takes the
	// mouse on the first click.
	int grab;                   // is the mouse ours
	int warped;                 // it has been put back once already
	Cursor blank;               // and it is not drawn while playing
};

struct keys {
	int forward, back, left, right, strafe_left, strafe_right, quit;
	int push;        // a press on space, waiting to be read
	int hit;         // a press on ctrl: fists first, then the gun
	int mute;        // a press on n: the sound goes away
	int map;         // is the map on screen? M takes it away
	int menu;        // escape: the menu opens or closes
	int validate;    // entree
	int weapon;      // 1 or 2: the bar or the gun
	double look;     // how far the mouse moved sideways, in radians
	int any;         // somebody is at the keys
};

// the frame buffer belongs to the screen; everyone else just writes in it
int screen_open(struct screen *screen);
int screen_resize(struct screen *screen, int width, int height);
void screen_toggle_fullscreen(struct screen *screen);
void screen_read_keys(struct screen *screen, struct keys *keys);
// gives the mouse back to the desktop. the menu does it, and so does losing
// focus.
void screen_release_mouse(struct screen *screen);
void screen_present(struct screen *screen);

#endif
