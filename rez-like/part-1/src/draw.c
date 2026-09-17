#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"

unsigned int *view;
int view_width, view_height;

// the loops over a row go by blocks of eight pixels, a constant, which lets
// the compiler turn a block into two wide instructions with no tail. the
// arrays are padded for the last block, and a row is never wider than this
#define BLOCK    8
#define ROW_MAX  4096

static int blocks_of(int width)
{
	return (width + BLOCK - 1) / BLOCK;
}

// light adds up in these, one per channel, nothing clamped before the end
// of the frame. a hundred faint lines crossing make a bright spot, the way
// real light does
static float *red, *green, *blue;
// the bloom works on a picture shrunk by this much, and its taps
static int bloom_down = BLOOM_DOWN;
static int small_width, small_height;
static float *small[3], *blurred[3], *tmp[3];
static float *wide[3];
static int *blur_x0, *blur_x1, *blur_y0, *blur_y1;
static float *blur_fx, *blur_fy;
// how much bigger than the base size the picture is
static double scale_up = 1.0;
// once the frame is finished, what is drawn goes straight into the picture,
// sharp, without the bloom. that is how the numbers stay readable
static int finished;
static double fog_depth = 38.0;
static double pulse;

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

// the small blurred picture is stretched back in two steps, along the rows
// into a wide strip, then down the columns, each pixel a mix of its two
// neighbours. the weights are the same for every row, computed once
static void make_blur_taps(void)
{
	for (int x = 0; x < view_width; x++) {
		double u = (double)x / bloom_down - 0.5;
		int x0 = (int)floor(u);
		blur_fx[x] = (float)(u - x0);
		blur_x0[x] = x0 < 0 ? 0 : x0;
		blur_x1[x] = x0 + 1 >= small_width ? small_width - 1 : x0 + 1;
	}
	for (int y = 0; y < view_height; y++) {
		double v = (double)y / bloom_down - 0.5;
		int y0 = (int)floor(v);
		blur_fy[y] = (float)(v - y0);
		blur_y0[y] = y0 < 0 ? 0 : y0;
		blur_y1[y] = y0 + 1 >= small_height ? small_height - 1 : y0 + 1;
	}
}

