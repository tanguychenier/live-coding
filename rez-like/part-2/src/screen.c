#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "draw.h"
#include "pool.h"
#include "screen.h"

// the picture is the window, or the window shrunk by a whole number when
// the window has more pixels than the budget. the stretch back mixes the
// four picture pixels around each window pixel, by distance, and the
// columns and rows to mix are worked out once per window size
static void screen_fit(struct screen *screen)
{
	int divisor = 1;
	while ((screen->width / divisor) * (screen->height / divisor) > VIEW_PIXELS_MAX)
		divisor++;
	int wide = screen->width / divisor, tall = screen->height / divisor;
	if (wide < VIEW_MIN_WIDTH)
		wide = VIEW_MIN_WIDTH;
	if (tall < VIEW_MIN_HEIGHT)
		tall = VIEW_MIN_HEIGHT;
	draw_resize(wide, tall);
	// the picture keeps its shape, as big as fits, centred
	double across = (double)screen->width / view_width;
	double down = (double)screen->height / view_height;
	double zoom = across < down ? across : down;
	screen->wide = (int)(view_width * zoom);
	screen->tall = (int)(view_height * zoom);
	screen->left = (screen->width - screen->wide) / 2;
	screen->top = (screen->height - screen->tall) / 2;
	free(screen->col0);
	free(screen->row0);
	free(screen->strips);
	screen->col0 = calloc((size_t)screen->wide * 3, sizeof *screen->col0);
	screen->row0 = calloc((size_t)screen->tall * 3, sizeof *screen->row0);
	screen->strips = calloc((size_t)screen->wide * view_height, sizeof *screen->strips);
	screen->col1 = screen->col0 + screen->wide;
	screen->col_mix = screen->col1 + screen->wide;
	screen->row1 = screen->row0 + screen->tall;
	screen->row_mix = screen->row1 + screen->tall;
	for (int x = 0; x < screen->wide; x++) {
		double at = (x + 0.5) / zoom - 0.5;
		int first = (int)floor(at);
		screen->col_mix[x] = (int)((at - first) * STRETCH_STEPS);
		screen->col0[x] = first < 0 ? 0 : first;
		screen->col1[x] = first + 1 >= view_width ? view_width - 1 : first + 1;
	}
	for (int y = 0; y < screen->tall; y++) {
		double at = (y + 0.5) / zoom - 0.5;
		int first = (int)floor(at);
		screen->row_mix[y] = (int)((at - first) * STRETCH_STEPS);
		screen->row0[y] = first < 0 ? 0 : first;
		screen->row1[y] = first + 1 >= view_height ? view_height - 1 : first + 1;
	}
}

// the window buffer follows the window. a new size means a new buffer and
// a new image, and a new picture
static int screen_resize(struct screen *screen, int width, int height)
{
	if (screen->image) {
		screen->image->data = NULL;
		XDestroyImage(screen->image);
		free(screen->pixels);
	}
	screen->width = width;
	screen->height = height;
	screen->pixels = calloc((size_t)width * height, sizeof *screen->pixels);
	if (!screen->pixels)
		return 0;
	screen_fit(screen);
	// when the picture is the window, the image is made over the picture
	// itself, there is nothing to copy
	screen->direct = screen->wide == view_width && screen->tall == view_height
		&& screen->left == 0 && screen->top == 0;
	int number = DefaultScreen(screen->display);
	screen->image = XCreateImage(screen->display,
		DefaultVisual(screen->display, number),
		(unsigned int)DefaultDepth(screen->display, number), ZPixmap, 0,
		(char *)(screen->direct ? view : screen->pixels), (unsigned int)width,
		(unsigned int)height, 32, 0);
	return screen->image != NULL;
}

// how many times the base size the window is asked to be, or zero for the
// whole screen
static int window_scale(void)
{
	const char *asked = getenv("TEC_SCALE");
	return asked ? atoi(asked) : 0;
}

