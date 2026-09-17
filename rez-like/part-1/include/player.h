#ifndef PLAYER_H
#define PLAYER_H

#include "draw.h"
#include "screen.h"

// the cursor marks targets while fire is held, up to eight, and letting go
// sends a shot to each, one per sixteenth, landing on a sixteenth. the
// chain is how many left at once, and the score grows with its square
#define LOCKS_MAX        8
// how close to the cursor a target has to be, in pixels, plus the target's
// own size seen from where it is
#define LOCK_RADIUS      34.0
#define LOCK_SIZE_GAIN   40.0
// a shot leaves one sixteenth after the one before, and lands this many
// sixteenths after it leaves
#define SHOT_FLIGHT_STEPS 2
#define SHOTS_MAX        32
// how fast the cursor moves with the keys, in pixels per second
#define CURSOR_SPEED     520.0
// a kill is worth this, times the chain squared. eight at once is sixty
// four times eight singles
#define SCORE_KILL       10
// how long the chain is shown big on the screen after a release
#define CHAIN_SHOW       1.2
// a shot bends this far out of its straight line, so that eight of them fan
#define SHOT_CURVE       0.9
// a shot's path is drawn in this many pieces, this dim at the hand end,
// with a head of light this big in world units
#define SHOT_TRAIL_STEPS 10
#define SHOT_TRAIL_DIM   0.15
#define SHOT_HEAD_SIZE   0.14
// the ring of the sight, this much of the lock radius at rest, swelling by
// this much this fast while it marks
#define CURSOR_REST      0.7
#define CURSOR_PULSE     0.08
#define CURSOR_PULSE_RATE 18.0
// the corners on a marked target, their size in pixels, their wobble, and
// the arms of each corner as a fraction of the size
#define BRACKET_SIZE     9.0
#define BRACKET_SWING    2.0
#define BRACKET_RATE     12.0
#define BRACKET_ARM      0.4
// the up and down of a shot's curve, as a fraction of its sideways bend
#define SHOT_CURVE_UP    0.5
// the spark a shot leaves at its head each frame
#define SHOT_SPARK_LIFE  0.25
#define SHOT_SPARK_SIZE  0.04

struct shot {
	int used;
	int flying;            // it has left the hand
	struct vec from;
	unsigned int target;   // the serial of the thing it flies to
	double launch, land;   // on the music clock
	int note;              // the degree of the scale it plays
	int side;              // which way it curves
};

struct player {
	double cursor_x, cursor_y;
	unsigned int locked[LOCKS_MAX];   // serials of the marked things
	int locks;
	int holding;                 // fire is down
	int was_holding;             // and it was last frame, a release is the edge
	struct shot shots[SHOTS_MAX];
	long score;
	int chain;                   // the last release, how many at once
	double chain_at;
	int best_chain;
	int kills;
	double released_at;          // the last release, for the pilot's arms
	int released;                // a chain just left, this frame
	int released_full;           // and it was a chain of eight, for the camera
};

void player_reset(struct player *player);
// reads the keys and moves the sight
void player_update(struct player *player, const struct keys *keys, double elapsed);
// marks targets while fire is held, releases the shots when it is let go,
// and flies them. from is where the shots leave
void player_aim(struct player *player, const struct camera *cam, struct vec from, double now);
void player_draw(const struct player *player, const struct camera *cam, double now);

#endif
