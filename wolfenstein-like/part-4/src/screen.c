#include "screen.h"
#include "story.h"

#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// every pixel of the game lands here first
unsigned int *view;
int view_width = WIN_WIDTH / (WIN_HEIGHT / VIEW_TARGET_H);
int view_height = WIN_HEIGHT / (WIN_HEIGHT / VIEW_TARGET_H);

// the window at the size of the picture, in the middle of the screen
static void screen_windowed(struct screen *screen)
{
	int number = DefaultScreen(screen->display);
	int full_width = DisplayWidth(screen->display, number);
	int full_height = DisplayHeight(screen->display, number);

	XMoveResizeWindow(screen->display, screen->window,
		(full_width - WIN_WIDTH) / 2, (full_height - WIN_HEIGHT) / 2,
		WIN_WIDTH, WIN_HEIGHT);
}

// and full screen, without asking anyone: there is no xlib call for it, only
// a window as big as the screen
static void screen_fullscreen(struct screen *screen)
{
	int number = DefaultScreen(screen->display);

	XMoveResizeWindow(screen->display, screen->window, 0, 0,
		DisplayWidth(screen->display, number),
		DisplayHeight(screen->display, number));
}

void screen_toggle_fullscreen(struct screen *screen)
{
	screen->full = !screen->full;
	if (screen->full)
		screen_fullscreen(screen);
	else
		screen_windowed(screen);

	XFlush(screen->display);
}

// how big a block each view pixel becomes, and where the picture sits in the
// window. a whole number of pixels per pixel, or the walls shimmer
static void screen_fit(struct screen *screen)
{
	// the window decides how many rays we cast, and a view pixel stays a
	// whole number of screen pixels. the block follows the window, so that a
	// pixel looks the same size whatever the window does.
	int scale = screen->height / VIEW_TARGET_H;
	if (scale < 1)
		scale = 1;
	if (scale > PIXEL_MAX)
		scale = PIXEL_MAX;
	int vw = screen->width / scale;
	int vh = screen->height / scale;
	if (vw < VIEW_MIN_W)
		vw = VIEW_MIN_W;
	if (vh < VIEW_MIN_H)
		vh = VIEW_MIN_H;
	if (vw != view_width || vh != view_height || !view) {
		unsigned int *bigger = realloc(view, (size_t)vw * vh * sizeof(*view));
		if (bigger) {
			view = bigger;
			view_width = vw;
			view_height = vh;
		}
	}

	screen->scale = scale;
	screen->left = (screen->width - view_width * screen->scale) / 2;
	screen->top = (screen->height - view_height * screen->scale) / 2;
}

// a new window size means a new buffer and a new image. XDestroyImage frees
// the pixels it was handed, so the old buffer goes with the old image
int screen_resize(struct screen *screen, int width, int height)
{
	unsigned int *pixels = malloc((size_t)width * height * sizeof(unsigned int));
	if (!pixels) {
		fprintf(stderr, "out of memory\n");
		return 0;
	}

	if (screen->image)
		XDestroyImage(screen->image);

	screen->pixels = pixels;
	screen->width = width;
	screen->height = height;
	// XCreateImage copies nothing: we keep writing into our own buffer and
	// the server reads it from there
	screen->image = XCreateImage(screen->display, screen->visual, screen->depth,
		ZPixmap, 0, (char *)pixels, width, height, SCANLINE_PAD, 0);
	screen_fit(screen);
	return screen->image != NULL;
}

