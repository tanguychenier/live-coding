#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"

unsigned int *view;
int view_width, view_height;

// light adds up in these, one per channel, nothing clamped before the end
// of the frame. a hundred faint lines crossing make a bright spot, the way
// real light does
static float *red, *green, *blue;
// how much bigger than the base size the picture is
static double scale_up = 1.0;
// once the frame is finished, what is drawn goes straight into the picture,
// sharp, without the bloom. that is how the numbers stay readable
static int finished;
static double fog_depth = 38.0;

struct light light_hue(double hue)
{
	hue = hue - floor(hue);
	double r = fabs(hue * 6.0 - 3.0) - 1.0;
	double g = 2.0 - fabs(hue * 6.0 - 2.0);
	double b = 2.0 - fabs(hue * 6.0 - 4.0);
	r = r < 0 ? 0 : r > 1 ? 1 : r;
	g = g < 0 ? 0 : g > 1 ? 1 : g;
	b = b < 0 ? 0 : b > 1 ? 1 : b;
	// a quarter of white under every hue
	const double floor_white = 0.25;
	return light(floor_white + (1.0 - floor_white) * r,
		     floor_white + (1.0 - floor_white) * g,
		     floor_white + (1.0 - floor_white) * b);
}

double draw_scale(void)
{
	return scale_up;
}

// the picture, as big as the window, made anew when the window changes
int draw_resize(int width, int height)
{
	if (width == view_width && height == view_height && view)
		return 1;
	free(view);
	free(red);
	free(green);
	free(blue);
	view_width = width;
	view_height = height;
	scale_up = (double)height / VIEW_BASE_HEIGHT;
	size_t pixels = (size_t)width * height;
	view = calloc(pixels, sizeof *view);
	red = calloc(pixels, sizeof *red);
	green = calloc(pixels, sizeof *green);
	blue = calloc(pixels, sizeof *blue);
	return view && red && green && blue;
}

// the frame starts dark, and the sky, a gradient from top to bottom, is
// added at the end, it is the same on every pixel of a row
static struct light sky_top, sky_bottom;

void draw_clear(struct light top, struct light bottom)
{
	finished = 0;
	sky_top = top;
	sky_bottom = bottom;
}

void draw_fog(double depth)
{
	fog_depth = depth;
}

// the camera is built from where it is and what it looks at. the roll turns
// it around its own line of sight, which is what a rail does in a bend
void camera_look(struct camera *cam, struct vec eye, struct vec at,
		 struct vec up, double roll, double focal)
{
	cam->eye = eye;
	cam->forward = unit(sub(at, eye));
	cam->right = unit(cross(cam->forward, up));
	cam->up = cross(cam->right, cam->forward);
	double cs = cos(roll), sn = sin(roll);
	struct vec new_right = add(scale(cam->right, cs), scale(cam->up, sn));
	struct vec new_up = sub(scale(cam->up, cs), scale(cam->right, sn));
	cam->right = new_right;
	cam->up = new_up;
	cam->focal = focal;
}

// a point of the world seen from the eye, x to the right, y up, z ahead
static struct vec to_camera(const struct camera *cam, struct vec point)
{
	struct vec delta = sub(point, cam->eye);
	return vec(dot(delta, cam->right), dot(delta, cam->up), dot(delta, cam->forward));
}

int draw_project(const struct camera *cam, struct vec point, double *x, double *y,
		 double *z)
{
	struct vec seen = to_camera(cam, point);
	if (seen.z < NEAR_PLANE)
		return 0;
	*x = view_width / 2.0 + cam->focal * seen.x / seen.z;
	*y = view_height / 2.0 - cam->focal * seen.y / seen.z;
	*z = seen.z;
	return 1;
}

// the fog keeps a third of the light at the fog depth, and takes the rest by
// twice that. it is what gives a wireframe its depth without any shading
static double fog(double z)
{
	return exp(-z / fog_depth);
}

