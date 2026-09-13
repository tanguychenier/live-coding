// wolfenstein 3d drew its corridors with one ray per column, in 1992
// let us write that engine in c, from this empty file
// x11 gives us a window and a block of memory, the rest is ours

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <math.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// the world is drawn at 320x200 and blown up to fill the window: chunky
// pixels are not nostalgia, they are four times fewer rays to trace
#define VIEW_WIDTH  320
#define VIEW_HEIGHT 200
#define SCALE       2
#define WIN_WIDTH   (VIEW_WIDTH * SCALE)
#define WIN_HEIGHT  (VIEW_HEIGHT * SCALE)
#define HORIZON     (VIEW_HEIGHT / 2)

#define MAP_WIDTH  24
#define MAP_HEIGHT 24

#define WALK_SPEED 3.0
#define TURN_SPEED 2.2

#define CEILING_COLOR 0x2a3038
#define FLOOR_COLOR   0x3a3630
#define WALL_LIGHT    0x8a93a5
#define WALL_DARK     0x5d6675

// how wide the view is: 0.66 against a unit direction is about 66 degrees
#define FIELD_OF_VIEW 0.66

#define MAP_CELL   3
#define MAP_LEFT   4
#define MAP_TOP    4

#define MAP_WALL   0x6f7b8f
#define MAP_FLOOR  0x161a22
#define MAP_HEADING 0xe8b04b
#define MAP_PLAYER 0xd8534f

#define GAME_NAME  "The Keep"
#define WINDOW_BG  0x182636

// the whole world: a wall is anything that is not a dot
static const char *map[MAP_HEIGHT] = {
	"########################",
	"#......................#",
	"#..####..........####..#",
	"#..#..#..........#..#..#",
	"#..#..#..........#..#..#",
	"#..####..........####..#",
	"#......................#",
	"#......................#",
	"####.##########.########",
	"#......................#",
	"#..###....##....###....#",
	"#....#....##....#......#",
	"#....#....##....#......#",
	"#....#..........#......#",
	"#....############......#",
	"#......................#",
	"#......................#",
	"########.####.##########",
	"#......................#",
	"#..##..........##......#",
	"#..##..........##......#",
	"#......................#",
	"#......................#",
	"########################",
};

static int is_wall(int x, int y)
{
	if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
		return 1;
	return map[y][x] != '.';
}

struct screen {
	Display *display;
	Window   window;
	GC       gc;
	XImage  *image;
	Atom     close_message;
	unsigned int *pixels;
	int full;                   // are we filling the screen?
};

// every pixel of the game lands here first
static unsigned int view[VIEW_WIDTH * VIEW_HEIGHT];

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

static void screen_toggle_fullscreen(struct screen *screen)
{
	screen->full = !screen->full;
	if (screen->full)
		screen_fullscreen(screen);
	else
		screen_windowed(screen);

	XFlush(screen->display);
}

static int screen_open(struct screen *screen)
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

	// without this the close button cuts the connection under our feet;
	// with it we get a message and we decide what to do
	screen->close_message = XInternAtom(screen->display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(screen->display, screen->window, &screen->close_message, 1);

	// and without this one, holding a key gives us release/press pairs
	// thirty times a second, so the player would stutter
	Bool supported;
	XkbSetDetectableAutoRepeat(screen->display, True, &supported);

	XSelectInput(screen->display, screen->window, KeyPressMask | KeyReleaseMask);

	screen_windowed(screen);
	XMapWindow(screen->display, screen->window);
	screen->gc = XCreateGC(screen->display, screen->window, 0, NULL);
	screen->full = 0;

	// XCreateImage copies nothing: we keep writing into our own buffer and
	// the server reads it from there
	screen->pixels = malloc(WIN_WIDTH * WIN_HEIGHT * sizeof(unsigned int));
	if (!screen->pixels) {
		fprintf(stderr, "out of memory\n");
		return 0;
	}

	screen->image = XCreateImage(screen->display, visual, depth, ZPixmap, 0,
		(char *)screen->pixels, WIN_WIDTH, WIN_HEIGHT, 32, 0);
	XFlush(screen->display);
	return 1;
}

struct keys {
	int forward, back, left, right, strafe_left, strafe_right, quit;
};

