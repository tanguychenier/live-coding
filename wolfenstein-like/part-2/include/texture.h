#ifndef TEXTURE_H
#define TEXTURE_H

// a texture is a square of pixels drawn once at startup. a power of two, so
// wrapping around it is a bitwise and instead of a division
#define TEX_SIZE   64
#define TEX_MASK   (TEX_SIZE - 1)

// a brick wall: the size of one brick, its two colours, and how far one brick
// is allowed to differ from the next so that the wall is not flat
#define BRICK_WIDTH  16
#define BRICK_HEIGHT 8
#define BRICK_COLOR  0x8a6f5d
#define BRICK_JOINT  0x4a4038
#define BRICK_LIGHT  0.78
#define BRICK_VARY   0.30

// a tiled floor: a tile is square, so one size is enough
#define TILE_SIZE   32
#define TILE_COLOR  0x4a4a52
#define TILE_JOINT  0x2e2e34
#define TILE_LIGHT  0.85
#define TILE_VARY   0.25

// a panelled ceiling: panels are wide and flat, so a height and not a size,
// and a coarser grain than stone
#define PANEL_HEIGHT 16
#define PANEL_GRAIN  8
#define PANEL_COLOR  0x343b46
#define PANEL_JOINT  0x232932
#define PANEL_LIGHT  0.90
#define PANEL_VARY   0.15

// a wall met on a north-south line keeps this much of its light
#define SIDE_LIGHT   0.68

extern unsigned int wall_texture[TEX_SIZE * TEX_SIZE];
extern unsigned int floor_texture[TEX_SIZE * TEX_SIZE];
extern unsigned int ceiling_texture[TEX_SIZE * TEX_SIZE];

double noise(int x, int y);
unsigned int shade(unsigned int color, double light);
void make_wall_texture(void);
void make_floor_texture(void);
void make_ceiling_texture(void);

#endif