#ifndef DEMO_H
#define DEMO_H

#include "boss.h"
#include "draw.h"
#include "player.h"
#include "screen.h"

// the game plays itself, like a person would. it sweeps the sight onto what
// it sees, holds fire until it has enough, and lets go. it notices late and
// not always the same, it overshoots and settles, it lets go early now and
// then, and it misses one

// no key for this long from the first frame, and the cabinet takes over.
// it plays this long, then the game starts over from the first frame
#define DEMO_AFTER       8.0
#define DEMO_LENGTH      60.0
// the cabinet notices a target this long after it could have, and it never
// holds fire longer than this before letting go
#define DEMO_REACTION    0.3
#define DEMO_HOLD_MAX    2.4
// the cursor is close enough to a target when within this many pixels
#define DEMO_TOLERANCE   6.0
// it lets a target come this close, in world units, before it bothers
#define DEMO_ENGAGE      34.0
#define DEMO_PANIC       14.0
// with nothing to shoot, the cursor drifts back to the middle. the hand
// never quite rests, it wanders around the middle by this much of the
// picture, slowly, two turns that never line up
#define DEMO_REST_X      (view_width / 2.0)
#define DEMO_REST_Y      (view_height / 2.0)
#define HAND_WANDER      0.06
#define HAND_WANDER_RATE_X 0.7
#define HAND_WANDER_RATE_Y 0.9
// the hand. its reaction is drawn between these, in seconds, longer at the
// core where there is more to look at
#define HAND_REACT_MIN   0.15
#define HAND_REACT_MAX   0.45
#define HAND_REACT_CORE  0.35
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
#define HAND_BLIND_ODDS  0.3
#define HAND_BLIND_ZONE  ZONE_SWARM
// at the core, the phase it dies in the first time, the third. while it is
// dying it is this much slower to see anything, and if it has not died
// after this long it gives up dying and plays
#define HAND_DEATH_PHASE 2
#define HAND_DEATH_SLOW  0.7
#define HAND_DEATH_GIVE_UP 50.0
// it presses start this long after the title, once the name and the
// signature have written themselves, enter this long after a death, and
// types its name a letter at a time, this long apart
#define HAND_START_WAIT  4.0
#define HAND_RETRY_MIN   1.5
#define HAND_RETRY_MAX   3.0
#define HAND_WON_WAIT    6.0
#define HAND_LETTER_GAP  0.4
#define HAND_NAME        "TEC"
// the die of the hand, seeded on the clock so that no two runs are the same
#define HAND_DICE_SHIFT_A 13
#define HAND_DICE_SHIFT_B 17
#define HAND_DICE_SHIFT_C 5
#define HAND_DICE_BITS   16
#define HAND_DICE_MASK   ((1u << HAND_DICE_BITS) - 1)

void demo_reset(void);
// the hand takes the keys instead of the cabinet, for the rest of the run
void demo_hand(void);
int  demo_is_hand(void);
// writes the keys for this frame, as a player would. the boss, when there
// is one, tells the hand which phase it is in
void demo_drive(const struct player *player, const struct camera *cam, struct keys *keys,
		double now, int zone, const struct boss *boss);
// the hand at the title, at the death screen, at the end screen and at the
// name screen, each with its own delay. since says how long the screen has
// been up
void demo_title(struct keys *keys, double since);
void demo_dead(struct keys *keys, double since, int at_core);
void demo_won(struct keys *keys, double since);
void demo_name(struct keys *keys, double since);

#endif