static void screen_read_keys(struct screen *screen, struct keys *keys)
{
	while (XPending(screen->display)) {
		XEvent event;
		XNextEvent(screen->display, &event);

		if (event.type == ClientMessage &&
		    (Atom)event.xclient.data.l[0] == screen->close_message)
			keys->quit = 1;

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
			case XK_F11:
				if (down)
					screen_toggle_fullscreen(screen);
				break;
			}
		}
	}
}

// blow the 320x200 view up into the window, one source pixel per block
static void screen_present(struct screen *screen)
{
	for (int y = 0; y < VIEW_HEIGHT; y++) {
		unsigned int *row = view + y * VIEW_WIDTH;
		for (int copy = 0; copy < SCALE; copy++) {
			unsigned int *out = screen->pixels + (y * SCALE + copy) * WIN_WIDTH;
			for (int x = 0; x < VIEW_WIDTH; x++) {
				unsigned int color = row[x];
				for (int again = 0; again < SCALE; again++)
					*out++ = color;
			}
		}
	}
	XPutImage(screen->display, screen->window, screen->gc, screen->image,
		0, 0, 0, 0, WIN_WIDTH, WIN_HEIGHT);
	XFlush(screen->display);
}

struct player {
	double x, y;                // where we stand, in map squares
	double dir_x, dir_y;        // a unit vector: where the eyes point
	double plane_x, plane_y;    // the camera plane, as wide as the view is
};

// each axis is tested on its own, so a shoulder against a wall keeps sliding
// instead of stopping the player dead. the margin is the nose: without it we
// walk until the eyes are inside the texture
static void move_player(struct player *player, double step_x, double step_y)
{
	const double margin = 0.2;
	double nose_x = step_x > 0 ? margin : -margin;
	double nose_y = step_y > 0 ? margin : -margin;

	if (!is_wall((int)(player->x + step_x + nose_x), (int)player->y))
		player->x += step_x;
	if (!is_wall((int)player->x, (int)(player->y + step_y + nose_y)))
		player->y += step_y;
}

// turning is one rotation matrix, applied to both vectors at once
static void turn_player(struct player *player, double angle)
{
	double cosine = cos(angle);
	double sine = sin(angle);

	double dir_x = player->dir_x;
	player->dir_x = dir_x * cosine - player->dir_y * sine;
	player->dir_y = dir_x * sine + player->dir_y * cosine;
	double plane_x = player->plane_x;
	player->plane_x = plane_x * cosine - player->plane_y * sine;
	player->plane_y = plane_x * sine + player->plane_y * cosine;
}

static unsigned int shade(unsigned int color, double light)
{
	if (light > 1.0)
		light = 1.0;

	unsigned int red = (unsigned int)(((color >> 16) & 0xff) * light);
	unsigned int green = (unsigned int)(((color >> 8) & 0xff) * light);
	unsigned int blue = (unsigned int)((color & 0xff) * light);
	return (red << 16) | (green << 8) | blue;
}

// far away is dark. one line, and the corridors have depth
static double fog(double distance)
{
	return 1.0 / (1.0 + distance * distance * 0.02);
}

static void draw_column(int x, int top, int bottom, unsigned int color)
{
	if (top < 0)
		top = 0;
	if (bottom > VIEW_HEIGHT)
		bottom = VIEW_HEIGHT;

	for (int y = top; y < bottom; y++)
		view[y * VIEW_WIDTH + x] = color;
}

// how far the ray is from the first grid line it will cross
static double first_line(double position, double direction)
{
	double cell = position - floor(position);
	return direction < 0 ? cell : 1.0 - cell;
}

