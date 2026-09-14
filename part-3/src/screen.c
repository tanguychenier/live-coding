#include "screen.h"

#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>

// every pixel of the game lands here first
unsigned int *view;
int view_width = WIN_WIDTH / PIXEL;
int view_height = WIN_HEIGHT / PIXEL;

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
	// the window decides how many rays we cast, and a view
	// pixel stays a whole number of screen pixels
	int vw = screen->width / PIXEL;
	int vh = screen->height / PIXEL;
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

	screen->scale = PIXEL;
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
		KeyPressMask | KeyReleaseMask | StructureNotifyMask);

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

void screen_read_keys(struct screen *screen, struct keys *keys)
{
	while (XPending(screen->display)) {
		XEvent event;
		XNextEvent(screen->display, &event);

		if (event.type == ClientMessage &&
		    (Atom)event.xclient.data.l[0] == screen->close_message)
			keys->quit = 1;

		// the server tells us the new size; the picture follows it
		if (event.type == ConfigureNotify &&
		    (event.xconfigure.width != screen->width ||
		     event.xconfigure.height != screen->height))
			screen_resize(screen, event.xconfigure.width,
				event.xconfigure.height);

		if (event.type == KeyPress || event.type == KeyRelease) {
			int down = event.type == KeyPress;
			KeySym key = XkbKeycodeToKeysym(screen->display,
				event.xkey.keycode, 0, 0);
			switch (key) {
			case XK_Escape: keys->quit = down; break;
			case XK_Up:     keys->forward = down; break;
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
	int cover_w = view_width * scale, cover_h = view_height * scale;
	if (screen->left > 0 || screen->top > 0
	    || cover_w < screen->width || cover_h < screen->height)
		for (int i = 0; i < screen->width * screen->height; i++)
			screen->pixels[i] = WINDOW_BG;
	// a window narrower than the picture: we draw the columns that fit, and
	// this does not change from one row to the next
	int visible = (screen->width - screen->left) / scale;
	if (visible > view_width)
		visible = view_width;
	for (int y = 0; y < view_height; y++) {
		unsigned int *row = view + y * view_width;
		for (int copy = 0; copy < scale; copy++) {
			int line = screen->top + y * scale + copy;
			if (line >= screen->height)
				continue;
			unsigned int *out = screen->pixels + line * screen->width
				+ screen->left;
			for (int x = 0; x < visible; x++) {
				unsigned int color = row[x];
				for (int again = 0; again < scale; again++)
					*out++ = color;
			}
		}
	}
	XPutImage(screen->display, screen->window, screen->gc, screen->image,
		0, 0, 0, 0, screen->width, screen->height);
	XFlush(screen->display);
}