static void plot(int x, int y, struct light lit, double gain)
{
	if (x < 0 || y < 0 || x >= view_width || y >= view_height)
		return;
	size_t i = (size_t)y * view_width + x;
	if (finished) {
		unsigned int packed = view[i];
		int r = (int)((packed >> 16) & 0xff) + (int)(lit.r * gain * 255.0);
		int g = (int)((packed >> 8) & 0xff) + (int)(lit.g * gain * 255.0);
		int b = (int)(packed & 0xff) + (int)(lit.b * gain * 255.0);
		view[i] = rgb(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
		return;
	}
	red[i] += (float)(lit.r * gain);
	green[i] += (float)(lit.g * gain);
	blue[i] += (float)(lit.b * gain);
}

// how wide a line is at this depth, in half pixels. close lines are thick,
// far lines are one pixel with soft edges
static double half_width(double z)
{
	double thick_max = THICK_MAX * scale_up;
	double w = thick_max * THICK_DEPTH / (z > NEAR_PLANE ? z : NEAR_PLANE);
	return w < LINE_SOFT ? LINE_SOFT : w > thick_max ? thick_max : w;
}

// the line is walked one pixel at a time along its longer axis, each step
// lighting a run of pixels across, brightest in the middle
static void line_2d(double x0, double y0, double z0, double x1, double y1,
		    double z1, struct light lit, double lit_by)
{
	double dx = x1 - x0, dy = y1 - y0;
	int steps = (int)(fabs(dx) > fabs(dy) ? fabs(dx) : fabs(dy)) + 1;
	int across_x = fabs(dx) > fabs(dy) ? 0 : 1;
	int across_y = 1 - across_x;
	for (int s = 0; s <= steps; s++) {
		double t = (double)s / steps;
		double z = z0 + (z1 - z0) * t;
		double gain = fog(z) * lit_by;
		double hw = half_width(z);
		int reach = (int)ceil(hw) - 1;
		double fx = x0 + dx * t, fy = y0 + dy * t;
		int x = (int)floor(fx + 0.5), y = (int)floor(fy + 0.5);
		// the distance of the pixel centre to the true line, so that a
		// line moving slowly across the pixels does not flicker
		double off = across_x ? fx - x : fy - y;
		for (int d = -reach; d <= reach; d++) {
			double along = (d - off) / hw;
			double weight = 1.0 - along * along;
			if (weight > 0.0)
				plot(x + d * across_x, y + d * across_y, lit, gain * weight);
		}
	}
}

void draw_line(const struct camera *cam, struct vec from, struct vec to,
	       struct light colour)
{
	struct vec start = to_camera(cam, from), finish = to_camera(cam, to);
	// behind the eye, nothing to see. crossing the near plane, the line is
	// cut where it crosses, and only the part in front is drawn
	if (start.z < NEAR_PLANE && finish.z < NEAR_PLANE)
		return;
	if (start.z < NEAR_PLANE)
		start = mix(start, finish, (NEAR_PLANE - start.z) / (finish.z - start.z));
	if (finish.z < NEAR_PLANE)
		finish = mix(finish, start, (NEAR_PLANE - finish.z) / (start.z - finish.z));
	double x0 = view_width / 2.0 + cam->focal * start.x / start.z;
	double y0 = view_height / 2.0 - cam->focal * start.y / start.z;
	double x1 = view_width / 2.0 + cam->focal * finish.x / finish.z;
	double y1 = view_height / 2.0 - cam->focal * finish.y / finish.z;
	// a line entirely off screen costs nothing more than this test
	if ((x0 < 0 && x1 < 0) || (y0 < 0 && y1 < 0)
	    || (x0 >= view_width && x1 >= view_width)
	    || (y0 >= view_height && y1 >= view_height))
		return;
	line_2d(x0, y0, start.z, x1, y1, finish.z, colour, 1.0);
}

// the flat picture is never dimmed and its strokes grow with the picture,
// so that the numbers keep their weight on a big screen
void draw_line_2d(double x0, double y0, double x1, double y1, struct light colour)
{
	double hw = FLAT_WIDTH * scale_up;
	if (hw < LINE_SOFT)
		hw = LINE_SOFT;
	double dx = x1 - x0, dy = y1 - y0;
	int steps = (int)(fabs(dx) > fabs(dy) ? fabs(dx) : fabs(dy)) + 1;
	int across_x = fabs(dx) > fabs(dy) ? 0 : 1;
	int across_y = 1 - across_x;
	int reach = (int)ceil(hw) - 1;
	for (int s = 0; s <= steps; s++) {
		double t = (double)s / steps;
		double fx = x0 + dx * t, fy = y0 + dy * t;
		int x = (int)floor(fx + 0.5), y = (int)floor(fy + 0.5);
		double off = across_x ? fx - x : fy - y;
		for (int d = -reach; d <= reach; d++) {
			double along = (d - off) / hw;
			double weight = 1.0 - along * along;
			if (weight > 0.0)
				plot(x + d * across_x, y + d * across_y, colour, weight);
		}
	}
}

// a flat face, filled one row at a time with the fog of its depth. faint on
// purpose, glass and not a wall, and it adds like everything else so the
// order of drawing does not matter
void draw_triangle(const struct camera *cam, struct vec first, struct vec second,
		   struct vec third, struct light colour)
{
	double x[3], y[3], z[3];
	if (!draw_project(cam, first, &x[0], &y[0], &z[0])
	    || !draw_project(cam, second, &x[1], &y[1], &z[1])
	    || !draw_project(cam, third, &x[2], &y[2], &z[2]))
		return;
	// sort the corners from top to bottom
	for (int i = 0; i < 2; i++)
		for (int j = i + 1; j < 3; j++)
			if (y[j] < y[i]) {
				double s;
				s = x[i]; x[i] = x[j]; x[j] = s;
				s = y[i]; y[i] = y[j]; y[j] = s;
				s = z[i]; z[i] = z[j]; z[j] = s;
			}
	int top = (int)ceil(y[0]), bottom = (int)floor(y[2]);
	if (top < 0)
		top = 0;
	if (bottom >= view_height)
		bottom = view_height - 1;
	if (x[0] < 0 && x[1] < 0 && x[2] < 0)
		return;
	if (x[0] >= view_width && x[1] >= view_width && x[2] >= view_width)
		return;
	struct light lit = light_scale(colour, FACE_LIGHT);
	for (int row = top; row <= bottom; row++) {
		// the long edge from the top corner to the bottom one, and the
		// short edge that changes at the middle corner
		double ka = (row - y[0]) / (y[2] - y[0] + 1e-9);
		double xa = x[0] + (x[2] - x[0]) * ka, za = z[0] + (z[2] - z[0]) * ka;
		double xb, zb;
		if (row < y[1]) {
			double kb = (row - y[0]) / (y[1] - y[0] + 1e-9);
			xb = x[0] + (x[1] - x[0]) * kb;
			zb = z[0] + (z[1] - z[0]) * kb;
		} else {
			double kb = (row - y[1]) / (y[2] - y[1] + 1e-9);
			xb = x[1] + (x[2] - x[1]) * kb;
			zb = z[1] + (z[2] - z[1]) * kb;
		}
		if (xa > xb) {
			double s;
			s = xa; xa = xb; xb = s;
			s = za; za = zb; zb = s;
		}
		int left = (int)ceil(xa), right = (int)floor(xb);
		if (left < 0)
			left = 0;
		if (right >= view_width)
			right = view_width - 1;
		for (int col = left; col <= right; col++) {
			double kz = (col - xa) / (xb - xa + 1e-9);
			plot(col, row, lit, fog(za + (zb - za) * kz));
		}
	}
}

// a dot of light, size in world units, which shrinks with distance like
// everything else
void draw_point(const struct camera *cam, struct vec point, struct light colour,
		double size)
{
	double x, y, z;
	if (!draw_project(cam, point, &x, &y, &z))
		return;
	double radius = cam->focal * size / z;
	if (radius < 0.5)
		radius = 0.5;
	if (radius > POINT_RADIUS_MAX * scale_up)
		radius = POINT_RADIUS_MAX * scale_up;
	double gain = fog(z);
	int reach = (int)ceil(radius);
	double inverse = 1.0 / (radius * radius);
	for (int dy = -reach; dy <= reach; dy++)
		for (int dx = -reach; dx <= reach; dx++) {
			// the light falls off with the square of the distance, which
			// needs no root
			double weight = 1.0 - (dx * dx + dy * dy) * inverse;
			if (weight > 0.0)
				plot((int)x + dx, (int)y + dy, colour, gain * weight);
		}
}

// a soft shoulder instead of a hard clamp, one minus exp of minus the
// light. the exp is one over its own series, four terms, close enough on
// this side of white, and it lets the compiler do eight pixels at a time
static inline float tone(float x)
{
	// the light is never negative, so the series never goes below one
	float sum = 1.0f + x * (1.0f + x * (0.5f + x * (1.0f / 6.0f + x * (1.0f / 24.0f))));
	return 255.0f * (1.0f - 1.0f / sum);
}

// the light of each pixel, with the sky under it, goes through the tone
// curve into a byte per channel, and the planes are emptied on the way
void draw_finish(void)
{
	for (int y = 0; y < view_height; y++) {
		struct light sky = light_mix(sky_top, sky_bottom, (double)y / view_height);
		size_t row = (size_t)y * view_width;
		for (int x = 0; x < view_width; x++) {
			size_t i = row + x;
			view[i] = rgb((int)tone((red[i] + (float)sky.r) * (float)TONE_KNEE),
				      (int)tone((green[i] + (float)sky.g) * (float)TONE_KNEE),
				      (int)tone((blue[i] + (float)sky.b) * (float)TONE_KNEE));
			red[i] = green[i] = blue[i] = 0.0f;
		}
	}
	finished = 1;
}
