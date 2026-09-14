#include "render.h"
#include "light.h"

#include "screen.h"
#include "texture.h"

#include <math.h>

// one column of the wall, read down one column of the texture
static void draw_wall_column(int x, int top, int height, int tex_x,
	double warm, int dark, int kind)
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
	if (bottom > view_height)
		bottom = view_height;

	for (; y < bottom; y++) {
		unsigned int color = wall_texture[kind][((int)tex_y & TEX_MASK) * TEX_SIZE + tex_x];
		double lit = dark ? warm * SIDE_LIGHT : warm;
		// and the height: under the vault it is dark, at strip
		// height it is day. without this the wall is a flat wash.
		lit *= at_height(tex_y / TEX_SIZE);
		if (lit < 0.0)
			lit = 0.0;
		view[y * view_width + x] = tint(color, lit, GLOOM);
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

	for (int y = HORIZON + 1; y < view_height; y++) {
		// how far the ground under this row is: a row one pixel below
		// the horizon is very far, the bottom row is right at our feet,
		// and the eye is half a wall above the floor
		double distance = (double)view_height / (2 * y - view_height);

		// where that row starts on the floor, and what one pixel to the
		// right is worth. the camera plane spans from -plane to +plane,
		// so the whole row is two planes wide
		double step_x = distance * 2.0 * player->plane_x / view_width;
		double step_y = distance * 2.0 * player->plane_y / view_width;
		double ground_x = player->x + distance * left_x;
		double ground_y = player->y + distance * left_y;
		// the floor takes what its square takes: that is what
		// lays the pools of light on the ground
		double base = CARRIED * lamp(distance);

		for (int x = 0; x < view_width; x++) {
			int tex_x = (int)(ground_x * TEX_SIZE) & TEX_MASK;
			int tex_y = (int)(ground_y * TEX_SIZE) & TEX_MASK;
			int at = tex_y * TEX_SIZE + tex_x;

			double light = base + lit_at(ground_x, ground_y);
			if (light > 1.0)
				light = 1.0;
			int painted = stencil_at((int)ground_x, (int)ground_y);
			// the badge shines with a light of its own: it is seen
			// from the far end of the hold
			int on_badge = is_badge((int)ground_x, (int)ground_y);
			unsigned int ground = on_badge ? badge_tile(tex_x, tex_y)
				: painted >= 0 ? stencil_tile(tex_x, tex_y, painted)
				: floor_texture[at];
			view[y * view_width + x] = tint(ground, on_badge ? 1.0 : light, GLOOM);
			view[(view_height - 1 - y) * view_width + x] =
				tint(ceiling_texture[at], light * CEILING_PART, GLOOM);

			ground_x += step_x;
			ground_y += step_y;
		}
	}
}

