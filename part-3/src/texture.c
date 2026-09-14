#include "texture.h"

unsigned int wall_texture[TEX_SIZE * TEX_SIZE];
unsigned int floor_texture[TEX_SIZE * TEX_SIZE];
unsigned int ceiling_texture[TEX_SIZE * TEX_SIZE];

// the same square always gets the same number between 0 and 1: a brick keeps
// its shade from one frame to the next, and the wall does not crawl. the three
// constants are arbitrary large odd numbers, any others would do as well
double noise(int x, int y)
{
	unsigned int n = (unsigned int)(x * 374761393 + y * 668265263);
	n = (n ^ (n >> 13)) * 1274126177u;
	return (double)((n >> 16) & 0xffff) / 65535.0;
}

unsigned int shade(unsigned int color, double light)
{
	if (light > 1.0)
		light = 1.0;
	if (light < 0.0)
		light = 0.0;

	unsigned int red = (unsigned int)(((color >> 16) & 0xff) * light);
	unsigned int green = (unsigned int)(((color >> 8) & 0xff) * light);
	unsigned int blue = (unsigned int)((color & 0xff) * light);
	return (red << 16) | (green << 8) | blue;
}

// bricks: rows half a brick apart, a mortar line between them, and each
// brick a shade of its own. drawn once, read a million times
void make_wall_texture(void)
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
void make_floor_texture(void)
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
void make_ceiling_texture(void)
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
