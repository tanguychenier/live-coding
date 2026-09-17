#ifndef DRAW_H
#define DRAW_H

#include "screen.h"
#include "vec.h"

// how the eye sees. three unit axes square to each other, and the focal,
// how many pixels one unit of width is worth at one unit of depth
struct camera {
	struct vec eye, forward, right, up;
	double focal;
};

// the field of view, a focal length as a fraction of the picture's width.
// 0.656 of 640 is 420 pixels, a little under ninety degrees across
#define FOCAL_OF_WIDTH 0.65625
#define FOCAL          (FOCAL_OF_WIDTH * view_width)
// nothing nearer than this is drawn, a line crossing it is cut there
#define NEAR_PLANE     0.05
// a line one pixel wide still lights its two neighbours. half width 1.24
// gives them 0.35 with the profile below, the softness that keeps a
// diagonal from looking like a staircase, in pixels of the screen
#define LINE_SOFT      1.24
// nearer than this depth a line grows thicker, up to this many pixels of
// half width at the base size. the pilot is drawn with the thickest lines
#define THICK_DEPTH    3.5
#define THICK_MAX      1.8
// the strokes of the flat picture, the sight and the numbers, this many
// half pixels at the base size and more on a bigger picture
#define FLAT_WIDTH     0.9
// a filled face is this much of its edges' light, a tint and not a wall
#define FACE_LIGHT     0.16
// a point of light never grows past this many pixels of radius at the base
// size, a spark passing the eye must not fill the screen
#define POINT_RADIUS_MAX 8.0
// the soft shoulder of the tone curve. light past one goes white gently
#define TONE_KNEE      1.6

// a colour in floating point, so that light can add up past white before
// it is clamped
struct light {
	double r, g, b;
};

static inline struct light light(double r, double g, double b)
{
	return (struct light){ r, g, b };
}

static inline struct light light_scale(struct light lit, double gain)
{
	return light(lit.r * gain, lit.g * gain, lit.b * gain);
}

static inline struct light light_mix(struct light from, struct light to, double third)
{
	return light(from.r + (to.r - from.r) * third, from.g + (to.g - from.g) * third,
		     from.b + (to.b - from.b) * third);
}

// the colour wheel, a hue from zero to one. never fully saturated, a pure
// primary reads as a computer and not as light
struct light light_hue(double hue);

// the picture takes a new size. everything drawn after is that big
int draw_resize(int width, int height);
// how much bigger than the base size the picture is, for the sight, which
// is laid out in base pixels
double draw_scale(void);

// the frame begins as a gradient, top to bottom, the sky of the zone
void draw_clear(struct light top, struct light bottom);
// the fog of the frame. at this depth a line keeps a third of its light
void draw_fog(double depth);
void draw_line(const struct camera *cam, struct vec from, struct vec to,
	       struct light colour);
// a flat face, fogged like a line, faint like glass
void draw_triangle(const struct camera *cam, struct vec first, struct vec second,
		   struct vec third, struct light colour);
void draw_point(const struct camera *cam, struct vec point, struct light colour,
		double size);
// the tone curve, and the picture is done
void draw_finish(void);
// a line in screen pixels, for the sight. it sits on the near plane, in
// front of everything, and the fog leaves it alone
void draw_line_2d(double x0, double y0, double x1, double y1, struct light colour);
// where a point of the world lands on the screen, or 0 if it is behind the eye
int draw_project(const struct camera *cam, struct vec point, double *x, double *y,
		 double *z);
void camera_look(struct camera *cam, struct vec eye, struct vec at,
		 struct vec up, double roll, double focal);

#endif
