#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "draw.h"
#include "screen.h"

// the picture follows the window. a new size means a new picture and a new
// image over it, XCreateImage copies nothing
static int screen_resize(struct screen *screen, int width, int height)
{
	if (screen->image) {
		screen->image->data = NULL;
		XDestroyImage(screen->image);
	}
	screen->width = width;
	screen->height = height;
	if (!draw_resize(width, height))
		return 0;
	int number = DefaultScreen(screen->display);
	screen->image = XCreateImage(screen->display,
		DefaultVisual(screen->display, number),
		(unsigned int)DefaultDepth(screen->display, number), ZPixmap, 0,
		(char *)view, (unsigned int)width, (unsigned int)height, 32, 0);
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
	}
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
			keys->mouse_x = event.xmotion.x;
			keys->mouse_y = event.xmotion.y;
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
			case XK_n:
			case XK_N:         if (down) keys->mute = 1; break;
			case XK_F11:       if (down) screen_toggle_fullscreen(screen); break;
			}
		}
	}
}

// the image is the picture, so the whole of it goes to the window at once
void screen_present(struct screen *screen)
{
	XPutImage(screen->display, screen->window, screen->gc, screen->image,
		  0, 0, 0, 0, (unsigned int)screen->width,
		  (unsigned int)screen->height);
	XFlush(screen->display);
}
