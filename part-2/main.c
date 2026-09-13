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
#define SCALE       4
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

// a texture is a square of pixels drawn once at startup. a power of two, so
// wrapping around it is a bitwise and instead of a division
#define TEX_SIZE   64
#define TEX_MASK   (TEX_SIZE - 1)

// a brick wall: the size of one brick, its two colours, and how far one brick
// is allowed to differ from the next so that the wall is not flat
#define BRICK_WIDTH  16
#define BRICK_HEIGHT 8
#define BRICK_COLOR  0x8a6f5d
#define BRICK_JOINT  0x4a4038
#define BRICK_LIGHT  0.78
#define BRICK_VARY   0.30

// a tiled floor: a tile is square, so one size is enough
#define TILE_SIZE   32
#define TILE_COLOR  0x4a4a52
#define TILE_JOINT  0x2e2e34
#define TILE_LIGHT  0.85
#define TILE_VARY   0.25

// a panelled ceiling: panels are wide and flat, so a height and not a size,
// and a coarser grain than stone
#define PANEL_HEIGHT 16
#define PANEL_GRAIN  8
#define PANEL_COLOR  0x343b46
#define PANEL_JOINT  0x232932
#define PANEL_LIGHT  0.90
#define PANEL_VARY   0.15

// a wall met on a north-south line keeps this much of its light
#define SIDE_LIGHT   0.68

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
	// the window is the player's to resize; the picture keeps its own size
	Visual *visual;
	int depth;
	int width, height;
	int scale, left, top;
	int full;                   // are we filling the screen?
};

// every pixel of the game lands here first
static unsigned int view[VIEW_WIDTH * VIEW_HEIGHT];

static unsigned int wall_texture[TEX_SIZE * TEX_SIZE];
static unsigned int floor_texture[TEX_SIZE * TEX_SIZE];
static unsigned int ceiling_texture[TEX_SIZE * TEX_SIZE];

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

// how big a block each view pixel becomes, and where the picture sits in the
// window. a whole number of pixels per pixel, or the walls shimmer
static void screen_fit(struct screen *screen)
{
	int by_width = screen->width / VIEW_WIDTH;
	int by_height = screen->height / VIEW_HEIGHT;

	screen->scale = by_width < by_height ? by_width : by_height;
	if (screen->scale < 1)
		screen->scale = 1;
	// a window smaller than the picture would give a negative corner, and we
	// would copy pixels in front of the buffer
	screen->left = (screen->width - VIEW_WIDTH * screen->scale) / 2;
	if (screen->left < 0)
		screen->left = 0;
	screen->top = (screen->height - VIEW_HEIGHT * screen->scale) / 2;
	if (screen->top < 0)
		screen->top = 0;
}