int draw_resize(int width, int height)
{
	if (width == view_width && height == view_height && view)
		return 1;
	free(view);
	free(red);
	free(green);
	free(blue);
	free(blur_x0);
	free(blur_fx);
	for (int ch = 0; ch < 3; ch++) {
		free(small[ch]);
		free(blurred[ch]);
		free(tmp[ch]);
		free(wide[ch]);
	}
	view_width = width;
	view_height = height;
	scale_up = (double)height / VIEW_BASE_HEIGHT;
	bloom_down = (int)floor(BLOOM_DOWN * scale_up + 0.5);
	if (bloom_down < 1)
		bloom_down = 1;
	small_width = width / bloom_down;
	small_height = height / bloom_down;
	// a block of pixels past the end of every row, for the loops that go
	// by blocks
	size_t pixels = (size_t)width * height + BLOCK, smalls = (size_t)small_width * small_height;
	view = calloc(pixels, sizeof *view);
	red = calloc(pixels, sizeof *red);
	green = calloc(pixels, sizeof *green);
	blue = calloc(pixels, sizeof *blue);
	blur_x0 = calloc((size_t)width * 2 + (size_t)height * 2 + 2 * BLOCK, sizeof *blur_x0);
	blur_fx = calloc((size_t)width + height + BLOCK, sizeof *blur_fx);
	for (int ch = 0; ch < 3; ch++) {
		small[ch] = calloc(smalls, sizeof *small[ch]);
		blurred[ch] = calloc(smalls, sizeof *blurred[ch]);
		tmp[ch] = calloc(smalls, sizeof *tmp[ch]);
		wide[ch] = calloc((size_t)small_height * width + BLOCK, sizeof *wide[ch]);
	}
	if (!view || !red || !green || !blue || !blur_x0 || !blur_fx
	    || !small[2] || !blurred[2] || !tmp[2] || !wide[2])
		return 0;
	blur_x1 = blur_x0 + width + BLOCK;
	blur_y0 = blur_x1 + width + BLOCK;
	blur_y1 = blur_y0 + height;
	blur_fy = blur_fx + width + BLOCK;
	make_blur_taps();
	return 1;
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

// the bloom. the picture is shrunk, blurred three times and added back on
// top of itself, and a wireframe starts to look like neon. five taps along
// the rows then down the columns, the edges done apart, no test in the middle
#define BLUR_TAPS  5
#define BLUR_EDGE  (BLUR_TAPS / 2)
static const float BLUR_WEIGHT[BLUR_TAPS] = { 1 / 16.f, 4 / 16.f, 6 / 16.f, 4 / 16.f, 1 / 16.f };

static float blur_at(const float *in, int width, int height, int x, int y)
{
	x = x < 0 ? 0 : x >= width ? width - 1 : x;
	y = y < 0 ? 0 : y >= height ? height - 1 : y;
	return in[y * width + x];
}

static void blur_pass(float *in, float *out, float *scratch)
{
	int width = small_width, height = small_height;
	for (int y = 0; y < height; y++) {
		const float *row = in + y * width;
		float *dst = scratch + y * width;
		for (int x = BLUR_EDGE; x < width - BLUR_EDGE; x++)
			dst[x] = row[x - 2] * BLUR_WEIGHT[0] + row[x - 1] * BLUR_WEIGHT[1]
				+ row[x] * BLUR_WEIGHT[2] + row[x + 1] * BLUR_WEIGHT[3]
				+ row[x + 2] * BLUR_WEIGHT[4];
		for (int x = 0; x < width; x += width - 1 - BLUR_EDGE) {
			for (int e = x; e < x + BLUR_EDGE; e++) {
				float sum = 0;
				for (int k = -BLUR_EDGE; k <= BLUR_EDGE; k++)
					sum += blur_at(in, width, height, e + k, y) * BLUR_WEIGHT[k + BLUR_EDGE];
				dst[e] = sum;
			}
		}
	}
	for (int y = BLUR_EDGE; y < height - BLUR_EDGE; y++) {
		float *dst = out + y * width;
		const float *r0 = scratch + (y - 2) * width, *r1 = scratch + (y - 1) * width;
		const float *r2 = scratch + y * width, *r3 = scratch + (y + 1) * width;
		const float *r4 = scratch + (y + 2) * width;
		for (int x = 0; x < width; x++)
			dst[x] = r0[x] * BLUR_WEIGHT[0] + r1[x] * BLUR_WEIGHT[1] + r2[x] * BLUR_WEIGHT[2]
				+ r3[x] * BLUR_WEIGHT[3] + r4[x] * BLUR_WEIGHT[4];
	}
	for (int y = 0; y < height; y += height - 1 - BLUR_EDGE)
		for (int e = y; e < y + BLUR_EDGE; e++)
			for (int x = 0; x < width; x++) {
				float sum = 0;
				for (int k = -BLUR_EDGE; k <= BLUR_EDGE; k++)
					sum += blur_at(scratch, width, height, x, e + k) * BLUR_WEIGHT[k + BLUR_EDGE];
				out[e * width + x] = sum;
			}
}

// the rows of a block added into one, half of the shrink. a plain loop
// over blocks, so that it runs wide
static void add_row(float *restrict acc, const float *restrict src, int blocks)
{
	for (int b = 0; b < blocks; b++)
		for (int k = b * BLOCK; k < b * BLOCK + BLOCK; k++)
			acc[k] += src[k];
}

// the channels blurred at the end of a frame for the next one. the glow
// trails the picture by a frame, which no eye can tell, and the planes are
// read once per frame instead of twice
static void blur_channel(int first, int last, void *data)
{
	(void)data;
	for (int ch = first; ch < last; ch++) {
		blur_pass(small[ch], blurred[ch], tmp[ch]);
		blur_pass(blurred[ch], small[ch], tmp[ch]);
		blur_pass(small[ch], blurred[ch], tmp[ch]);
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

// the glow of each channel is stretched along into its strip, the three
// channels side by side
static void stretch_row(float *restrict dst, const float *restrict row, int blocks)
{
	for (int b = 0; b < blocks; b++)
		for (int k = b * BLOCK; k < b * BLOCK + BLOCK; k++)
			dst[k] = row[blur_x0[k]] + (row[blur_x1[k]] - row[blur_x0[k]]) * blur_fx[k];
}

static void blur_and_stretch(int first, int last, void *data)
{
	blur_channel(first, last, data);
	for (int ch = first; ch < last; ch++)
		for (int y = 0; y < small_height; y++)
			stretch_row(wide[ch] + (size_t)y * view_width, blurred[ch] + y * small_width,
				    blocks_of(view_width));
}

// the pieces of a row, each a plain loop over arrays that the compiler is
// told do not overlap, so that it can run them eight pixels at a time
static void mix_row(float *restrict dst, const float *restrict src,
		    const float *restrict above, const float *restrict below,
		    float fy, float amount, float base, int blocks)
{
	for (int b = 0; b < blocks; b++)
		for (int k = b * BLOCK; k < b * BLOCK + BLOCK; k++)
			dst[k] = src[k] + amount * (above[k] + (below[k] - above[k]) * fy) + base;
}

static void tone_row(int *restrict out, const float *restrict in, int blocks)
{
	for (int b = 0; b < blocks; b++)
		for (int k = b * BLOCK; k < b * BLOCK + BLOCK; k++)
			out[k] = (int)tone(in[k] * (float)TONE_KNEE);
}

static void pack_row(unsigned int *restrict out, const int *restrict r, const int *restrict g,
		     const int *restrict b, int blocks)
{
	for (int block = 0; block < blocks; block++)
		for (int k = block * BLOCK; k < block * BLOCK + BLOCK; k++)
			out[k] = ((unsigned int)r[k] << 16) | ((unsigned int)g[k] << 8) | (unsigned int)b[k];
}

// a row is finished in one pass, glow and sky added, tone curve, pack. the rows go by bands of one small pixel's height, and each band is
// added up into the small picture of the next frame's glow on the way
struct finish_job {
	float amount;
};

static void finish_bands(int first, int last, void *data)
{
	const struct finish_job *job = data;
	float *plane[3] = { red, green, blue };
	static _Thread_local float value[ROW_MAX + BLOCK];
	static _Thread_local float acc[3][ROW_MAX + BLOCK];
	static _Thread_local int byte[3][ROW_MAX + BLOCK];
	int width = view_width < ROW_MAX ? view_width : ROW_MAX;
	int blocks = blocks_of(width);
	float share = 1.0f / (float)(bloom_down * bloom_down);
	for (int band = first; band < last; band++) {
		memset(acc, 0, sizeof acc);
		int y0 = band * bloom_down, y1 = y0 + bloom_down;
		if (y1 > view_height)
			y1 = view_height;
		for (int y = y0; y < y1; y++) {
			struct light sky = light_mix(sky_top, sky_bottom, (double)y / view_height);
			float base[3] = { (float)sky.r, (float)sky.g, (float)sky.b };
			size_t row = (size_t)y * view_width;
			for (int ch = 0; ch < 3; ch++) {
				float *src = plane[ch] + row;
				add_row(acc[ch], src, blocks);
				// the sky is never negative, nor the light, so what reaches
				// the tone curve is never negative either
				mix_row(value, src, wide[ch] + (size_t)blur_y0[y] * view_width,
					wide[ch] + (size_t)blur_y1[y] * view_width, blur_fy[y],
					job->amount, base[ch], blocks);
				tone_row(byte[ch], value, blocks);
				// the row is emptied for the next frame while it is still here
				memset(src, 0, (size_t)width * sizeof *src);
			}
			pack_row(view + row, byte[0], byte[1], byte[2], blocks);
		}
		// the band, shrunk to one row of the small picture
		if (band < small_height)
			for (int ch = 0; ch < 3; ch++)
				for (int x = 0; x < small_width; x++) {
					float sum = 0;
					for (int dx = 0; dx < bloom_down; dx++)
						sum += acc[ch][x * bloom_down + dx];
					small[ch][band * small_width + x] = sum * share;
				}
	}
}

void draw_finish(void)
{
	// a line covers less of a bigger block, so the glow of a bigger picture
	// is scaled back up by the block, to keep the same neon
	struct finish_job job = {
		(float)(BLOOM_AMOUNT * (1.0 + BLOOM_PULSE * pulse) * bloom_down / BLOOM_DOWN) };
	finish_bands(0, (view_height + bloom_down - 1) / bloom_down, &job);
	blur_and_stretch(0, 3, NULL);
	finished = 1;
}
