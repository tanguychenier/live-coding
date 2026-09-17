#ifndef BOSS_H
#define BOSS_H

#include "draw.h"
#include "palette.h"
#include "thing.h"

// the core. a solid at the centre of the arena, the eye circling it, and
// four phases, each with one thing to learn. the shields first, then the
// weak points on the shell, then the shell that opens and closes on the
// bar, then the naked core that fires at everything
#define BOSS_PHASES        4
#define BOSS_CORE_SIZE     5.0
#define BOSS_SPIN          0.35     // radians per second, faster with each phase
#define BOSS_SPIN_GAIN     0.4
#define BOSS_PITCH_RATIO   0.6      // the shell pitches this much of its yaw
// phase one, the shields. eight nodes on two tilted rings around the core,
// two hits each, and every so many beats one of them fires
#define BOSS_NODES         8
#define BOSS_NODE_HEALTH   2
#define BOSS_NODE_SIZE     1.1
#define BOSS_NODE_ORBIT    10.0
#define BOSS_NODE_RATE     0.45
#define BOSS_NODE_TILT     0.6
#define BOSS_NODE_FIRE_BEATS 8.0
// phase two, the weak points. six of the shell's corners can be hurt, two
// hits each, and the core throws a fan of three bolts every so many beats
#define BOSS_POINTS        6
#define BOSS_POINT_SIZE    0.9
#define BOSS_POINT_HEALTH  2
#define BOSS_FAN_BEATS     8.0
// every bolt of the core flies this fast. it is aimed at where the eye will
// be, so it need not be faster than the eye, and it stays in sight for
// about three seconds, the time to mark it
#define BOSS_BOLT_SPEED    7.0
#define BOSS_FAN_SPREAD    0.12
// phase three, the shell splits open for two bars and closes for two. the
// inner core can only be hurt while it is open, and while it is open a
// drone comes out of it every so many beats, aimed at the eye
#define BOSS_INNER_SIZE    2.4
#define BOSS_INNER_HEALTH  12
#define BOSS_SPLIT         5.0      // how far the halves move apart
#define BOSS_SHELL_SLIDE   4.0      // parts per second, the slide of the halves
#define BOSS_PARTED        0.05     // past this much open, the halves are apart
#define BOSS_OPEN_BARS     2
#define BOSS_DRONE_BEATS   2.0
#define BOSS_DRONE_SPEED   7.0
#define BOSS_DRONE_SIZE    0.7
// phase four, the fury. the naked core, always open, spinning, firing pairs
// of bolts every six beats. the fans and the pairs open by this many
// radians, little enough that one sweep of the sight marks them all
#define BOSS_FURY_HEALTH   16
#define BOSS_FURY_BEATS    6.0
#define BOSS_FURY_SPREAD   0.18
// the score of the parts, in kills
#define BOSS_NODE_WORTH    3
#define BOSS_POINT_WORTH   4
#define BOSS_INNER_WORTH   25
#define BOSS_FURY_WORTH    60
// how long the stinger and the flash of a phase change last, and how much
// brighter the shell goes
#define BOSS_FLASH         0.5
#define BOSS_FLASH_GAIN    2.0
// the throb of the fury on the beat, the light in the heart and its size,
// bigger when the shell is open, and how far the shards fly at the death
#define BOSS_THROB         0.08
#define BOSS_HEART_LIGHT   0.8
#define BOSS_HEART_SIZE    0.8
#define BOSS_HEART_OPEN    0.6
#define BOSS_SPOKE_LIGHT   0.5
#define BOSS_SHARD_FLIGHT  40.0
// the eye circles the core at this many radians per second, and looks at
// it after this many seconds of turning from the rail
#define BOSS_ORBIT_RATE         0.36
#define ORBIT_HEIGHT       2.5
#define ORBIT_TURN_IN      2.5
// the slow zoom of the boss, how much the focal length swells and how
// slowly, in seconds per breath
#define BOSS_ZOOM          0.22
#define BOSS_ZOOM_PERIOD   14.0
// after the core dies, the arena burns for this long before the end
#define BOSS_DEATH         4.0
#define BOSS_DEATH_SPARKS  260
#define BOSS_DEATH_SPEED   14.0

struct boss {
	int active, phase, dead;
	double began, phase_at, dead_at, flash_at;
	struct vec centre;
	// the orbit, its two axes in the world
	struct vec axis_x, axis_z;
	double spin;
	unsigned int node[BOSS_NODES];
	unsigned int point[BOSS_POINTS];
	unsigned int inner;
	double next_fire, next_drone;
	int fired;                       // which node fires next
	double open;                     // the shell, zero closed to one open
	int changed;                     // a phase just changed, read once
	long score;                      // what the boss added, for the tally
};

void boss_reset(struct boss *boss);
// the eye has reached the orbit. the arena stands at the centre, the rail
// ends at eye going forward
void boss_begin(struct boss *boss, struct vec centre, struct vec eye, struct vec forward,
		double now);
void boss_update(struct boss *boss, const struct sight *sight, double elapsed, double now);
// where the eye is and what it looks at, once the orbit has it, and where
// it goes and how its way bends, for the aim of the bolts
void boss_eye(const struct boss *boss, double now, struct vec *eye, struct vec *at,
	      struct vec *motion, struct vec *bend);
void boss_draw(const struct boss *boss, const struct camera *cam, double now,
	       const struct palette *pal);
// what is left of it, from one down to zero, for the bar
double boss_health(const struct boss *boss);
int boss_phase_changed(struct boss *boss);

#endif
