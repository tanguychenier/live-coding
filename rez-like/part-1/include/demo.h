#ifndef DEMO_H
#define DEMO_H

#include "draw.h"
#include "player.h"
#include "screen.h"

// the game plays itself, like a person would. it sweeps the sight onto what
// it sees, holds fire until it has enough, and lets go. it notices late and
// not always the same, it overshoots and settles, it lets go early now and
// then, and it misses one

// the hand reacts to a new target this long after it could have, drawn
// between these, and it never holds fire longer than this before letting go
#define DEMO_REACTION    0.3
#define DEMO_HOLD_MAX    2.4
// the cursor is close enough to a target when within this many pixels
#define DEMO_TOLERANCE   6.0
// it lets a target come this close, in world units, before it bothers
#define DEMO_ENGAGE      34.0
// with nothing to shoot, the cursor drifts back to the middle
#define DEMO_REST_X      (view_width / 2.0)
#define DEMO_REST_Y      (view_height / 2.0)
// the hand. its reaction is drawn between these, in seconds
#define HAND_REACT_MIN   0.15
#define HAND_REACT_MAX   0.45
// once it has reached a target it keeps going for this long before it
// notices, so it overshoots and settles
#define HAND_OVERSHOOT_MIN 0.05
#define HAND_OVERSHOOT_MAX 0.12
// how often it goes for the full eight, else it lets go with this many
#define HAND_FULL_ODDS   0.55
#define HAND_CHAIN_MIN   3
#define HAND_CHAIN_MAX   7
#define HAND_HOLD_MIN    1.6
#define HAND_HOLD_MAX    2.6
// how often it misses one of the targets of a hold
#define HAND_MISS_ODDS   0.15
// the target it is going for looks this much nearer than it is
#define HAND_STICK       0.4
// the die of the hand, seeded on the clock so that no two runs are the same
#define HAND_DICE_SHIFT_A 13
#define HAND_DICE_SHIFT_B 17
#define HAND_DICE_SHIFT_C 5
#define HAND_DICE_BITS   16
#define HAND_DICE_MASK   ((1u << HAND_DICE_BITS) - 1)

void demo_reset(void);
// writes the keys for this frame, as a player would
void demo_drive(const struct player *player, const struct camera *cam, struct keys *keys,
		double now);

#endif
