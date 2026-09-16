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
#define ALARM_CORE 0xffe9c2
#define ALARM_TUBE 0xf0a53c
#define ALARM_EDGE 0x8c3c10
#define COLD_CORE  0xf4f8ff
#define COLD_TUBE  0xbcd2f0
#define COLD_EDGE  0x5070a0
#define HOUSING    0x1a1f28    // the metal housing that holds it

// the colour of light: a plate under a neon and the same plate in shadow
// differ by colour, not only by how much light they take
#define LIGHT_LAMP  0xf2fffc   // a white barely turned teal
// and the shadow is frankly blue: the contrast between the two is what
// makes the picture, not the difference in brightness
#define LIGHT_NIGHT 0x2c4a80

// the same colour, taken under this much neon and this much gloom
unsigned int tint(unsigned int color, double lamp, double night);

// the light table, the way it was done in 1993. tinting a colour is three
// multiplies and three casts, and a floor of a hundred thousand pixels asks
// for that eight hundred thousand times a frame. doom computed none of it:
// it read a table. one table per channel, and the pixel only reads.
#define LIGHT_LEVELS 64
void make_light_table(void);
unsigned int tinted(unsigned int color, double lamp);
// the painted tile: `cell` says which of the four we are looking at
unsigned int stencil_tile(int x, int y, int cell);
// and the tile an item sleeps on, read where the floor is read. one
// function for all four: what changes is the colour, and the colour is
// what says which one it is from across the room.
unsigned int loot_tile(int x, int y, unsigned int body, unsigned int edge);

// a wall met on a north-south line keeps this much of its light
#define SIDE_LIGHT   0.68

// several kinds of wall, so one room is not the next: a panel size and four
// colours, still drawn in code
#define WALL_KINDS 9
#define DOOR_TEXTURE 3         // which of the nine is the door
// three colours of strip, one per zone: the gangway in service, the machine
// room, and the hold on its emergency lighting
#define LAMP_TEXTURE 4         // the ordinary strip, teal
#define ALARM_TEXTURE 5        // the emergency one, amber
#define COLD_TEXTURE 6         // and the cold white of the machines
// the torn plating. the airlock does not say that something happened with a
// text, it says it with its walls, a plate ripped off, the dark behind it,
// and what splashed on it. it is the first thing the player sees.
#define TORN_TEXTURE 7
#define VAULT_TEXTURE 8        // the thick bulkhead of the last room
#define BLOOD_DARK  0x3a0f0e
#define BLOOD_WET   0x6e1c16
#define TORN_EDGE   0x8f9aa8   // the metal curled up along the tear

// the name stencilled on the airlock floor: four squares, three letters
// each, every letter turned a quarter turn to read the way one walks
#define STENCIL       "TANSOFTWARE"
// three tiles, for a geometric reason. with four, the middle of the band
// falls half a cell off the player's line and the name looks shifted to the
// left. with three, the middle falls exactly on the waking cell, and the word
// is centred in the frame.
#define STENCIL_CELLS 3
#define STENCIL_EACH  4        // how many letters to a square
#define STENCIL_PAD   7        // the margin around a letter, in pixels
#define STENCIL_PAINT 0xb9c6cf
#define STENCIL_WEAR  0.72     // the paint is worn, not fresh

// the badge: a plate set into the tile, it shines, and one goes to get it
#define BADGE_BODY 0x1d5f5a
#define BADGE_EDGE 0x8ff0e0
#define GUN_BODY   0x2b3140
#define GUN_EDGE   0xccd8e6
#define AMMO_BODY  0x4a3210
#define AMMO_EDGE  0xf0b44c
#define MED_BODY   0x123a2c
#define MED_EDGE   0x86f0b4
#define BADGE_R    36.0        // half the side of the plate
#define BADGE_HALO 22.0        // and how far its glow spills on the tile

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