int screen_open(struct screen *screen)
{
	screen->display = XOpenDisplay(NULL);
	if (!screen->display) {
		fprintf(stderr, "no X display\n");
		return 0;
	}

	int number = DefaultScreen(screen->display);
	Visual *visual = DefaultVisual(screen->display, number);
	int depth = DefaultDepth(screen->display, number);
	screen->window = XCreateSimpleWindow(screen->display,
		RootWindow(screen->display, number),
		0, 0, WIN_WIDTH, WIN_HEIGHT, 0, 0, WINDOW_BG);

	// the window manager wants to know who it is drawing a title bar for
	XClassHint class_hint = { "raycaster", "Raycaster" };
	XSetClassHint(screen->display, screen->window, &class_hint);
	XStoreName(screen->display, screen->window, GAME_NAME);

	// the window may grow as much as it likes, but never below the picture:
	// under that there is nothing left to show
	XSizeHints hints = { .flags = PMinSize,
		.min_width = view_width, .min_height = view_height };
	XSetWMNormalHints(screen->display, screen->window, &hints);

	// without this the close button cuts the connection under our feet;
	// with it we get a message and we decide what to do
	screen->close_message = XInternAtom(screen->display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(screen->display, screen->window, &screen->close_message, 1);

	// and without this one, holding a key gives us release/press pairs
	// thirty times a second, so the player would stutter
	Bool supported;
	XkbSetDetectableAutoRepeat(screen->display, True, &supported);

	// StructureNotifyMask is what tells us the window changed size. without
	// it the picture stays in the corner when the window grows
	XSelectInput(screen->display, screen->window,
		KeyPressMask | KeyReleaseMask | StructureNotifyMask
		| PointerMotionMask | ButtonPressMask | FocusChangeMask);

	// we use an empty pointer, because a desktop cross in the middle of a
	// corridor breaks everything, and the mouse no longer points at anything
	// anyway
	char nothing[8] = { 0 };
	Pixmap empty = XCreateBitmapFromData(screen->display, screen->window,
		nothing, 8, 8);
	XColor black = { 0 };
	screen->blank = XCreatePixmapCursor(screen->display, empty, empty,
		&black, &black, 0, 0);
	XFreePixmap(screen->display, empty);
	screen->warped = 0;
	screen->grab = 0;

	screen_windowed(screen);
	XMapWindow(screen->display, screen->window);
	screen->gc = XCreateGC(screen->display, screen->window, 0, NULL);

	screen->visual = visual;
	screen->depth = depth;
	screen->image = NULL;
	screen->full = 0;

	if (!screen_resize(screen, WIN_WIDTH, WIN_HEIGHT))
		return 0;

	XFlush(screen->display);
	return 1;
}

// gives the mouse back, so the pointer is visible again and free to leave the
// window
void screen_release_mouse(struct screen *screen)
{
	if (!screen->grab)
		return;
	screen->grab = 0;
	screen->warped = 0;
	XUndefineCursor(screen->display, screen->window);
}

void screen_read_keys(struct screen *screen, struct keys *keys)
{
	keys->look = 0.0;
	while (XPending(screen->display)) {
		XEvent event;
		XNextEvent(screen->display, &event);

		if (event.type == ClientMessage &&
		    (Atom)event.xclient.data.l[0] == screen->close_message)
			keys->quit = 1;

		// the server tells us the new size; the picture follows it
		if (event.type == ConfigureNotify &&
		    (event.xconfigure.width != screen->width ||
		     event.xconfigure.height != screen->height)) {
			// we give the mouse back as soon as the window changes size,
			// because while you drag an edge, the window manager and the game
			// both pull the pointer and the view goes anywhere. a click in
			// the window takes it again.
			screen_release_mouse(screen);
			screen_resize(screen, event.xconfigure.width,
				event.xconfigure.height);
		}

		// on a click in the window we take the mouse and hide the pointer.
		// the menu and a loss of focus give it back.
		if (event.type == ButtonPress && !screen->grab) {
			screen->grab = 1;
			screen->warped = 0;
			XDefineCursor(screen->display, screen->window, screen->blank);
		}
		if (event.type == FocusOut)
			screen_release_mouse(screen);

		// the pointer is pulled back to the centre after every move, and we
		// throw away the move we just caused, otherwise the return to the
		// centre would be read as a move by the player
		if (event.type == MotionNotify && screen->grab) {
			int mx = screen->width / 2, my = screen->height / 2;
			int dx = event.xmotion.x - mx;
			if (event.xmotion.x == mx && event.xmotion.y == my)
				continue;
			if (screen->warped)
				keys->look += dx * MOUSE_SPEED;
			XWarpPointer(screen->display, None, screen->window,
				0, 0, 0, 0, mx, my);
			screen->warped = 1;
		}

		if (event.type == KeyPress || event.type == KeyRelease) {
			int down = event.type == KeyPress;
			if (down)
				keys->any = 1;
			KeySym key = XkbKeycodeToKeysym(screen->display,
				event.xkey.keycode, 0, 0);
			switch (key) {
			// escape opens the menu, it does not quit, because a game whose
			// only exit is closing the window has no pause, no restart, and
			// no reminder of the keys
			case XK_Escape: if (down) keys->menu = 1; break;
			case XK_Return:
			case XK_KP_Enter: if (down) keys->validate = 1; break;
			case XK_Control_L:
			case XK_Control_R: if (down) keys->hit = 1; break;
			case XK_n:
			case XK_N: if (down) keys->mute = 1; break;
			// you choose your weapon yourself. the game falls back to the bar
			// on an empty magazine, which is right, but on its own the player
			// would never learn that he holds two things.
			case XK_1: case XK_KP_1: if (down) keys->weapon = 1; break;
			case XK_2: case XK_KP_2: if (down) keys->weapon = 2; break;
			case XK_w:
			case XK_z:
			case XK_Up:     keys->forward = down; break;
			case XK_s:
			case XK_Down:   keys->back = down; break;
			case XK_Left:   keys->left = down; break;
			case XK_Right:  keys->right = down; break;
			case XK_a:      keys->strafe_left = down; break;
			case XK_d:      keys->strafe_right = down; break;
			case XK_space:
				// a short press must not be lost: press and
				// release can fall in the same frame
				if (down)
					keys->push = 1;
				break;
			case XK_m:
				// on release, not on press: a held key repeats,
				// and the map would blink
				if (!down)
					keys->map = !keys->map;
				break;
			case XK_F11:
				if (down)
					screen_toggle_fullscreen(screen);
				break;
			}
		}
	}
}

// blow the 320x200 view up into the window, one source pixel per block
void screen_present(struct screen *screen)
{
	// we only clear what we do not redraw: clearing it all
	// first is writing every pixel of every frame twice
	int scale = screen->scale;
	// a hit throws the picture, and the alarm takes the colour out of it.
	// both are applied here, on the way to the window, because the renderer
	// knows nothing about the story and the story draws nothing itself.
	int jolt_x = story_shake(0) * scale, jolt_y = story_shake(1) * scale;
	// a shake leaves a band that is not copied over, and that band keeps the
	// previous frame, a strip of corridor frozen at the edge. so we also
	// clear the window when the picture is shifted, not only when it is
	// smaller.
	int cover_w = view_width * scale, cover_h = view_height * scale;
	if (screen->left > 0 || screen->top > 0 || jolt_x || jolt_y
	    || cover_w < screen->width || cover_h < screen->height)
		for (int i = 0; i < screen->width * screen->height; i++)
			screen->pixels[i] = WINDOW_BG;
	// a window narrower than the picture: we draw the columns that fit, and
	// this does not change from one row to the next
	int visible = (screen->width - screen->left) / scale;
	if (visible > view_width)
		visible = view_width;
	// the shake shifts the copy, so it can leave the window. the rows are
	// bounded and the columns have to be too, so we compute once per frame
	// the first and last column that fall inside, instead of testing every
	// pixel.
	int base = screen->left + jolt_x;
	int first = base < 0 ? (-base + scale - 1) / scale : 0;
	int last = visible;
	if (base + last * scale > screen->width)
		last = (screen->width - base) / scale;
	// the second row is a copy of the first. enlarging every pixel once per
	// row cost a million writes per frame, but the bottom row of a block is
	// the same as its top row, so we build it once and copy it whole.
	for (int y = 0; y < view_height && first < last; y++) {
		unsigned int *row = view + y * view_width;
		int line = screen->top + y * scale + jolt_y;
		if (line < 0 || line >= screen->height)
			continue;
		unsigned int *out = screen->pixels + line * screen->width
			+ base + first * scale;
		for (int x = first; x < last; x++) {
			unsigned int color = story_grade_at(row[x], y);
			for (int again = 0; again < scale; again++)
				*out++ = color;
		}
		for (int copy = 1; copy < scale; copy++) {
			int twin = line + copy;
			if (twin < 0 || twin >= screen->height)
				break;
			memcpy(screen->pixels + twin * screen->width
					+ base + first * scale,
			       screen->pixels + line * screen->width
					+ base + first * scale,
			       (size_t)(last - first) * scale
					* sizeof(*screen->pixels));
		}
	}
	XPutImage(screen->display, screen->window, screen->gc, screen->image,
		0, 0, 0, 0, screen->width, screen->height);
	XFlush(screen->display);
}
