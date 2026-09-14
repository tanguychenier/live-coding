#ifndef RENDER_H
#define RENDER_H

#include "world.h"

// a colour is three bytes in one number: where each one starts, and what it
// takes to keep a single one. every blend of colours starts here
#define RED_SHIFT   16
#define GREEN_SHIFT 8
#define CHANNEL     0xff

// how wide the view is: 0.66 against a unit direction is about 66 degrees
#define FIELD_OF_VIEW 0.66

// how fast the light falls off with distance
#define FOG_DENSITY   0.02
// never divide by less than this: a wall right against the eye
#define NEAR_CLIP     0.02
// a ray parallel to an axis never crosses that axis's grid lines
#define VERY_FAR      1e30

#define MAP_CELL   3
#define MAP_LEFT   4
#define MAP_TOP    4

// the player on the map: half the side of his square, and how many cells long
// the line showing where he looks
#define MAP_DOT    2
#define MAP_ARROW  4

#define MAP_WALL   0x6f7b8f
#define MAP_FLOOR  0x161a22
#define MAP_HEADING 0xe8b04b
#define MAP_PLAYER 0xd8534f

double fog(double distance);
void render_floor_and_ceiling(const struct player *player);
void render_walls(const struct player *player);
void render_map(const struct player *player, int cell, int left, int top);

#endif
