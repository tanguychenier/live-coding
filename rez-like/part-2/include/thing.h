#ifndef THING_H
#define THING_H

#include "draw.h"
#include "mesh.h"
#include "rail.h"
#include "tunnel.h"

// what flies in the world. a thing lives in rail coordinates, how far along
// the rail and where in the ring around it, so that the tunnel can bend
// and the thing stays inside it
#define THINGS_MAX      96
// how far behind the eye a thing is gone
#define GONE_BEHIND     0.08
// a thing that reaches the eye costs this much health, a bolt too
#define THING_DAMAGE    1
// how long a thing flashes after a hit that did not kill it
#define HIT_FLASH       0.12
// a thing shines brighter than the scenery, it is the reason the eye looks
#define THING_GLOW      1.9
// a turret comes to this many rail units ahead of the eye and keeps pace
// there for so many beats, firing every so many beats, then lets itself
// fall back and pass. it takes aim for this long before each shot, so that
// the warning can be heard and the turret shot first
#define TURRET_KEEP     1.2
#define TURRET_STAY     14.0
#define TURRET_AIM      0.55
#define TURRET_BEATS    4.0
#define TURRET_FIRST    3.0
// a bolt drifts this fast, in world units per second. the eye flies into it
// at more than twice that, so it is seen coming for a couple of seconds. it
// hits the eye when it comes this close, past the eye by more, it is gone
#define BOLT_SPEED      6.0
#define BOLT_HIT        2.2
#define BOLT_PAST       3.0
#define BOLT_SIZE       0.5
// a bolt is aimed at where the eye will be when it gets there. the flight
// time depends on the aim and the aim on the flight time, so the aim is
// refined this many times. in flight it still bends toward that place, by
// this much of the way per second, in case the eye did not go straight
#define AIM_STEPS       3
#define BOLT_HOMING     1.5
// a dive closes on the middle of the tunnel as it comes, over this many
// rail units, and a thing that orbits does so at this many tunnel radii
#define DIVE_CLOSE      2.4
#define ORBIT_RADIUS    0.62
#define ORBIT_RATE      1.1
#define CROSS_RATE      1.6
#define CROSS_SWING     0.7
// a hovering thing drifts on the beat by this much of a radius
#define HOVER_DRIFT     0.04
// how many bursts of sparks a death throws, and how fast
#define BURST_SPARKS    40
#define BURST_SPEED     6.0
#define HIT_SPARKS      8
#define HIT_SPEED       3.0
// the turning of a thing, radians per second, and a little more for every
// third one so that a wave does not turn as one. pitch follows the yaw
#define SPIN_RATE       1.2
#define SPIN_VARIETY    0.3
#define SPIN_PITCH      0.6
#define GATE_ROLL       0.3
#define MANTA_ROLL      0.35     // the wing rocks this much, this fast
#define MANTA_ROCK      3.0
#define SPINDLE_YAW     0.8
#define BOLT_ROLL       3.0
// each slot of the table has its own starting angle, this many radians
// apart, so that nothing born at once moves as one
#define PHASE_SPREAD    0.7
#define HOVER_RATE      2.0      // the breath of a hovering thing, per second
#define CROSS_BOB       0.15     // the up and down of a crossing manta
#define CROSS_BOB_RATE  3.1
#define AIM_BLINK       40.0     // the blink of a turret taking aim, per second
// a marked thing burns this much brighter, and pulses by this much, this fast
#define LOCK_PULSE      1.35
#define LOCK_PULSE_SWING 0.35
#define LOCK_PULSE_RATE 14.0
#define SHIELD_DIM      0.5      // a shielded part is this dim, it cannot be hurt
// the trail behind a bolt, drifting back at this fraction of its speed
#define BOLT_TRAIL_LIFE 0.3
#define BOLT_TRAIL_SIZE 0.12
#define BOLT_TRAIL_DRIFT 0.1

enum motion { MOTION_HOVER, MOTION_ORBIT, MOTION_CROSS, MOTION_DIVE, MOTION_GATE,
	      MOTION_SHOOTER, MOTION_BOLT, MOTION_HELD };

struct thing {
	int used;
	enum shape shape;
	enum motion motion;
	double t;              // along the rail
	double u, v;           // across the rail, in tunnel radii, right and up
	double speed;          // rail units per second, towards the eye
	double phase, spin;    // for the motions and the turning
	double size;
	int health;
	double hurt_at;
	struct vec at;         // where it is in the world, computed each frame
	struct vec velocity;   // bolts fly straight, in world units per second
	struct light colour;
	int locked;            // by the player, this frame
	double fire_at;        // a turret, the next moment it fires
	double leave_at;       // a turret, when it stops keeping pace
	int aiming;            // a turret, warned and about to fire
	int worth;             // the score, in kills. a boss part is worth more
	int shielded;          // cannot be marked nor hurt, the boss while closed
	unsigned int serial;   // which spawn this is, so a slot reused is not mistaken
};

// what the things aim at, the eye and where it is going
struct sight {
	struct vec eye, forward;
	struct vec motion;     // where the eye goes, world units per second
	struct vec bend;       // how the way bends, the pull to the side per second
};

extern struct thing things[THINGS_MAX];

void things_clear(void);
// every thing bursts into sparks and is gone, the end of a run
void things_scatter(void);
// puts a thing in the world ahead of the eye. returns it, or NULL if full
struct thing *thing_spawn(enum shape shape, enum motion motion, double t, double u,
			  double v, double speed, double size, int health,
			  struct light colour);
// a thing in world space, held by the boss or flying as a bolt
struct thing *thing_place(enum shape shape, enum motion motion, struct vec at, double size,
			  int health, struct light colour);
// fires a bolt from a point at the eye, leading it
void thing_fire(struct vec from, const struct sight *sight, struct light colour);
// the same with a choice of shape, size and speed, and turned aside by an
// angle in radians, for the fans of the core
void thing_launch(struct vec from, const struct sight *sight, struct light colour,
		  enum shape shape, double size, double speed, double spread);
// the thing with this serial, or NULL when it is gone
struct thing *thing_by_serial(unsigned int serial);
// moves everything, drops what is gone, and counts what reached the eye
int things_update(const struct rail *rail, double t_eye, const struct sight *sight,
		  double elapsed, double now);
void things_draw(const struct camera *cam, double now);
// the thing's place on screen, or 0 if it is behind the eye
int thing_on_screen(const struct camera *cam, const struct thing *thing,
		    double *x, double *y);
// a shot landed. the thing flashes or bursts, and the caller gets one for a
// kill, zero when it survived
int thing_hurt(struct thing *thing, double now);
// where a rail coordinate is in the world
struct vec thing_world(const struct rail *rail, double t, double u, double v);
int things_count(void);

#endif
