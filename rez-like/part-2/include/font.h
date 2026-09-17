#ifndef FONT_H
#define FONT_H

#include "draw.h"

// numbers and capitals drawn as strokes, in the same light as everything
// else. each glyph lives in a box three wide and five tall, and the same
// strokes can be written flat on the screen or laid in the world
#define GLYPH_W  3.0
#define GLYPH_H  5.0
// a letter outside ascii comes as two bytes of utf-8. the first one only
// announces it, the second names the glyph in the table
#define UTF8_LEAD  '\xc3'
// how many sparks a bursting stroke throws, and how fast
#define STROKE_SPARKS  3
#define STROKE_SPEED   2.5

// what is done with each stroke of a text, in glyph grid pixels from the
// top left, y going down. index counts the strokes of the whole text
typedef void (*stroke_fn)(double x0, double y0, double x1, double y1, int index,
			  int total, void *data);
void font_walk(double x, double y, const char *text, double size, stroke_fn fn,
	       void *data);
int font_strokes(const char *text);
// writes a string at x, y (top left), each glyph unit being `size` pixels
void font_write(double x, double y, const char *text, double size, struct light colour);
// the same, with only this fraction of the strokes there, the last one
// partly, so that a line can write itself
void font_write_drawn(double x, double y, const char *text, double size,
		      struct light colour, double drawn);
// how wide a string comes out, in pixels
double font_width(const char *text, double size);
// writes in the world, on the plane of origin (the top left), right and up,
// a glyph unit being size world units. only this fraction of the strokes is
// there, the last one partly, so that a title can draw itself
void font_write_3d(const struct camera *cam, struct vec origin, struct vec right,
		   struct vec up, const char *text, double size, struct light colour,
		   double drawn);
// the strokes burst into sparks, on the same plane
void font_burst_3d(struct vec origin, struct vec right, struct vec up, const char *text,
		   double size, struct light colour);

#endif
