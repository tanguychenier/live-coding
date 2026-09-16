#ifndef SPRITE_H
#define SPRITE_H

// a sprite is a raw file prepared offline: four integers, then the pixels.
// the engine still has no image decoder, and the art stays a file one can
// replace without rebuilding.
//   "TECS", width, height, how many frames, then 0xAARRGGBB pixels

struct sprite {
	int width, height, frames;
	unsigned int *pixels;      // frames * height * width
	// where its pixels actually are. a weapon sheet covers the whole
	// frame and fills a quarter of it: copying all of it reads two
	// hundred thousand pixels to place sixty thousand. worked out once.
	int x0, y0, x1, y1;
};

int  sprite_load(struct sprite *s, const char *path);
// one pixel of one frame, or 0 when it is clear or out of the sheet
unsigned int sprite_at(const struct sprite *s, int frame, int x, int y);

#endif
