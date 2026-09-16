#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sprite.h"

int sprite_load(struct sprite *s, const char *path)
{
	memset(s, 0, sizeof *s);
	FILE *f = fopen(path, "rb");
	if (!f)
		return 0;
	char magic[4];
	unsigned int head[3];
	if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "TECS", 4) != 0
	    || fread(head, sizeof *head, 3, f) != 3) {
		fclose(f);
		return 0;
	}
	s->width = (int)head[0];
	s->height = (int)head[1];
	s->frames = (int)head[2];
	size_t amount = (size_t)s->width * s->height * s->frames;
	if (s->width <= 0 || s->height <= 0 || s->frames <= 0 || amount > 64u << 20) {
		fclose(f);
		return 0;
	}
	s->pixels = malloc(amount * sizeof *s->pixels);
	if (!s->pixels || fread(s->pixels, sizeof *s->pixels, amount, f) != amount) {
		free(s->pixels);
		s->pixels = NULL;
		fclose(f);
		return 0;
	}
	fclose(f);

	// the box: the smallest rectangle holding everything that is not
	// clear, across every frame
	s->x0 = s->width;
	s->y0 = s->height;
	s->x1 = 0;
	s->y1 = 0;
	for (int n = 0; n < s->frames; n++)
		for (int y = 0; y < s->height; y++)
			for (int x = 0; x < s->width; x++)
				if (s->pixels[((size_t)n * s->height + y)
					      * s->width + x] >> 24 >= 128) {
					if (x < s->x0) s->x0 = x;
					if (y < s->y0) s->y0 = y;
					if (x >= s->x1) s->x1 = x + 1;
					if (y >= s->y1) s->y1 = y + 1;
				}
	if (s->x1 <= s->x0 || s->y1 <= s->y0) {
		s->x0 = s->y0 = 0;
		s->x1 = s->width;
		s->y1 = s->height;
	}
	return 1;
}

unsigned int sprite_at(const struct sprite *s, int frame, int x, int y)
{
	if (!s->pixels || frame < 0 || frame >= s->frames
	    || x < 0 || y < 0 || x >= s->width || y >= s->height)
		return 0;
	return s->pixels[((size_t)frame * s->height + y) * s->width + x];
}