// a new window size means a new buffer and a new image. XDestroyImage frees
// the pixels it was handed, so the old buffer goes with the old image
static int screen_resize(struct screen *screen, int width, int height)
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
		ZPixmap, 0, (char *)pixels, width, height, 32, 0);
	screen_fit(screen);
	return screen->image != NULL;
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

	// the window may grow as much as it likes, but never below the picture:
	// under that there is nothing left to show
	XSizeHints hints = { .flags = PMinSize,
		.min_width = VIEW_WIDTH, .min_height = VIEW_HEIGHT };
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
	// whatever the picture does not cover stays the window colour
	for (int i = 0; i < screen->width * screen->height; i++)
		screen->pixels[i] = WINDOW_BG;

	int scale = screen->scale;
	// a window narrower than the picture: we draw the columns that fit, and
	// this does not change from one row to the next
	int visible = (screen->width - screen->left) / scale;
	if (visible > VIEW_WIDTH)
		visible = VIEW_WIDTH;
	for (int y = 0; y < VIEW_HEIGHT; y++) {
		unsigned int *row = view + y * VIEW_WIDTH;
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

// the same square always gets the same number between 0 and 1: a brick keeps
// its shade from one frame to the next, and the wall does not crawl. the three
// constants are arbitrary large odd numbers, any others would do as well
static double noise(int x, int y)
{
	unsigned int n = (unsigned int)(x * 374761393 + y * 668265263);
	n = (n ^ (n >> 13)) * 1274126177u;
	return (double)((n >> 16) & 0xffff) / 65535.0;
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

// one column of the wall, read down one column of the texture
static void draw_wall_column(int x, int top, int height, int tex_x,
	double light, int dark)
{
	double step = (double)TEX_SIZE / height;
	double tex_y = 0.0;
	int y = top;

	// the wall can be taller than the screen: we start reading in the
	// middle of the texture rather than clamping, or the bricks slide
	// under our feet as we walk towards them
	if (y < 0) {
		tex_y = -top * step;
		y = 0;
	}

	int bottom = top + height;
	if (bottom > VIEW_HEIGHT)
		bottom = VIEW_HEIGHT;

	for (; y < bottom; y++) {
		unsigned int color = wall_texture[((int)tex_y & TEX_MASK) * TEX_SIZE + tex_x];
		view[y * VIEW_WIDTH + x] = shade(color, dark ? light * SIDE_LIGHT : light);
		tex_y += step;
	}
}

// bricks: rows half a brick apart, a mortar line between them, and each
// brick a shade of its own. drawn once, read a million times
static void make_wall_texture(void)
{
	for (int y = 0; y < TEX_SIZE; y++) {
		for (int x = 0; x < TEX_SIZE; x++) {
			int row = y / BRICK_HEIGHT;
			int shift = (row & 1) ? BRICK_WIDTH / 2 : 0;
			int column = (x + shift) / BRICK_WIDTH;
			int joint = y % BRICK_HEIGHT == 0 ||
				(x + shift) % BRICK_WIDTH == 0;
			unsigned int color = joint ? BRICK_JOINT
				: shade(BRICK_COLOR,
					BRICK_LIGHT + BRICK_VARY * noise(column, row));
			wall_texture[y * TEX_SIZE + x] = color;
		}
	}
}

// the ground: square tiles with a joint, and a grain that keeps the eye busy
static void make_floor_texture(void)
{
	for (int y = 0; y < TEX_SIZE; y++) {
		for (int x = 0; x < TEX_SIZE; x++) {
			int joint = x % TILE_SIZE == 0 || y % TILE_SIZE == 0;
			unsigned int color = joint ? TILE_JOINT
				: shade(TILE_COLOR,
					TILE_LIGHT + TILE_VARY * noise(x, y));
			floor_texture[y * TEX_SIZE + x] = color;
		}
	}
}

// the ceiling: wide panels, darker, so up and down never look alike
static void make_ceiling_texture(void)
{
	for (int y = 0; y < TEX_SIZE; y++) {
		for (int x = 0; x < TEX_SIZE; x++) {
			int joint = y % PANEL_HEIGHT == 0;
			unsigned int color = joint ? PANEL_JOINT
				: shade(PANEL_COLOR,
					PANEL_LIGHT + PANEL_VARY * noise(x / PANEL_GRAIN, y));
			ceiling_texture[y * TEX_SIZE + x] = color;
		}
	}
}

// how far the ray is from the first grid line it will cross
static double first_line(double position, double direction)
{
	double cell = position - floor(position);
	return direction < 0 ? cell : 1.0 - cell;
}

// the ground and the sky, one screen row at a time. every pixel of a row is
// the same distance away, so the walk across the floor is a straight line, and
// the row costs two additions per pixel
static void render_floor_and_ceiling(const struct player *player)
{
	// the ray through the left edge of the screen. the same for every row,
	// so it is worked out once
	double left_x = player->dir_x - player->plane_x;
	double left_y = player->dir_y - player->plane_y;

	for (int y = HORIZON + 1; y < VIEW_HEIGHT; y++) {
		// how far the ground under this row is: a row one pixel below
		// the horizon is very far, the bottom row is right at our feet,
		// and the eye is half a wall above the floor
		double distance = (double)VIEW_HEIGHT / (2 * y - VIEW_HEIGHT);

		// where that row starts on the floor, and what one pixel to the
		// right is worth. the camera plane spans from -plane to +plane,
		// so the whole row is two planes wide
		double step_x = distance * 2.0 * player->plane_x / VIEW_WIDTH;
		double step_y = distance * 2.0 * player->plane_y / VIEW_WIDTH;
		double ground_x = player->x + distance * left_x;
		double ground_y = player->y + distance * left_y;
		double light = fog(distance);

		for (int x = 0; x < VIEW_WIDTH; x++) {
			int tex_x = (int)(ground_x * TEX_SIZE) & TEX_MASK;
			int tex_y = (int)(ground_y * TEX_SIZE) & TEX_MASK;
			int at = tex_y * TEX_SIZE + tex_x;

			view[y * VIEW_WIDTH + x] = shade(floor_texture[at], light);
			view[(VIEW_HEIGHT - 1 - y) * VIEW_WIDTH + x] = shade(ceiling_texture[at], light);

			ground_x += step_x;
			ground_y += step_y;
		}
	}
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

		// where along the wall the ray landed, between 0 and 1. that is
		// the column of the texture to read
		double wall_x = side == 0 ? player->y + distance * ray_y
					  : player->x + distance * ray_x;
		wall_x -= floor(wall_x);
		int tex_x = (int)(wall_x * TEX_SIZE);
		// the two faces we can see of the same wall must not be mirror
		// images of each other
		if ((side == 0 && ray_x > 0) || (side == 1 && ray_y < 0))
			tex_x = TEX_SIZE - 1 - tex_x;

		// the two orientations must not share a shade, or every corner
		// disappears
		draw_wall_column(x, top, height, tex_x, fog(distance), side == 1);
	}
}

// a look at what we just made, until the walls can wear it
static void show_texture(const unsigned int *texture)
{
	for (int y = 0; y < TEX_SIZE; y++)
		for (int x = 0; x < TEX_SIZE; x++)
			view[y * VIEW_WIDTH + VIEW_WIDTH - TEX_SIZE + x] =
				texture[y * TEX_SIZE + x];
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

	// the textures cost nothing to keep and everything to draw: once, here
	make_wall_texture();
	make_floor_texture();
	make_ceiling_texture();

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
		render_floor_and_ceiling(&player);
		render_walls(&player);
		show_texture(wall_texture);
		render_map(&player, MAP_CELL, MAP_LEFT, MAP_TOP);
		screen_present(&screen);

		// nothing here needs more than sixty frames a second, and without
		// this the loop eats a whole core to draw the same thing twice
		double spare = 1.0 / 60.0 - (now_in_seconds() - moment);
		if (spare > 0)
			usleep((useconds_t)(spare * 1e6));

	}

	// XDestroyImage frees the buffer it was given, so this is the whole
	// clean-up: the picture, then the connection
	XDestroyImage(screen.image);
	XCloseDisplay(screen.display);
	return 0;
}
