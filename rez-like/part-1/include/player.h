#ifndef PLAYER_H
#define PLAYER_H

#include "draw.h"
#include "screen.h"

// the player is a sight, a ring the arrows and the mouse move over the
// picture. it marks, and what it marks is for the next step
#define LOCK_RADIUS      34.0
// how fast the cursor moves with the keys, in pixels per second
#define CURSOR_SPEED     520.0
// the ring of the sight, this much of the lock radius at rest, swelling by
// this much this fast while it marks
#define CURSOR_REST      0.7
#define CURSOR_PULSE     0.08
#define CURSOR_PULSE_RATE 18.0

struct player {
	double cursor_x, cursor_y;
	int holding;                 // fire is down
};

void player_reset(struct player *player);
// reads the keys and moves the sight
void player_update(struct player *player, const struct keys *keys, double elapsed);
void player_draw(const struct player *player, double now);

#endif
