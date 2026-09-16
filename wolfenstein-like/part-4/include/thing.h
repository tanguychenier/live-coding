#ifndef THING_H_INCLUDED
#define THING_H_INCLUDED

#include "world.h"

// a thing is a sheet of pixels prepared offline, a place in the world, and
// four states. the engine still has no image decoder: the sheet is a raw
// file one can replace without rebuilding.

#define THINGS_MAX      36
// how close it gets before it swings. under a metre and a half a thing as
// tall as the corridor fills the whole screen and one sees nothing at all:
// not it, not the room, not where the next blow comes from. it stops at
// arm's length, and the bar is longer than its arm.
#define THING_REACH     1.55
#define THING_SPEED     1.9
#define THING_SIGHT     11.0
#define THING_HEALTH    100.0
#define THING_DAMAGE    9.0
// it does not hit without pause. without this gap two of them against a wall
// take a hundred points off in two and a half seconds: one does not die of a
// mistake, one dies of having walked in. there has to be time to see the blow
// start, to step back, and to answer it.
#define THING_GAP       1.45
// the heavy one. three hundred points is nine swings of the bar, and nobody
// stands still for nine swings. it is what makes the sidearm necessary, and
// that is why it only shows up after it.
#define TOUGH_HEALTH    300.0
#define TOUGH_DAMAGE    14.0
#define TOUGH_SPEED     1.62
#define TOUGH_REACH     1.75
#define TOUGH_SIZE      1.22

enum thing_state { THING_IDLE, THING_ALERT, THING_HUNT, THING_STRIKE, THING_DEAD };

struct thing {
	double x, y;
	double dir_x, dir_y;
	enum thing_state state;
	double since;          // when it entered this state
	double health;
	double stride;         // the walk cycle
	double last_seen_x, last_seen_y;
	double next_strike;    // not before that moment
	// it does not walk in a straight line and it does not shrug off a hit.
	// a thing that comes at you and never flinches is played by walking
	// backwards; one that weaves and staggers has to be played.
	double weave;          // its own phase, so they do not all sway alike
	double stagger;        // it just took one: it shows
	// when it was hit, and nothing else. using "since" for this washed it
	// out at every change of state: up close it was pale all the time.
	double hurt_at;
	int used;
	int silent;            // it has cried once: it does not cry twice
	int tough;             // the heavy one, the one the bar cannot finish
};

extern struct thing things[THINGS_MAX];

void things_clear(void);
// the level says where they are. "x" one standing, "y" a body, "X" a heavy.
// placing them by hand in main.c would be two truths for one fact.
void things_from_level(void);
int  thing_add(double x, double y);
// their whole life: look, walk, strike. returns the damage dealt this frame
double things_update(const struct player *player, double elapsed, double now);
// drawn after the walls, so the depth buffer is full
void things_draw(const struct player *player, double now);
// a cry carries: everything asleep within reach wakes up
void things_hear(double x, double y, double reach, double now);

#endif
