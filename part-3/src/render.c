#include "render.h"

#include "screen.h"
#include "texture.h"

#include <math.h>

// far away is dark: light is 1 at the eye and falls towards 0 with the
// square of the distance. one line, and the corridors have depth
double fog(double distance)
{
	return 1.0 / (1.0 + distance * distance * FOG_DENSITY);
}

// one column of the wall, read down one column of the texture
static void draw_wall_column(int x, int top, int height, int tex_x,
	double light, int dark, int kind)
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
		unsigned int color = wall_texture[kind][((int)tex_y & TEX_MASK) * TEX_SIZE + tex_x];
		view[y * VIEW_WIDTH + x] = shade(color, dark ? light * SIDE_LIGHT : light);
		tex_y += step;
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
void render_floor_and_ceiling(const struct player *player)
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

// what a ray found: how far it went, which way the wall it met faces, and
// where along that wall it landed
struct hit {
	double distance;
	double wall_x;
	int side;
	// which of the walls it is: the level file says so, by a digit
	int kind;
};

// walk the grid square by square until we meet a wall, always stepping along
// the axis whose grid line is nearest: no square is missed, and none is
// visited twice
static struct hit cast_ray(const struct player *player, double ray_x, double ray_y)
{
	int map_x = (int)player->x;
	int map_y = (int)player->y;

	// the distance the ray covers to cross one whole square
	double delta_x = ray_x == 0.0 ? VERY_FAR : fabs(1.0 / ray_x);
	double delta_y = ray_y == 0.0 ? VERY_FAR : fabs(1.0 / ray_y);
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

	// how far it went, measured on the camera plane and not from the eye:
	// from the eye the corners of a wall are farther than its middle, and
	// a straight wall would bend into a fishbowl
	double distance = side == 0 ? side_x - delta_x : side_y - delta_y;
	if (distance < NEAR_CLIP)
		distance = NEAR_CLIP;

	// where along the wall it landed, between 0 and 1: that is the column
	// of the texture to read
	double wall_x = side == 0 ? player->y + distance * ray_y
		: player->x + distance * ray_x;
	wall_x -= floor(wall_x);

	struct hit hit = { .distance = distance, .wall_x = wall_x, .side = side,
			   .kind = wall_kind(map_x, map_y) };
	return hit;
}

// one ray per column of the screen, and how far that ray went decides how
// tall the wall is drawn: near is tall, far is short
void render_walls(const struct player *player)
{
	for (int x = 0; x < VIEW_WIDTH; x++) {
		// -1 on the left edge of the screen, +1 on the right edge
		double camera = 2.0 * x / VIEW_WIDTH - 1.0;
		double ray_x = player->dir_x + player->plane_x * camera;
		double ray_y = player->dir_y + player->plane_y * camera;
		struct hit hit = cast_ray(player, ray_x, ray_y);

		int height = (int)(VIEW_HEIGHT / hit.distance);
		int top = HORIZON - height / 2;

		int tex_x = (int)(hit.wall_x * TEX_SIZE);
		// the two faces we can see of the same wall must not be mirror
		// images of each other
		if ((hit.side == 0 && ray_x > 0) || (hit.side == 1 && ray_y < 0))
			tex_x = TEX_SIZE - 1 - tex_x;

		// the two orientations must not share a shade, or every corner
		// disappears
		draw_wall_column(x, top, height, tex_x, fog(hit.distance),
			hit.side == 1, hit.kind);
	}
}

// one pixel, if it is on the screen at all. the heading line runs off the
// map, and until now it wrote wherever that landed in memory
static void put_pixel(int x, int y, unsigned int color)
{
	if (x >= 0 && x < VIEW_WIDTH && y >= 0 && y < VIEW_HEIGHT)
		view[y * VIEW_WIDTH + x] = color;
}

// the same map, now small enough to live in a corner
void render_map(const struct player *player, int cell, int left, int top)
{
	for (int y = 0; y < map_height; y++)
		for (int x = 0; x < map_width; x++) {
			unsigned int color = is_wall(x, y) ? MAP_WALL : MAP_FLOOR;
			for (int line = 0; line < cell; line++)
				for (int column = 0; column < cell; column++)
					put_pixel(left + x * cell + column,
						top + y * cell + line, color);
		}

	// where we stand, and which way we look
	int dot_x = left + (int)(player->x * cell);
	int dot_y = top + (int)(player->y * cell);
	for (int step = MAP_DOT + 1; step < MAP_ARROW * cell; step++)
		put_pixel(dot_x + (int)(player->dir_x * step),
			dot_y + (int)(player->dir_y * step), MAP_HEADING);
	for (int line = -MAP_DOT; line <= MAP_DOT; line++)
		for (int column = -MAP_DOT; column <= MAP_DOT; column++)
			put_pixel(dot_x + column, dot_y + line, MAP_PLAYER);
}
