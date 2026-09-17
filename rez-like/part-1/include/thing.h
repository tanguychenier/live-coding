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
// how long a thing flashes after a hit that did not kill it
#define HIT_FLASH       0.12
// a thing shines brighter than the scenery, it is the reason the eye looks
#define THING_GLOW      1.9
// a thing that orbits does so at this many tunnel radii, this fast
#define ORBIT_RADIUS    0.62
#define ORBIT_RATE      1.1
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
// each slot of the table has its own starting angle, this many radians
// apart, so that nothing born at once moves as one
#define PHASE_SPREAD    0.7
#define HOVER_RATE      2.0      // the breath of a hovering thing, per second
// a marked thing burns this much brighter, and pulses by this much, this fast
#define LOCK_PULSE      1.35
#define LOCK_PULSE_SWING 0.35
#define LOCK_PULSE_RATE 14.0

enum motion { MOTION_HOVER, MOTION_ORBIT };

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
	struct light colour;
	int locked;            // by the player, this frame
	unsigned int serial;   // which spawn this is, so a slot reused is not mistaken
};

extern struct thing things[THINGS_MAX];

void things_clear(void);
// puts a thing in the world ahead of the eye. returns it, or NULL if full
struct thing *thing_spawn(enum shape shape, enum motion motion, double t, double u,
			  double v, double speed, double size, int health,
			  struct light colour);
// the thing with this serial, or NULL when it is gone
struct thing *thing_by_serial(unsigned int serial);
// moves everything, and drops what is gone behind the eye
void things_update(const struct rail *rail, double t_eye, double elapsed, double now);
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
