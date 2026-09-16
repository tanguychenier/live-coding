#include "light.h"
#include "world.h"

#include <math.h>
#include <stdlib.h>

// what the neons are worth at this instant, around 1, and what a failing
// tube is worth, which is another matter
static double neon = 1.0;
static double fault = 1.0;

// two waves at unrelated speeds never repeat: the eye stops finding a
// pattern in them.
void lamp_flicker(double seconds)
{
	neon = 1.0 + LAMP_HUM * (sin(seconds * LAMP_HZ)
				 + 0.6 * sin(seconds * LAMP_HZ * 2.37));

	// the failing tube does not modulate, it cuts: three unrelated
	// waves, two thresholds, and the wobble seems to give up.
	double wobble = sin(seconds * 13.7) + sin(seconds * 4.3)
		+ sin(seconds * 31.1) * 0.5;
	fault = wobble > 1.15 ? FAULT_LOW
		: wobble > 0.55 ? FAULT_HALF : neon;
}

// the light falls with the square of the distance, like the real thing, and
// stops at LAMP_REACH: beyond it lies the dark, not a grey wall
double lamp(double distance)
{
	double part = distance / LAMP_REACH;
	double light = neon / (1.0 + 4.0 * part * part);

	if (part >= 1.0)
		return LAMP_FLOOR;
	// the last quarter fades out, or the edge of the light is a hard
	// ring
	if (part > 0.75)
		light *= (1.0 - part) * 4.0;
	if (light < LAMP_FLOOR)
		light = LAMP_FLOOR;
	if (light > 1.0)
		light = 1.0;
	return light;
}

// `v` runs from 0 at the top of the wall to 1 at its foot. the most light
// is at strip height, and it falls away on both sides.
double at_height(double v)
{
	double gap = v - LAMP_HEIGHT;
	double loss = gap < 0.0 ? UNDER_ROOF * -gap : AT_THE_FOOT * gap;
	return 1.0 - loss;
}

// what each square takes, worked out once at load time: rendering only has
// to read an array.
static double *bright;
// and what the failing tubes give, kept apart: they have a life of their own
static double *faulty;

static int sees(int x1, int y1, int x2, int y2)
{
	// a wall between the two, and the strip lights nothing
	double dx = x2 - x1, dy = y2 - y1;
	double n = fabs(dx) > fabs(dy) ? fabs(dx) : fabs(dy);
	if (n < 1.0)
		return 1;
	for (double i = 1.0; i < n; i += 1.0) {
		int x = (int)(x1 + dx * i / n + 0.5);
		int y = (int)(y1 + dy * i / n + 0.5);
		if (is_wall(x, y) && !is_door(x, y))
			return 0;
	}
	return 1;
}

void light_map(void)
{
	free(bright);
	free(faulty);
	bright = calloc((size_t)map_width * map_height, sizeof(*bright));
	faulty = calloc((size_t)map_width * map_height, sizeof(*faulty));
	if (!bright || !faulty)
		return;
	// the base light of the ceiling strips, everywhere at once
	for (int i = 0; i < map_width * map_height; i++)
		bright[i] = STRIP_LIGHT;
	for (int y = 0; y < map_height; y++)
		for (int x = 0; x < map_width; x++) {
			if (!is_lamp(x, y))
				continue;
			for (int j = y - LAMP_ON_WALL; j <= y + LAMP_ON_WALL; j++)
				for (int i = x - LAMP_ON_WALL; i <= x + LAMP_ON_WALL; i++) {
					if (i < 0 || j < 0 || i >= map_width || j >= map_height)
						continue;
					double d = hypot(i - x, j - y);
					if (d > LAMP_ON_WALL || !sees(x, y, i, j))
						continue;
					double part = d / LAMP_ON_WALL;
					double received = 1.0 / (1.0 + 3.0 * part * part)
						* (1.0 - part);
					if (lamp_faulty(x, y))
						faulty[j * map_width + i] += received;
					else
						bright[j * map_width + i] += received;
				}
		}
}

// what one square takes from the strips of the level
double lit_here(int x, int y)
{
	if (!bright || x < 0 || y < 0 || x >= map_width || y >= map_height)
		return 0.0;
	double v = bright[y * map_width + x] * neon
		+ faulty[y * map_width + x] * fault;
	return v > 1.0 ? 1.0 : v;
}

// light does not stop at the edge of a square: we read between the four
// neighbours, or the floor and the walls get hard rectangles
double lit_at(double x, double y)
{
	double fx = x - 0.5, fy = y - 0.5;
	int x0 = (int)floor(fx), y0 = (int)floor(fy);
	double ax = fx - x0, ay = fy - y0;
	double a = lit_here(x0, y0), b = lit_here(x0 + 1, y0);
	double c = lit_here(x0, y0 + 1), d = lit_here(x0 + 1, y0 + 1);
	double top = a + (b - a) * ax;
	double bottom = c + (d - c) * ax;
	return top + (bottom - top) * ay;
}
