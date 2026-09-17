#include <math.h>
#include <string.h>

#include "level.h"
#include "sound.h"
#include "thing.h"

// the choreography of the rail. the tunnel teaches the eye what a bend and
// a dive are. it is one zone for now, flown again from the start when its
// end comes
static const struct move UPLINK_MOVES[] = {
	{ 4, 0.0, 0.0 },
	{ 5, TURN_SOFT, 0.0 },
	{ 3, 0.0, -PITCH_HARD },
	{ 3, 0.0, 0.0 },
	{ 3, 0.0, PITCH_HARD },
	{ 4, -TURN_SOFT, PITCH_SOFT },
	{ 4, -TURN_SOFT, -PITCH_SOFT },
	{ 6, 0.0, 0.0 },
	{ 4, TURN_HARD, 0.0 },
	{ 4, TURN_SOFT, PITCH_SOFT },
	{ 4, -TURN_SOFT, -PITCH_SOFT },
	{ 3, -TURN_HARD, 0.0 },
	{ 3, 0.0, -PITCH_SOFT },
	{ 3, 0.0, PITCH_SOFT },
	{ 3, TURN_SOFT, 0.0 },
};
static const struct move *const MOVES[ZONES] = { UPLINK_MOVES };
static const int MOVE_COUNTS[ZONES] = { (int)(sizeof UPLINK_MOVES / sizeof *UPLINK_MOVES) };
static const int BARS[ZONES] = { UPLINK_BARS };

static const double WAVE_BEATS[ZONES] = { UPLINK_WAVE_BEATS };

double level_zone_time(int zone)
{
	double t = 0.0;
	for (int z = 0; z < zone && z < ZONES; z++)
		t += BARS[z] * BAR;
	return t;
}

int level_zone(double now)
{
	(void)now;
	return 0;
}

// a zone lasts its bars whatever its length, so the eye flies it at the
// speed that makes the two agree
double level_speed(const struct rail *rail, double t)
{
	int zone = rail_zone_at(rail, t);
	return (rail->zone_start[zone + 1] - rail->zone_start[zone]) / (BARS[zone] * BAR);
}

void level_build_rail(struct rail *rail)
{
	rail_build(rail, ZONES, MOVES, MOVE_COUNTS);
}

// past the end the level is flown again from its start
double level_t_eye(const struct rail *rail, double now)
{
	double loop = level_zone_time(ZONES);
	double t = fmod(now, loop) * level_speed(rail, 0.0);
	double end = rail_end(rail);
	return t > end ? end : t;
}

void level_reset(struct level *level, double now)
{
	memset(level, 0, sizeof *level);
	level->zone = level_zone(now);
	level->zone_started = now;
	level->next_wave = now + FIRST_WAVE_BEATS * BEAT;
}

// an orbiting thing needs its own starting angle, or four of them would
// stack on one point of the ring
static void spawn_phase(enum shape shape, enum motion motion, double t, double speed,
			double size, int health, struct light colour, double phase)
{
	struct thing *thing = thing_spawn(shape, motion, t, 0, 0, speed, size, health, colour);
	if (thing)
		thing->phase = phase;
}

// a ring of things around the rail, evenly spread, the wave that asks for
// a full chain
static void spawn_ring(enum shape shape, enum motion motion, double t, int count,
		       double radius, double speed, double size, int health,
		       struct light first, struct light second)
{
	for (int i = 0; i < count; i++) {
		double angle = 2 * M_PI * i / count;
		thing_spawn(shape, motion, t, radius * cos(angle), radius * sin(angle), speed,
			    size, health, i % 2 ? second : first);
	}
}

// the tunnel teaches. one target, then three, then the rings. the targets
// come slowly here
static void wave_uplink(int wave, double t, const struct palette *pal)
{
	const double slow = 0.30, size = 1.1;
	if (wave < 3) {
		thing_spawn(SHAPE_OCTA, MOTION_HOVER, t, 0.0, 0.05, slow, size, 1, pal->enemy);
	} else if (wave < 6) {
		for (int i = 0; i < 3; i++)
			thing_spawn(SHAPE_OCTA, MOTION_HOVER, t + i * 0.1, (i - 1) * 0.45, 0.0,
				    slow, size, 1, i == 1 ? pal->enemy_alt : pal->enemy);
	} else {
		switch (wave % 3) {
		case 0:
			for (int i = 0; i < 4; i++)
				spawn_phase(SHAPE_OCTA, MOTION_ORBIT, t + i * 0.05, slow, 0.9, 1,
					    i % 2 ? pal->enemy_alt : pal->enemy, i * M_PI / 2);
			break;
		case 1:
			thing_spawn(SHAPE_DIAMOND, MOTION_HOVER, t, 0.0, 0.0, slow, 1.2, 3, pal->enemy_alt);
			for (int i = 0; i < 2; i++)
				thing_spawn(SHAPE_OCTA, MOTION_HOVER, t + 0.15, i ? 0.5 : -0.5, 0.2,
					    slow, size, 1, pal->enemy);
			break;
		default:
			spawn_ring(SHAPE_OCTA, MOTION_HOVER, t, 6, 0.55, slow, 0.9, 1, pal->enemy, pal->enemy_alt);
			break;
		}
	}
}

void level_update(struct level *level, const struct rail *rail, double t_eye, double now)
{
	int zone = level_zone(now);
	if (zone != level->zone) {
		level->zone = zone;
		level->wave = 0;
		level->zone_started = now;
		level->next_wave = now + FIRST_WAVE_BEATS * BEAT;
	}
	if (now < level->next_wave)
		return;
	if (t_eye + WAVE_AHEAD >= rail_end(rail))
		return;
	wave_uplink(level->wave, t_eye + WAVE_AHEAD, palette_of(zone));
	level->wave++;
	level->next_wave = now + WAVE_BEATS[zone] * BEAT;
}
