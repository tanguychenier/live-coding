#include <ctype.h>
#include <string.h>

#include "font.h"
#include "particle.h"

// a glyph is a list of strokes on a grid three wide and five tall. each
// stroke is four digits, x0 y0 x1 y1, and a space separates strokes. the
// grid has y going down, like the screen. a slash and a dot stand for the
// rows above the grid, one and two up, where an accent goes
struct glyph {
	char letter;
	const char *strokes;
};

static const struct glyph GLYPHS[] = {
	{ '0', "0004 0424 2420 2000 0024" },
	{ '1', "1014 0111" },
	{ '2', "0020 2022 2202 0204 0424" },
	{ '3', "0020 2024 2404 1222" },
	{ '4', "0002 0222 2024" },
	{ '5', "2000 0002 0222 2224 2404" },
	{ '6', "2000 0004 0424 2422 2202" },
	{ '7', "0020 2004" },
	{ '8', "0020 0004 2024 0424 0222" },
	{ '9', "2000 0002 0222 2024" },
	{ 'A', "0401 0110 1021 2124 0222" },
	{ 'B', "0004 0020 2022 2202 0222 2224 2404" },
	{ 'C', "2000 0004 0424" },
	{ 'D', "0004 0010 1031 3133 3314 1404" },
	{ 'E', "2000 0004 0424 0212" },
	{ '\x89', "2000 0004 0424 0212 1/2." },
	{ 'F', "2000 0004 0212" },
	{ 'G', "2000 0004 0424 2422 2212" },
	{ 'H', "0004 2024 0222" },
	{ 'I', "0020 1014 0424" },
	{ 'J', "2022 2214 1404" },
	{ 'K', "0004 2002 0224" },
	{ 'L', "0004 0424" },
	{ 'M', "0400 0012 1220 2024" },
	{ 'N', "0400 0024 2420" },
	{ 'O', "0020 2024 2404 0400" },
	{ 'P', "0400 0020 2022 2202" },
	{ 'Q', "0020 2024 2404 0400 1324" },
	{ 'R', "0400 0020 2022 2202 0224" },
	{ 'S', "2000 0002 0222 2224 2404" },
	{ 'T', "0020 1014" },
	{ 'U', "0004 0424 2420" },
	{ 'V', "0014 1420" },
	{ 'W', "0004 0412 1224 2420" },
	{ 'X', "0024 2004" },
	{ 'Y', "0012 2012 1214" },
	{ 'Z', "0020 2004 0424" },
	{ '-', "0222" },
	{ '.', "1414" },
	{ '/', "0420" },
	{ 0, "" },
};

static const char *strokes_of(char letter)
{
	letter = (char)toupper((unsigned char)letter);
	for (int i = 0; GLYPHS[i].letter; i++)
		if (GLYPHS[i].letter == letter)
			return GLYPHS[i].strokes;
	return "";
}

// the next stroke of a glyph, or 0 when there is none left
static const char *next_stroke(const char *stroke, double *x0, double *y0, double *x1, double *y1)
{
	while (*stroke == ' ')
		stroke++;
	if (strlen(stroke) < 4)
		return NULL;
	*x0 = stroke[0] - '0';
	*y0 = stroke[1] - '0';
	*x1 = stroke[2] - '0';
	*y1 = stroke[3] - '0';
	return stroke + 4;
}

int font_strokes(const char *text)
{
	int count = 0;
	for (; *text; text++) {
		if (*text == UTF8_LEAD)
			continue;
		const char *stroke = strokes_of(*text);
		double x0, y0, x1, y1;
		while ((stroke = next_stroke(stroke, &x0, &y0, &x1, &y1)))
			count++;
	}
	return count;
}

void font_walk(double x, double y, const char *text, double size, stroke_fn fn,
	       void *data)
{
	int total = font_strokes(text), index = 0;
	for (; *text; text++) {
		if (*text == UTF8_LEAD)
			continue;
		const char *stroke = strokes_of(*text);
		double x0, y0, x1, y1;
		while ((stroke = next_stroke(stroke, &x0, &y0, &x1, &y1)))
			fn(x + x0 * size, y + y0 * size, x + x1 * size, y + y1 * size,
			   index++, total, data);
		x += (GLYPH_W + 1.0) * size;
	}
}

// a text on the screen, and how much of it is drawn
struct flat {
	struct light colour;
	double drawn;
};

static void flat_stroke(double x0, double y0, double x1, double y1, int index,
			int total, void *data)
{
	const struct flat *flat = data;
	// strokes are drawn in order, the one at the front partly
	double front = flat->drawn * total;
	if (index >= front)
		return;
	double part = front - index;
	if (part > 1.0)
		part = 1.0;
	draw_line_2d(x0, y0, x0 + (x1 - x0) * part, y0 + (y1 - y0) * part, flat->colour);
}

void font_write_drawn(double x, double y, const char *text, double size,
		      struct light colour, double drawn)
{
	struct flat flat = { colour, drawn };
	font_walk(x, y, text, size, flat_stroke, &flat);
}

void font_write(double x, double y, const char *text, double size, struct light colour)
{
	font_write_drawn(x, y, text, size, colour, 1.0);
}

// how many glyphs a text has, its lead bytes not counted
static int glyphs_in(const char *text)
{
	int count = 0;
	for (; *text; text++)
		if (*text != UTF8_LEAD)
			count++;
	return count;
}

double font_width(const char *text, double size)
{
	return (double)glyphs_in(text) * (GLYPH_W + 1.0) * size - size;
}

// the plane a text lies on in the world, and how much of it is drawn
struct plane {
	const struct camera *cam;
	struct vec origin, right, up;
	struct light colour;
	double drawn;
};

static struct vec on_plane(const struct plane *plane, double x, double y)
{
	// the grid has y going down, the world's up goes up
	return add(plane->origin, sub(scale(plane->right, x), scale(plane->up, y)));
}

static void world_stroke(double x0, double y0, double x1, double y1, int index,
			 int total, void *data)
{
	const struct plane *plane = data;
	// strokes are drawn in order, the one at the front partly
	double front = plane->drawn * total;
	if (index >= front)
		return;
	double part = front - index;
	if (part > 1.0)
		part = 1.0;
	struct vec from = on_plane(plane, x0, y0);
	struct vec to = on_plane(plane, x0 + (x1 - x0) * part, y0 + (y1 - y0) * part);
	draw_line(plane->cam, from, to, plane->colour);
}

void font_write_3d(const struct camera *cam, struct vec origin, struct vec right,
		   struct vec up, const char *text, double size, struct light colour,
		   double drawn)
{
	struct plane plane = { cam, origin, right, up, colour, drawn };
	font_walk(0.0, 0.0, text, size, world_stroke, &plane);
}

static void burst_stroke(double x0, double y0, double x1, double y1, int index,
			 int total, void *data)
{
	const struct plane *plane = data;
	(void)index;
	(void)total;
	for (int i = 0; i < STROKE_SPARKS; i++) {
		double part = (i + 0.5) / STROKE_SPARKS;
		struct vec at = on_plane(plane, x0 + (x1 - x0) * part, y0 + (y1 - y0) * part);
		particles_burst(at, 1, STROKE_SPEED, plane->colour);
	}
}

void font_burst_3d(struct vec origin, struct vec right, struct vec up, const char *text,
		   double size, struct light colour)
{
	struct plane plane = { NULL, origin, right, up, colour, 1.0 };
	font_walk(0.0, 0.0, text, size, burst_stroke, &plane);
}
