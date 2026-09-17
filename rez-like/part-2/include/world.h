#ifndef WORLD_H
#define WORLD_H

#include "draw.h"
#include "palette.h"
#include "rail.h"

// the scenery of each zone. the tunnel has its own file, this one holds the
// plain, the swarm's sky, and the arena of the core. nothing is stored,
// everything is computed from where the eye is, and what looks random is a
// hash of the place, so that it is the same every time the eye comes by

// the plain. a grid on the floor, monoliths standing on it, and three lines
// high in the sky that give the speed
#define GRID_STEP        8.0
#define GRID_REACH       120.0
#define GRID_BEHIND      12.0
#define GRID_LIGHT       0.55
#define MONO_CELL        36.0
#define MONO_REACH       150.0
#define MONO_CLEAR       14.0     // no monolith closer to the rail than this
#define MONO_ODDS        0.45
#define MONO_HEIGHT_MIN  8.0
#define MONO_HEIGHT_MAX  46.0
#define MONO_WIDTH_MIN   2.5
#define MONO_WIDTH_MAX   6.0
#define MONO_PULSE       0.2      // each one breathes by this much, this fast
#define MONO_PULSE_RATE  2.0
#define SKY_LINE_Y       34.0
#define SKY_LINE_X       45.0
#define SKY_LINES        2        // this many each side of the middle one
#define SKY_LINE_LIGHT   0.35

// the swarm's sky. a huge solid around the eye that turns slowly, dust
// that stands still while the eye flies through it, and streaks along the
// rail
#define SHELL_RADIUS     72.0
#define SHELL_TURN       0.03     // radians per second
#define SHELL_LIGHT      0.8
#define DUST_CELL        14.0
#define DUST_REACH       80.0
#define DUST_SIZE        0.16
#define DUST_NEAR        12.0     // a mote nearer than this would be a blob
#define DUST_BEHIND      0.2      // how much of the reach is kept behind the eye
#define DUST_LIGHT       0.6
#define STREAK_SPACING   0.06     // rail units between streaks
#define STREAK_AHEAD     3.0
#define STREAK_LENGTH    6.0
#define STREAK_RADIUS_MIN 9.0
#define STREAK_RADIUS_MAX 30.0
#define STREAK_LIGHT     0.5
#define STREAK_BEHIND    0.2      // rail units of streaks kept behind the eye

// the arena of the core. a sphere of lines around the fight, a ring inside
// it turning the other way, and the eye on its orbit inside
#define ARENA_RADIUS     44.0
#define ARENA_LATITUDES  7
#define ARENA_LONGITUDES 12
#define ARENA_SEGMENTS   24
#define ARENA_TURN       0.05
#define ARENA_RING_RADIUS 34.0
#define ARENA_RING_TILT  0.5
#define ARENA_LIGHT      0.7
#define ARENA_GROWTH     0.25     // the sphere grows by this much over the fight
#define ARENA_WEAK       0.5      // the lesser lines, as a fraction of the light
#define ARENA_MERIDIAN   0.4
#define ARENA_RING_LIGHT 0.8
#define ORBIT_RADIUS_EYE 22.0

void world_draw(const struct camera *cam, const struct rail *rail, double t_eye,
		double now, int zone, const struct palette *pal);
// where the floor of the plain lies, told by the level once the rail is laid
void world_set_floor(double y);
// the void, dust and streaks and the far shell, at this much of its light
void world_void(const struct camera *cam, const struct rail *rail, double t_eye,
		double now, const struct palette *pal, double gain);
// the arena drawn around a centre, for the last zone and the boss
void world_arena(const struct camera *cam, struct vec centre, double now,
		 const struct palette *pal, double open);
// where the arena stands, at the end of the rail, and the orbit the eye
// takes around it
struct vec world_arena_centre(const struct rail *rail);
// a number from zero to one that depends only on its inputs
#define HASH_PRIME_A     374761393u
#define HASH_PRIME_B     668265263u
#define HASH_PRIME_C     2246822519u
#define HASH_PRIME_D     1274126177u
#define HASH_SHIFT_A     13
#define HASH_SHIFT_B     16
#define HASH_BITS        24
#define HASH_MASK        ((1u << HASH_BITS) - 1)
double world_hash(int first, int second, int third);

#endif
