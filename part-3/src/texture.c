#include "texture.h"

#include "render.h"

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

// noise in patches, not per pixel: one draw per pixel gives salt and pepper.
// we draw one value every `cell` pixels and slide between them
static double smooth_noise(int x, int y, int cell)
{
	int cells = TEX_SIZE / cell;
	int gx = x / cell, gy = y / cell;
	double tx = (double)(x % cell) / cell;
	double ty = (double)(y % cell) / cell;
	// a curve that leaves and lands flat: without it the grid shows
	tx = tx * tx * (3.0 - 2.0 * tx);
	ty = ty * ty * (3.0 - 2.0 * ty);
	int dx = (gx + 1) % cells, dy = (gy + 1) % cells;
	double top = noise(gx, gy) * (1.0 - tx) + noise(dx, gy) * tx;
	double bottom = noise(gx, dy) * (1.0 - tx) + noise(dx, dy) * tx;
	return top * (1.0 - ty) + bottom * ty;
}

unsigned int mix(unsigned int a, unsigned int b, double part)
{
	if (part < 0.0)
		part = 0.0;
	if (part > 1.0)
		part = 1.0;
	unsigned int out = 0;
	for (int d = 0; d <= RED_SHIFT; d += GREEN_SHIFT) {
		double ca = (a >> d) & CHANNEL, cb = (b >> d) & CHANNEL;
		out |= (unsigned int)(ca + (cb - ca) * part) << d;
	}
	return out;
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

// above zero toward the light, below toward the shadow: one grey lightened
// and darkened gives cardboard, two colours give relief
static unsigned int relief(const struct plate *p, double v)
{
	return v > 0.0 ? mix(p->color, p->light, v) : mix(p->color, p->dark, -v);
}

// sheet metal: a panel, its seam, its bevel, its rivets. every surface in
// the level comes out of here, with a panel size and four colours
static void make_plating(unsigned int *out, const struct plate *p)
{
	int wide = TEX_SIZE / p->across, high = TEX_SIZE / p->down;

	for (int y = 0; y < TEX_SIZE; y++) {
		for (int x = 0; x < TEX_SIZE; x++) {
			int dx = x % wide, dy = y % high;

			// the seam is hollow, and the panel above casts its
			// shadow into it. without it the panels float
			if (dx < SEAM_W || dy < SEAM_W) {
				double hollow = (dx == SEAM_W - 1 || dy == SEAM_W - 1)
					? 0.15 : 0.70;
				out[y * TEX_SIZE + x] = mix(p->seam, p->dark, hollow);
				continue;
			}

			// the light comes from the ceiling: the top of the
			// panel takes it, the bottom much less
			double v = PLATE_SHEEN * (0.5 - (double)dy / high);
			v += PLATE_GRAIN * (smooth_noise(x, y, GRAIN_CELL) - 0.5);

			// the bevel: the edge in the light at the top and the
			// left, in shadow at the bottom and the right
			if (dx == SEAM_W || dy == SEAM_W)
				v += PLATE_EDGE;
			else if (dx == wide - 1 || dy == high - 1)
				v -= PLATE_EDGE;

			// four rivets to a panel, each with its own lit side:
			// that is what makes one believe the metal
			if (p->rivets) {
				int rx = (dx < wide / 2 ? dx : wide - 1 - dx) - RIVET_IN;
				int ry = (dy < high / 2 ? dy : high - 1 - dy) - RIVET_IN;
				if (rx * rx + ry * ry <= 2)
					v += rx + ry < 0 ? 0.60 : -0.40;
			}
			out[y * TEX_SIZE + x] = relief(p, v);
		}
	}
}

// the plates of the station: four colours and two numbers each, and that is
// all that separates the gangway from the deck underfoot
static const struct plate HULL = {
	.color = 0x424b59, .light = 0x9aa9bb, .dark = 0x141821,
	.seam = 0x0d1016, .across = 2, .down = 2, .rivets = 1,
};
static const struct plate DECK = {
	.color = 0x2f3540, .light = 0x6d7b8c, .dark = 0x0f1218,
	.seam = 0x090b10, .across = 4, .down = 4, .rivets = 0,
};
static const struct plate ROOF = {
	.color = 0x232833, .light = 0x525d6d, .dark = 0x080a0e,
	.seam = 0x06080b, .across = 2, .down = 2, .rivets = 0,
};

// the ordinary wall of the gangway
void make_wall_texture(void)
{
	make_plating(wall_texture, &HULL);
}

// the floor: dark tiles, tight, no rivets
void make_floor_texture(void)
{
	make_plating(floor_texture, &DECK);
}

// the ceiling: nearly black, and that is on purpose
void make_ceiling_texture(void)
{
	make_plating(ceiling_texture, &ROOF);
}
