#ifndef THING_H_INCLUDED
#define THING_H_INCLUDED

#include "world.h"

// a thing is a sheet of pixels prepared offline, a place in the world, and
// four states. the engine still has no image decoder: the sheet is a raw
// file one can replace without rebuilding.

#define THINGS_MAX      36
#define NEVER           1e9       // a moment that does not come
#define THING_W         24        // how wide it is drawn, in its own pixels
#define THING_H         40
// how close it comes. under a metre and a half, a thing as tall as the
// corridor fills the whole screen and you cannot see anything any more, not
// it, not the room, not where the next blow comes from. so it stops at arm's
// length, and the bar is longer than its arm.
#define THING_REACH     1.55      // how close it has to be to hit you
#define THING_WINDUP    0.45      // how long the blow takes to come down
#define BOSS_WINDUP     0.62
#define STRIKE_SLACK    0.3       // a little further than its reach still lands
#define THING_SPEED     1.9
#define THING_SIGHT     11.0
#define THING_HEALTH    100.0
#define THING_DAMAGE    9.0
// it does not hit without pause. without this gap two of them against a wall
// take a hundred points off in two and a half seconds: one does not die of a
// mistake, one dies of having walked in. there has to be time to see the blow
// start, to step back, and to answer it.
#define THING_GAP       1.45
// how far its shoulder stays from a wall. tested on its centre alone it walks
// until half of its body is drawn inside the plating
#define THING_SHOULDER  0.3
// the heavy one. three hundred points is nine swings of the bar, and nobody
// stands still for nine swings. it is what makes the sidearm necessary, and
// that is why it only shows up after it.
#define TOUGH_HEALTH    300.0
#define TOUGH_DAMAGE    14.0
#define TOUGH_SPEED     1.62
#define TOUGH_REACH     1.75
#define TOUGH_SIZE      1.22

#define BOSS_HEALTH     540.0
#define BOSS_DAMAGE     20.0
#define BOSS_SPEED      1.45
#define BOSS_REACH      2.25
#define BOSS_SIZE       1.55      // its size, in corridor heights
// these are the only saturated pixels on the screen, so you see them before
// you see the shape
#define THING_EYE       0xff5a3c

// the rim takes the colour of the neon of the zone it stands in, so that it
// belongs to the room instead of looking pasted on top of it
unsigned int zone_tint(double x, double y);

enum thing_state { THING_IDLE, THING_ALERT, THING_HUNT, THING_STRIKE, THING_DEAD };

// the thing at the bottom has a pattern, and that is what makes it a boss. a
// beast that just walks at you and soaks up hits is only a longer health bar.
// this one announces its charge, charges, and then rests, so the player
// learns to step aside and to hit while it rests.
enum boss_move {
	BOSS_WALK,      // it walks, like the others
	BOSS_WIND,      // it stops and roars: the announcement, and it lasts
	BOSS_CHARGE,    // it rushes in a straight line, three times faster
	BOSS_REST,      // it missed and it is winded: hits count double here
	BOSS_SLAM       // under a third of its health it hits the floor instead
};
#define BOSS_WIND_TIME   0.95
#define BOSS_CHARGE_TIME 1.30
#define BOSS_REST_TIME   1.70
#define BOSS_WALK_TIME   3.20
#define BOSS_CHARGE_HIT  26.0
#define BOSS_SLAM_REACH  5.0
#define BOSS_SLAM_HIT    18.0

struct thing {
	double x, y;
	double dir_x, dir_y;
	enum thing_state state;
	double since;          // when it entered this state
	double health;
	double stride;         // the walk cycle, 0 to 1
	double last_seen_x, last_seen_y;
	double next_strike;    // not before this moment
	// it does not walk in a straight line and it does not shrug off a hit.
	// a thing that comes at you and never flinches is played by walking
	// backwards; one that weaves and staggers has to be played.
	double weave;          // its own phase, so they do not all sway alike
	double stagger;        // it just took one: it shows
	// when it was hit, and nothing else. using "since" for this washed it
	// out at every change of state: up close it was pale all the time.
	double hurt_at;
	int used;
	int silent;            // it has been seen already: it does not shriek twice
	int boss;              // the one at the bottom of the hold
	enum boss_move move;   // and where it is in its pattern
	double move_at;
	int tough;             // the heavy one, the one after the sidearm
	// the survivors lie on the floor, they talk, and at some point they stop.
	// it is the only way to have a crew without ever showing it alive,
	// because we always arrive too late and we hear it.
	int crew;
	int line;              // how many lines it still has
	double next_line;
	// these are the ones you see through the jammed door. they come and go at
	// the far end of the corridor and they never come here, because they are
	// not there to fight, they are there so that you understand what is
	// waiting outside.
	int roam;
	double roam_x, roam_y;   // the point they start from
	double roam_to;          // and how far they stray from it
	double roam_wob;         // and how far they drift sideways
	int leaving;             // it is done, it leaves by the north corridor
	// where it looks, when that is not where it walks. if we wrote the gaze
	// into dir, it would steer the beast, and at the next step it would walk
	// that way.
	double look_x, look_y;
	// the armed crew, which you see only once. they hold the far end of the
	// deck, they fire, and they lose. they never come towards us, because the
	// scene is there to be watched, not to be played.
	int crewman;
	double fire_until;
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
// gives the health of the boss from 0 to 1 once it is awake, or -1 before
// that
double boss_health(void);
// tells whether the boss is down. it is a separate question, otherwise the
// bar would stay on screen.
int boss_down(void);
// how much one hit is worth on it right now. it is double while the boss
// rests.
double thing_weak(const struct thing *t);
// tells whether the boss is winded right now, because that is the moment to
// hit it
int boss_resting(void);
// a noise carries far. after a shot or a cry, everything asleep within reach
// gets up, and that is why you think twice before firing.
void things_hear(double x, double y, double reach, double now);
// gives what a survivor still has to say, or NULL when he has nothing left
const char *crew_speak(const struct player *player, double now);
// after the blast, the beasts at the far end leave
void things_roam_away(void);
// during the scene one of them falls, and that is the one we hear die
void things_roam_kill(void);
// the beasts close in on the crew, weaving as they come
void things_roam_advance(void);
// when a round passes close, the beast flashes white for an instant
void things_roam_graze(double now);
// makes a crewman fire (0 or 1), and makes a crewman fall
void crew_fire(int which, double now);
void crew_fall(int which);

#endif