// one ray per column of the screen. the ray walks the grid square by square
// until it meets a wall, and how far it went decides how tall to draw it
static void render_walls(const struct player *player)
{
	for (int x = 0; x < VIEW_WIDTH; x++) {
		// -1 on the left edge of the screen, +1 on the right edge
		double camera = 2.0 * x / VIEW_WIDTH - 1.0;
		double ray_x = player->dir_x + player->plane_x * camera;
		double ray_y = player->dir_y + player->plane_y * camera;

		int map_x = (int)player->x;
		int map_y = (int)player->y;

		// the distance the ray covers to cross one whole square
		double delta_x = ray_x == 0.0 ? 1e30 : fabs(1.0 / ray_x);
		double delta_y = ray_y == 0.0 ? 1e30 : fabs(1.0 / ray_y);
		double side_x = first_line(player->x, ray_x) * delta_x;
		double side_y = first_line(player->y, ray_y) * delta_y;
		int step_x = ray_x < 0 ? -1 : 1;
		int step_y = ray_y < 0 ? -1 : 1;

		// always step along the axis whose grid line is nearest. that is
		// the whole trick: no square is missed, and none is visited twice
		int side = 0;
		while (!is_wall(map_x, map_y)) {
			if (side_x < side_y) {
				side_x += delta_x;
				map_x += step_x;
				side = 0;
			} else {
				side_y += delta_y;
				map_y += step_y;
				side = 1;
			}
		}

		// how far the ray actually distance before it hit something
		double distance = side == 0 ? side_x - delta_x : side_y - delta_y;
		// measured from the eye, the corners of a wall are farther than its
		// middle, so a straight wall bends into a fishbowl. what we want is
		// the distance on the camera plane, and the ray was already giving
		// it to us before we "corrected" it
		if (distance < 0.02)
			distance = 0.02;

		int height = (int)(VIEW_HEIGHT / distance);
		int top = HORIZON - height / 2;

		draw_column(x, 0, top, CEILING_COLOR);
		// the two orientations must not share a shade, or every corner
		// disappears
		double light = fog(distance);
		draw_column(x, top, top + height,
			shade(side == 0 ? WALL_LIGHT : WALL_DARK, light));
		draw_column(x, top + height, VIEW_HEIGHT, FLOOR_COLOR);
	}
}

// the same map, now small enough to live in a corner
static void render_map(const struct player *player, int cell, int left, int top)
{
	for (int y = 0; y < MAP_HEIGHT; y++)
		for (int x = 0; x < MAP_WIDTH; x++) {
			unsigned int color = is_wall(x, y) ? MAP_WALL : MAP_FLOOR;
			for (int line = 0; line < cell; line++)
				for (int column = 0; column < cell; column++)
					view[(top + y * cell + line) * VIEW_WIDTH
					     + left + x * cell + column] = color;
		}

	// where we stand, and which way we look
	int dot_x = left + (int)(player->x * cell);
	int dot_y = top + (int)(player->y * cell);
	for (int step = 3; step < 4 * cell; step++)
		view[(dot_y + (int)(player->dir_y * step)) * VIEW_WIDTH
		     + dot_x + (int)(player->dir_x * step)] = MAP_HEADING;
	for (int line = -2; line <= 2; line++)
		for (int column = -2; column <= 2; column++)
			view[(dot_y + line) * VIEW_WIDTH + dot_x + column] = MAP_PLAYER;
}

static double now_in_seconds(void)
{
	struct timespec moment;
	clock_gettime(CLOCK_MONOTONIC, &moment);
	return moment.tv_sec + moment.tv_nsec / 1e9;
}

int main(void)
{
	struct screen screen;
	if (!screen_open(&screen))
		return 1;

	struct player player = {
		.x = 2.5, .y = 6.5,
		.dir_x = 1.0, .dir_y = 0.0,
		.plane_x = 0.0, .plane_y = FIELD_OF_VIEW,
	};
	struct keys keys = { 0 };
	double last = now_in_seconds();

	while (!keys.quit) {
		double moment = now_in_seconds();
		double elapsed = moment - last;
		last = moment;

		screen_read_keys(&screen, &keys);

		// every move is scaled by the time the last frame took, so the game
		// runs at the same speed whatever the machine is doing
		double forward = (keys.forward - keys.back) * WALK_SPEED * elapsed;
		double sideways = (keys.strafe_right - keys.strafe_left) * WALK_SPEED * elapsed;
		double turn = (keys.right - keys.left) * TURN_SPEED * elapsed;

		move_player(&player, player.dir_x * forward + player.plane_x * sideways,
			player.dir_y * forward + player.plane_y * sideways);
		turn_player(&player, turn);
		render_walls(&player);
		render_map(&player, MAP_CELL, MAP_LEFT, MAP_TOP);
		screen_present(&screen);

		// nothing here needs more than sixty frames a second, and without
		// this the loop eats a whole core to draw the same thing twice
		double spare = 1.0 / 60.0 - (now_in_seconds() - moment);
		if (spare > 0)
			usleep((useconds_t)(spare * 1e6));

	}
	XCloseDisplay(screen.display);
	return 0;
}
