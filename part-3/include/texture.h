#ifndef TEXTURE_H
#define TEXTURE_H

// a texture is a square of pixels drawn once at startup. a power of two, so
// wrapping around it is a bitwise and instead of a division
#define TEX_SIZE   128
#define TEX_MASK   (TEX_SIZE - 1)

// a station wall is panels, not bricks: a hollow seam, a bevel lit at the
// top left, a gradient inside the panel, rivets at the corners
struct plate {
	unsigned int color;        // the sheet metal
	unsigned int light;        // what it becomes in full light
	unsigned int dark;         // and in shadow
	unsigned int seam;         // the hollow joint between two panels
	int across, down;          // how many panels across, and down
	int rivets;                // are there rivets at the corners?
};

#define SEAM_W      3          // how wide a seam is
#define RIVET_IN    6          // how far from the corner a rivet sits
#define PLATE_SHEEN 0.34       // the gradient down a panel
#define PLATE_EDGE  0.30       // how strong the bevel is
#define PLATE_GRAIN 0.10       // and how strong the grain is
#define GRAIN_CELL  8          // the size of a patch of grain

// a wall met on a north-south line keeps this much of its light
#define SIDE_LIGHT   0.68

extern unsigned int wall_texture[TEX_SIZE * TEX_SIZE];
extern unsigned int floor_texture[TEX_SIZE * TEX_SIZE];
extern unsigned int ceiling_texture[TEX_SIZE * TEX_SIZE];

double noise(int x, int y);
// two colours blended: part = 0 gives the first, 1 gives the second
unsigned int mix(unsigned int a, unsigned int b, double part);
unsigned int shade(unsigned int color, double light);
void make_wall_texture(void);
void make_floor_texture(void);
void make_ceiling_texture(void);

#endif