// the door opens in two, and in the middle of its square: the ray passes
// between the leaves, and that is how one sees it open instead of vanish
static int door_leaf(const struct player *player, double ray_x, double ray_y,
	int mx, int my, double *distance, double *along)
{
	// which way does the wall that holds the door run? that is what says
	// which plane the threshold lies in
	int north_south = is_wall(mx, my - 1) && is_wall(mx, my + 1);
	double toward = north_south ? ray_x : ray_y;
	if (toward == 0.0)
		return 0;                     // the ray runs along the threshold

	double d = north_south ? (mx + 0.5 - player->x) / ray_x
			    : (my + 0.5 - player->y) / ray_y;
	if (d <= 0.0)
		return 0;
	double u = north_south ? player->y + d * ray_y - my
			    : player->x + d * ray_x - mx;
	if (u < 0.0 || u > 1.0)
		return 0;                     // it passes beside the threshold

	double gap = u - 0.5;
	double half_open = door_at(mx, my) / 2.0;
	if (gap < half_open && -gap < half_open)
		return 0;                     // it passes between the two leaves

	*distance = d;
	// the leaf has slid: the texture slides with it
	*along = gap < 0.0 ? u + half_open : u - half_open;
	return 1;
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
	while (1) {
		if (side_x < side_y) {
			side_x += delta_x;
			map_x += step_x;
			side = 0;
		} else {
			side_y += delta_y;
			map_y += step_y;
			side = 1;
		}

		// a door does not stop a whole square: we ask the
		// leaf itself, and the ray carries on if it is open
		if (is_door(map_x, map_y)) {
			double d, along;
			if (!door_leaf(player, ray_x, ray_y, map_x, map_y,
				       &d, &along))
				continue;
			int right = is_wall(map_x, map_y - 1)
				&& is_wall(map_x, map_y + 1);
			struct hit door = {
				.distance = d < NEAR_CLIP ? NEAR_CLIP : d,
				.wall_x = along, .side = right ? 0 : 1,
				.kind = DOOR_TEXTURE };
			return door;
		}
		if (is_wall(map_x, map_y))
			break;
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
	for (int x = 0; x < view_width; x++) {
		// -1 on the left edge of the screen, +1 on the right edge
		double camera = 2.0 * x / view_width - 1.0;
		double ray_x = player->dir_x + player->plane_x * camera;
		double ray_y = player->dir_y + player->plane_y * camera;
		struct hit hit = cast_ray(player, ray_x, ray_y);

		int height = (int)(view_height / hit.distance);
		int top = HORIZON - height / 2;

		int tex_x = (int)(hit.wall_x * TEX_SIZE);
		// the two faces we can see of the same wall must not be mirror
		// images of each other
		if ((hit.side == 0 && ray_x > 0) || (hit.side == 1 && ray_y < 0))
			tex_x = TEX_SIZE - 1 - tex_x;

		// how square this wall is to us: a north-south wall met by a ray
		// going mostly east is seen head on, and catches the light
		double facing = hit.side == 0 ? ray_x : ray_y;
		double length = sqrt(ray_x * ray_x + ray_y * ray_y);
		if (length > 0.0)
			facing /= length;
		// this wall takes what the square we look at it from
		// takes, at the point we hit, plus the glow we carry
		double on_wall = lit_at(player->x + ray_x * hit.distance * 0.94,
				       player->y + ray_y * hit.distance * 0.94);
		double lit = on_wall + CARRIED * lamp(hit.distance);
		if (lit > 1.0)
			lit = 1.0;
		// a wall facing us takes the neon, a wall seen at an angle
		// far less
		double face = LAMP_AMBIENT + (1.0 - LAMP_AMBIENT)
			* (facing < 0 ? -facing : facing);
		draw_wall_column(x, top, height, tex_x, lit * face,
			hit.side == 1, hit.kind);
	}
}

// the same pixel, but blended with what is already there. `part` says how
// much of the new colour wins, from 0 to 1.
static void blend(int x, int y, unsigned int color, double part)
{
	if (x < 0 || x >= view_width || y < 0 || y >= view_height)
		return;
	unsigned int under = view[y * view_width + x];
	unsigned int out = 0;
	for (int d = 0; d <= RED_SHIFT; d += GREEN_SHIFT) {
		double a = (under >> d) & CHANNEL, b = (color >> d) & CHANNEL;
		out |= (unsigned int)(a + (b - a) * part) << d;
	}
	view[y * view_width + x] = out;
}

// one pixel, if it is on the screen at all. the heading line runs off the
// map, and until now it wrote wherever that landed in memory
static void put_pixel(int x, int y, unsigned int color)
{
	if (x >= 0 && x < view_width && y >= 0 && y < view_height)
		view[y * view_width + x] = color;
}

// the same map, now small enough to live in a corner
void render_map(const struct player *player, int cell, int left, int top)
{
	// the frame is always there, even over what has not been seen: without it
	// nothing says how big the level is
	int l = map_width * cell, h = map_height * cell;
	for (int i = -MAP_EDGE; i < l + MAP_EDGE; i++)
		for (int e = 1; e <= MAP_EDGE; e++) {
			put_pixel(left + i, top - e, MAP_FRAME);
			put_pixel(left + i, top + h + e - 1, MAP_FRAME);
		}
	for (int j = -MAP_EDGE; j < h + MAP_EDGE; j++)
		for (int e = 1; e <= MAP_EDGE; e++) {
			put_pixel(left - e, top + j, MAP_FRAME);
			put_pixel(left + l + e - 1, top + j, MAP_FRAME);
		}

	// what we do not know is dark, not missing, and the
	// whole thing is translucent over the world
	for (int j = 0; j < h; j++)
		for (int i = 0; i < l; i++)
			blend(left + i, top + j, MAP_UNSEEN, MAP_ALPHA);

	for (int y = 0; y < map_height; y++)
		for (int x = 0; x < map_width; x++) {
			// what we have not walked past is not on the map yet
			if (!is_seen(x, y))
				continue;
			unsigned int color = is_door(x, y) ? MAP_DOOR
				: is_wall(x, y) ? MAP_WALL : MAP_FLOOR;
			for (int line = 0; line < cell; line++)
				for (int column = 0; column < cell; column++)
					blend(left + x * cell + column,
						top + y * cell + line, color, MAP_ALPHA);
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
