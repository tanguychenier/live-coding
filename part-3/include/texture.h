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

// the door: two leaves that part, a hazard band, a seam down the middle. it
// has to read as a door before anyone knows what to do with it
#define DOOR_STEEL 0x4d5866
#define DOOR_LIGHT 0x9daab8
#define DOOR_DARK  0x121620
#define DOOR_SEAM  0x0b0e14
#define HAZARD     0xd8a13a    // the diagonal stripes
#define HAZARD_DARK 0x1b1f27
#define DOOR_BAND  22          // how tall the hazard band is
#define DOOR_STRIPE 10         // how wide one stripe is

// the neon is the only thing in the level that shines, and it is what gives
// its colour to everything else
#define NEON_CORE  0xeafffb
#define NEON_TUBE  0x9ff0e4
#define NEON_EDGE  0x3aa896
#define HOUSING    0x1a1f28    // the metal housing that holds it

// the colour of light: a plate under a neon and the same plate in shadow
// differ by colour, not only by how much light they take
#define LIGHT_LAMP  0xf2fffc   // a white barely turned teal
// and the shadow is frankly blue: the contrast between the two is what
// makes the picture, not the difference in brightness
#define LIGHT_NIGHT 0x2c4a80

// the same colour, taken under this much neon and this much gloom
unsigned int tint(unsigned int color, double lamp, double night);

// a wall met on a north-south line keeps this much of its light
#define SIDE_LIGHT   0.68

// several kinds of wall, so one room is not the next: a panel size and four
// colours, still drawn in code
#define WALL_KINDS 5
#define DOOR_TEXTURE 3         // which of the five is the door
#define LAMP_TEXTURE 4         // and the wall that carries a strip

extern unsigned int wall_texture[WALL_KINDS][TEX_SIZE * TEX_SIZE];
extern unsigned int floor_texture[TEX_SIZE * TEX_SIZE];
extern unsigned int ceiling_texture[TEX_SIZE * TEX_SIZE];

double noise(int x, int y);
// two colours blended: part = 0 gives the first, 1 gives the second
unsigned int mix(unsigned int a, unsigned int b, double part);
unsigned int shade(unsigned int color, double light);
void make_wall_textures(void);
void make_floor_texture(void);
void make_ceiling_texture(void);

#endif
