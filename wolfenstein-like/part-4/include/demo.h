#ifndef DEMO_H
#define DEMO_H

#include "screen.h"
#include "world.h"

// the level plays itself, the way an arcade cabinet does when nobody has
// put a coin in. it walks a route written in a file, one point per line,
// and it fights what gets in the way with whatever is in hand.

#define ROUTE_FILE      "levels/keep.route"
#define ROUTE_MAX       64

// no key for this long, and the cabinet takes over. any key gives it back
#define DEMO_AFTER      12.0
// close enough to a point to go for the next one, and how long it keeps
// trying a point it gains nothing on before giving that point up
#define DEMO_ARRIVED    0.45
#define DEMO_PATIENCE   3.0
// it turns until the point is this close to straight ahead, and it walks
// as soon as the point is inside this cone
#define DEMO_TURN_TOL   0.06
#define DEMO_WALK_CONE  0.35
// how far it notices something alive, how wide a body is to aim at, and
// how close it walks in with the bar in hand
#define DEMO_ENGAGE     7.0
#define DEMO_BODY       0.30
#define DEMO_STANDOFF   1.6
// the cabinet does not play the same game twice. it rolls a die of its own,
// seeded on the clock, and the die decides where it starts among the first
// points of the route, whether it stops at a point, for how long, and for how
// long it turns its head
#define DICE_SHIFT_A    13
#define DICE_SHIFT_B    17
#define DICE_SHIFT_C    5
#define DICE_MASK       0xffffu
#define DEMO_START_SPAN 4
#define DEMO_LINGER_ODDS 0.4
#define DEMO_LINGER_MIN 0.4
#define DEMO_LINGER_MAX 1.4
#define DEMO_GLANCE     0.25
#define EVEN_ODDS       0.5

// how many points the file gave
int  demo_load(const char *path);
void demo_reset(void);
// reads the world, writes the keys: the same keys a hand would press
void demo_drive(const struct player *player, struct keys *keys, double now);

#endif