int screen_open(struct screen *screen, const char *title)
{
	memset(screen, 0, sizeof *screen);
	screen->display = XOpenDisplay(NULL);
	if (!screen->display) {
		fprintf(stderr, "no X display\n");
		return 0;
	}
	int number = DefaultScreen(screen->display);
	int scale = window_scale();
	int width = VIEW_BASE_WIDTH * WINDOW_SCALE, height = VIEW_BASE_HEIGHT * WINDOW_SCALE;
	if (scale > 0) {
		width = VIEW_BASE_WIDTH * scale;
		height = VIEW_BASE_HEIGHT * scale;
	}
	screen->window = XCreateSimpleWindow(screen->display,
		RootWindow(screen->display, number),
		(DisplayWidth(screen->display, number) - width) / 2,
		(DisplayHeight(screen->display, number) - height) / 2,
		(unsigned int)width, (unsigned int)height, 0, 0, WINDOW_BG);
	XStoreName(screen->display, screen->window, title);
	XClassHint hint = { "axon", "Axon" };
	XSetClassHint(screen->display, screen->window, &hint);

	// the window may grow, never shrink below the smallest picture
	XSizeHints hints = { .flags = PMinSize,
		.min_width = VIEW_MIN_WIDTH, .min_height = VIEW_MIN_HEIGHT };
	XSetWMNormalHints(screen->display, screen->window, &hints);

	// the close button becomes a message, so we decide what happens
	screen->close_message = XInternAtom(screen->display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(screen->display, screen->window, &screen->close_message, 1);

	// a held key must not arrive as a stream of press and release pairs
	Bool supported;
	XkbSetDetectableAutoRepeat(screen->display, True, &supported);

	XSelectInput(screen->display, screen->window,
		KeyPressMask | KeyReleaseMask | StructureNotifyMask
		| PointerMotionMask | ButtonPressMask | ButtonReleaseMask);
	// the desktop pointer has no place in the picture, the sight is drawn
	// by the game. an empty cursor hides it over the window
	char nothing[8] = { 0 };
	Pixmap empty = XCreateBitmapFromData(screen->display, screen->window, nothing, 8, 8);
	XColor black = { 0 };
	XDefineCursor(screen->display, screen->window,
		      XCreatePixmapCursor(screen->display, empty, empty, &black, &black, 0, 0));
	XFreePixmap(screen->display, empty);
	// without a scale, the window covers the whole screen. the window
	// manager reads the wish before the window is shown, and without a
	// window manager the window is simply made as big as the screen
	if (scale == 0) {
		Atom state = XInternAtom(screen->display, "_NET_WM_STATE", False);
		Atom full = XInternAtom(screen->display, "_NET_WM_STATE_FULLSCREEN", False);
		XChangeProperty(screen->display, screen->window, state, XA_ATOM, 32,
				PropModeReplace, (unsigned char *)&full, 1);
		screen->full = 1;
		width = DisplayWidth(screen->display, number);
		height = DisplayHeight(screen->display, number);
		XMoveResizeWindow(screen->display, screen->window, 0, 0,
				  (unsigned int)width, (unsigned int)height);
	}
	XMapWindow(screen->display, screen->window);
	screen->gc = XCreateGC(screen->display, screen->window, 0, NULL);
	if (!screen_resize(screen, width, height))
		return 0;
	XFlush(screen->display);
	return 1;
}

void screen_close(struct screen *screen)
{
	if (screen->image) {
		screen->image->data = NULL;
		XDestroyImage(screen->image);
		free(screen->pixels);
	}
	free(screen->col0);
	free(screen->row0);
	free(screen->strips);
	XFreeGC(screen->display, screen->gc);
	XDestroyWindow(screen->display, screen->window);
	XCloseDisplay(screen->display);
}

// there is no xlib call for full screen, only a window as big as the screen.
// the window manager, if there is one, is told through the usual hint
void screen_toggle_fullscreen(struct screen *screen)
{
	int number = DefaultScreen(screen->display);
	screen->full = !screen->full;
	Atom state = XInternAtom(screen->display, "_NET_WM_STATE", False);
	Atom full = XInternAtom(screen->display, "_NET_WM_STATE_FULLSCREEN", False);
	XEvent event;
	memset(&event, 0, sizeof event);
	event.xclient.type = ClientMessage;
	event.xclient.window = screen->window;
	event.xclient.message_type = state;
	event.xclient.format = 32;
	event.xclient.data.l[0] = screen->full;
	event.xclient.data.l[1] = (long)full;
	XSendEvent(screen->display, RootWindow(screen->display, number), False,
		   SubstructureRedirectMask | SubstructureNotifyMask, &event);
	if (screen->full)
		XMoveResizeWindow(screen->display, screen->window, 0, 0,
			(unsigned int)DisplayWidth(screen->display, number),
			(unsigned int)DisplayHeight(screen->display, number));
	else
		XResizeWindow(screen->display, screen->window,
			(unsigned int)(VIEW_BASE_WIDTH * WINDOW_SCALE),
			(unsigned int)(VIEW_BASE_HEIGHT * WINDOW_SCALE));
	XFlush(screen->display);
}

void screen_read_keys(struct screen *screen, struct keys *keys)
{
	while (XPending(screen->display)) {
		XEvent event;
		XNextEvent(screen->display, &event);
		if (event.type == ClientMessage
		    && (Atom)event.xclient.data.l[0] == screen->close_message)
			keys->quit = 1;
		if (event.type == ConfigureNotify
		    && (event.xconfigure.width != screen->width
			|| event.xconfigure.height != screen->height))
			screen_resize(screen, event.xconfigure.width,
				      event.xconfigure.height);
		if (event.type == MotionNotify) {
			// the mouse is read in picture pixels, whatever the window size
			keys->mouse_x = (event.xmotion.x - screen->left) * view_width / screen->wide;
			keys->mouse_y = (event.xmotion.y - screen->top) * view_height / screen->tall;
		}
		if (event.type == ButtonPress || event.type == ButtonRelease) {
			keys->mouse_down = event.type == ButtonPress;
			keys->any = 1;
		}
		if (event.type == KeyPress || event.type == KeyRelease) {
			int down = event.type == KeyPress;
			if (down)
				keys->any = 1;
			KeySym key = XkbKeycodeToKeysym(screen->display,
				event.xkey.keycode, 0, 0);
			switch (key) {
			case XK_Escape:    keys->quit = down; break;
			case XK_Up:        keys->up = down; break;
			case XK_Down:      keys->down = down; break;
			case XK_Left:      keys->left = down; break;
			case XK_Right:     keys->right = down; break;
			case XK_space:
			case XK_Control_L:
			case XK_Control_R: keys->fire = down; break;
			// a short press must not be lost, press and release can fall
			// in the same frame, so these are counted and read once
			case XK_Return:
			case XK_KP_Enter:  if (down) keys->enter = 1; break;
			case XK_BackSpace: if (down) keys->erase = 1; break;
			case XK_F11:       if (down) screen_toggle_fullscreen(screen); break;
			default:
				// a letter or a digit, for the name on the score table.
				// n on its own is the sound, when nobody is writing
				if (down && key >= XK_a && key <= XK_z)
					keys->typed = (char)('A' + (key - XK_a));
				else if (down && key >= XK_0 && key <= XK_9)
					keys->typed = (char)('0' + (key - XK_0));
				if (down && (key == XK_n || key == XK_N))
					keys->mute = 1;
				break;
			}
		}
	}
}

// a pixel holds three bytes, and two pixels are mixed in two multiplies by
// keeping the red and blue bytes in one word and the green in another, so
// that the bytes cannot run into each other
#define BYTE_MASK       ((1u << 8) - 1)
#define RED_BLUE_MASK   (BYTE_MASK | (BYTE_MASK << 16))
#define GREEN_MASK      (BYTE_MASK << 8)

static unsigned int mix_pixels(unsigned int a, unsigned int b, int weight)
{
	unsigned int keep = STRETCH_STEPS - (unsigned int)weight, take = (unsigned int)weight;
	unsigned int rb = ((a & RED_BLUE_MASK) * keep + (b & RED_BLUE_MASK) * take) >> STRETCH_SHIFT;
	unsigned int g = ((a & GREEN_MASK) * keep + (b & GREEN_MASK) * take) >> STRETCH_SHIFT;
	return (rb & RED_BLUE_MASK) | (g & GREEN_MASK);
}

// the picture is stretched into the window in two steps, each picture row
// stretched along into a strip as wide as the window, then each window row
// mixed from the two strips around it
static void stretch_along(int first, int last, void *data)
{
	struct screen *screen = data;
	for (int y = first; y < last; y++) {
		const unsigned int *row = view + y * view_width;
		unsigned int *strip = screen->strips + (size_t)y * screen->wide;
		for (int x = 0; x < screen->wide; x++)
			strip[x] = mix_pixels(row[screen->col0[x]], row[screen->col1[x]],
					      screen->col_mix[x]);
	}
}

static void stretch_down(int first, int last, void *data)
{
	struct screen *screen = data;
	for (int y = first; y < last; y++) {
		const unsigned int *above = screen->strips + (size_t)screen->row0[y] * screen->wide;
		const unsigned int *below = screen->strips + (size_t)screen->row1[y] * screen->wide;
		int down = screen->row_mix[y];
		unsigned int *out = screen->pixels
			+ (size_t)(screen->top + y) * screen->width + screen->left;
		for (int x = 0; x < screen->wide; x++)
			out[x] = mix_pixels(above[x], below[x], down);
	}
}

// at the picture's own size the rows are copied whole
static void copy_rows(int first, int last, void *data)
{
	struct screen *screen = data;
	for (int y = first; y < last; y++)
		memcpy(screen->pixels + (size_t)(screen->top + y) * screen->width + screen->left,
		       view + y * view_width, (size_t)view_width * sizeof *view);
}

void screen_present(struct screen *screen)
{
	if (screen->direct) {
		// the image is the picture
	} else if (screen->wide == view_width && screen->tall == view_height) {
		pool_run(view_height, copy_rows, screen);
	} else {
		pool_run(view_height, stretch_along, screen);
		pool_run(screen->tall, stretch_down, screen);
	}
	XPutImage(screen->display, screen->window, screen->gc, screen->image,
		  0, 0, 0, 0, (unsigned int)screen->width,
		  (unsigned int)screen->height);
	XFlush(screen->display);
}
