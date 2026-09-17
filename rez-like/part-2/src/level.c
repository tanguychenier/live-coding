#include <math.h>
#include <string.h>

#include "level.h"
#include "sound.h"
#include "thing.h"

static const int BARS[ZONES] = { UPLINK_BARS, FIELD_BARS, SWARM_BARS, APPROACH_BARS };
static const double WAVE_BEATS[ZONES] = { UPLINK_WAVE_BEATS, FIELD_WAVE_BEATS,
					  SWARM_WAVE_BEATS, 0.0 };

// the choreography of the rail, zone by zone. the tunnel teaches the eye
// what a bend, a dive and a sprint are. the plain is long sweeping turns
// at speed, the swarm is the storm, climbs, dives and hard turns in the
// open, and the approach runs straight and level into the core
static const struct move UPLINK_MOVES[] = {
	{ 4, 0.0, 0.0, PACE_NORMAL, 0 },
	{ 5, TURN_SOFT, 0.0, PACE_NORMAL, 0 },
	{ 3, 0.0, -PITCH_HARD, PACE_FAST, 0 },
	{ 3, 0.0, 0.0, PACE_SPRINT, 0 },
	{ 3, 0.0, PITCH_HARD, PACE_FAST, 0 },
	{ 4, -TURN_SOFT, PITCH_SOFT, PACE_CRUISE, 0 },
	{ 4, -TURN_SOFT, -PITCH_SOFT, PACE_CRUISE, 0 },
	{ 6, 0.0, 0.0, PACE_NORMAL, 1 },
	{ 4, TURN_HARD, 0.0, PACE_FAST, 1 },
	{ 4, TURN_SOFT, PITCH_SOFT, PACE_NORMAL, 0 },
	{ 4, -TURN_SOFT, -PITCH_SOFT, PACE_NORMAL, 0 },
	{ 3, -TURN_HARD, 0.0, PACE_SPRINT, 0 },
	{ 3, 0.0, -PITCH_SOFT, PACE_SPRINT, 0 },
	{ 3, 0.0, PITCH_SOFT, PACE_FAST, 0 },
	{ 3, TURN_SOFT, 0.0, PACE_NORMAL, 0 },
};
// a hill on the plain is a climb, a levelling, a dive and a levelling, so
// that the heading and the height both come back to where they were and
// the floor stays in sight
static const struct move FIELD_MOVES[] = {
	{ 6, 0.0, 0.0, PACE_NORMAL, 0 },
	{ 8, TURN_SOFT, 0.0, PACE_FAST, 0 },
	{ 3, 0.0, PITCH_SOFT / 2.0, PACE_FAST, 0 },
	{ 3, 0.0, -PITCH_SOFT / 2.0, PACE_SPRINT, 0 },
	{ 3, 0.0, -PITCH_SOFT / 2.0, PACE_SPRINT, 0 },
	{ 3, 0.0, PITCH_SOFT / 2.0, PACE_FAST, 0 },
	{ 8, -TURN_SOFT, 0.0, PACE_NORMAL, 0 },
	{ 6, -TURN_SOFT, 0.0, PACE_CRUISE, 0 },
	{ 3, 0.0, PITCH_SOFT / 2.0, PACE_NORMAL, 0 },
	{ 3, TURN_SOFT, -PITCH_SOFT / 2.0, PACE_FAST, 0 },
	{ 3, TURN_SOFT, -PITCH_SOFT / 2.0, PACE_FAST, 0 },
	{ 3, 0.0, PITCH_SOFT / 2.0, PACE_NORMAL, 0 },
	{ 8, 0.0, 0.0, PACE_SPRINT, 0 },
	{ 8, TURN_SOFT, 0.0, PACE_NORMAL, 0 },
	{ 8, -TURN_SOFT, 0.0, PACE_NORMAL, 0 },
};
static const struct move SWARM_MOVES[] = {
	{ 4, 0.0, 0.0, PACE_NORMAL, 1 },
	{ 6, TURN_SOFT, PITCH_SOFT, PACE_FAST, 1 },
	{ 6, TURN_SOFT, -PITCH_SOFT, PACE_FAST, 1 },
	{ 5, 0.0, 0.0, PACE_SPRINT, 1 },
	{ 6, -TURN_SOFT, -PITCH_SOFT, PACE_NORMAL, 1 },
	{ 6, -TURN_SOFT, PITCH_SOFT, PACE_NORMAL, 1 },
	{ 5, 0.0, 0.0, PACE_CRUISE, 1 },
	{ 6, TURN_HARD, 0.0, PACE_SPRINT, 1 },
	{ 6, -TURN_HARD, 0.0, PACE_SPRINT, 1 },
	{ 5, 0.0, PITCH_SOFT, PACE_FAST, 1 },
	{ 5, 0.0, -PITCH_SOFT, PACE_FAST, 1 },
	{ 5, 0.0, 0.0, PACE_NORMAL, 1 },
};
static const struct move APPROACH_MOVES[] = {
	{ 4, 0.0, 0.0, PACE_NORMAL, 1 },
};
static const struct move *const MOVES[ZONES] = {
	UPLINK_MOVES, FIELD_MOVES, SWARM_MOVES, APPROACH_MOVES };
static const int MOVE_COUNTS[ZONES] = {
	(int)(sizeof UPLINK_MOVES / sizeof *UPLINK_MOVES),
	(int)(sizeof FIELD_MOVES / sizeof *FIELD_MOVES),
	(int)(sizeof SWARM_MOVES / sizeof *SWARM_MOVES),
	(int)(sizeof APPROACH_MOVES / sizeof *APPROACH_MOVES),
};

// the pace of each zone in rail units per second at a move of pace one,
// and the table of where the eye is at every twentieth of a second
static double base_pace[ZONES];
static double eye_table[TABLE_MAX];
static int eye_table_count;

double level_zone_time(int zone)
{
	double t = 0.0;
	for (int z = 0; z < zone && z < ZONES; z++)
		t += BARS[z] * BAR;
	return t;
}

int level_zone(double now)
{
	int zone = 0;
	for (int z = 1; z < ZONES; z++)
		if (now >= level_zone_time(z))
			zone = z;
	return zone;
}

// a zone lasts its bars whatever its length, so the eye flies it at the
// speed that makes the two agree
double level_speed(const struct rail *rail, double t)
{
	return base_pace[rail_zone_at(rail, t)] * rail_pace(rail, t);
}

void level_build_rail(struct rail *rail)
{
	rail_build(rail, ZONES, MOVES, MOVE_COUNTS);
	// a zone lasts its bars whatever its moves, so its pace is the time its
	// moves would take at pace one, over the time it has
	for (int z = 0; z < ZONES; z++) {
		double slow_time = 0.0;
		for (int i = (int)rail->zone_start[z]; i < (int)rail->zone_start[z + 1]; i++)
			slow_time += 1.0 / rail->pace[i];
		base_pace[z] = slow_time / (BARS[z] * BAR);
	}
	// the eye flies the whole level once, here, and remembers where it was
	double t = 0.0;
	eye_table_count = 0;
	for (int z = 0; z < ZONES; z++) {
		t = rail->zone_start[z];
		int steps = (int)(BARS[z] * BAR * TABLE_HZ);
		for (int i = 0; i < steps && eye_table_count < TABLE_MAX; i++) {
			eye_table[eye_table_count++] = t;
			t += base_pace[z] * rail_pace(rail, t) / TABLE_HZ;
		}
	}
	// the floor of the plain sits under the lowest point the rail reaches
	// there
	double lowest = 1e9;
	for (int i = (int)rail->zone_start[ZONE_FIELD]; i < (int)rail->zone_start[ZONE_FIELD + 1]; i++)
		if (rail->point[i].y < lowest)
			lowest = rail->point[i].y;
}

// past the end the level is flown again from its start
double level_t_eye(const struct rail *rail, double now)
{
	double index = now * TABLE_HZ;
	if (index < 0.0)
		index = 0.0;
	int i = (int)index;
	if (i >= eye_table_count - 1)
		return rail_end(rail);
	double t = eye_table[i] + (eye_table[i + 1] - eye_table[i]) * (index - i);
	double end = rail_end(rail);
	return t > end ? end : t;
}

void level_reset(struct level *level, double now, long score)
{
	memset(level, 0, sizeof *level);
	level->zone = level_zone(now);
	level->zone_started = now;
	level->zone_score = score;
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
		switch (wave % 5) {
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
		case 2:
			thing_spawn(SHAPE_SPINDLE, MOTION_SHOOTER, t, 0.0, 0.35, 0.22, 1.1, 2, pal->enemy_alt);
			break;
		case 3:
			spawn_ring(SHAPE_OCTA, MOTION_HOVER, t, 6, 0.55, slow, 0.9, 1, pal->enemy, pal->enemy_alt);
			break;
		default:
			for (int i = 0; i < 2; i++) {
				struct thing *thing = thing_spawn(SHAPE_MANTA, MOTION_CROSS, t + i * 0.2, 0,
							       i ? -0.3 : 0.3, 0.36, 1.0, 1, pal->enemy);
				if (thing)
					thing->phase = i * 1.7;
			}
			thing_spawn(SHAPE_RING, MOTION_GATE, t + 0.6, 0, 0, slow, 4.0, 1, LIGHT_GATE);
			break;
		}
	}
}

// the plain is wide, so the waves spread wide. turrets in pairs, mantas
// that cross, rings of eight for the chain, and the gates
static void wave_field(int wave, double t, const struct palette *pal)
{
	const double speed = 0.34;
	switch (wave % 6) {
	case 0:
		spawn_ring(SHAPE_OCTA, MOTION_HOVER, t, 8, 0.6, speed, 0.9, 1, pal->enemy, pal->enemy_alt);
		break;
	case 1:
		for (int i = 0; i < 2; i++)
			thing_spawn(SHAPE_SPINDLE, MOTION_SHOOTER, t + i * 0.1, i ? 0.6 : -0.6, 0.3,
				    0.24, 1.1, 2, pal->enemy_alt);
		break;
	case 2:
		for (int i = 0; i < 3; i++) {
			struct thing *thing = thing_spawn(SHAPE_MANTA, MOTION_CROSS, t + i * 0.15, 0,
						       0.35 * (i - 1), 0.4, 1.0, 1, pal->enemy);
			if (thing)
				thing->phase = i * 1.3;
		}
		break;
	case 3:
		thing_spawn(SHAPE_DIAMOND, MOTION_HOVER, t, 0.0, 0.1, speed, 1.3, 3, pal->enemy_alt);
		for (int i = 0; i < 4; i++)
			spawn_phase(SHAPE_OCTA, MOTION_ORBIT, t + 0.1, speed, 0.85, 1, pal->enemy, i * M_PI / 2);
		break;
	case 4:
		for (int i = 0; i < 5; i++)
			thing_spawn(SHAPE_CUBE, MOTION_DIVE, t + DIVE_AHEAD + i * 0.12, (i - 2) * 0.4, 0.3,
				    DIVE_SPEED, 0.8, 1, i % 2 ? pal->enemy_alt : pal->enemy);
		break;
	default:
		thing_spawn(SHAPE_RING, MOTION_GATE, t + 0.3, 0, 0, speed, 4.0, 1, LIGHT_GATE);
		thing_spawn(SHAPE_SPINDLE, MOTION_SHOOTER, t + 0.8, 0.0, -0.3, 0.24, 1.1, 2, pal->enemy_alt);
		break;
	}
}

// the swarm is numbers. eight drones at once, again and again, cubes that
// dive, and turrets hidden in the crowd
static void wave_swarm(int wave, double t, const struct palette *pal)
{
	const double speed = 0.4;
	switch (wave % 4) {
	case 0:
		spawn_ring(SHAPE_TETRA, MOTION_DIVE, t + DIVE_AHEAD, 8, 0.7, DIVE_SPEED, 0.7, 1,
			   pal->enemy, pal->enemy_alt);
		break;
	case 1:
		for (int i = 0; i < 8; i++)
			spawn_phase(SHAPE_TETRA, MOTION_ORBIT, t + (i % 4) * 0.08, speed, 0.7, 1,
				    i % 2 ? pal->enemy_alt : pal->enemy, i * M_PI / 4);
		thing_spawn(SHAPE_SPINDLE, MOTION_SHOOTER, t + 0.4, 0.0, 0.0, 0.26, 1.1, 2, pal->enemy_alt);
		break;
	case 2:
		for (int i = 0; i < 6; i++)
			thing_spawn(SHAPE_CUBE, MOTION_DIVE, t + DIVE_AHEAD + i * 0.1, 0.5 * cos(i),
				    0.5 * sin(i), DIVE_SPEED, 0.75, 1, pal->enemy);
		spawn_ring(SHAPE_TETRA, MOTION_HOVER, t + 0.5, 6, 0.6, speed, 0.7, 1, pal->enemy_alt, pal->enemy);
		break;
	default:
		spawn_ring(SHAPE_TETRA, MOTION_DIVE, t + DIVE_AHEAD, 8, 0.65, DIVE_SPEED, 0.7, 1,
			   pal->enemy_alt, pal->enemy);
		thing_spawn(SHAPE_RING, MOTION_GATE, t + 0.5, 0, 0, speed, 4.0, 1, LIGHT_GATE);
		for (int i = 0; i < 2; i++)
			thing_spawn(SHAPE_SPINDLE, MOTION_SHOOTER, t + 0.8, i ? 0.55 : -0.55, 0.0,
				    0.26, 1.1, 2, pal->enemy_alt);
		break;
	}
}

void level_update(struct level *level, const struct rail *rail, double t_eye,
		  double now, long score)
{
	int zone = level_zone(now);
	if (zone != level->zone) {
		level->zone = zone;
		level->wave = 0;
		level->zone_started = now;
		level->zone_score = score;
		level->next_wave = now + FIRST_WAVE_BEATS * BEAT;
	}
	if (WAVE_BEATS[zone] <= 0.0 || now < level->next_wave)
		return;
	if (t_eye + WAVE_AHEAD >= rail->zone_start[ZONE_CORE])
		return;
	const struct palette *pal = palette_of(zone);
	double t = t_eye + WAVE_AHEAD;
	switch (zone) {
	case ZONE_UPLINK: wave_uplink(level->wave, t, pal); break;
	case ZONE_FIELD:  wave_field(level->wave, t, pal); break;
	default:          wave_swarm(level->wave, t, pal); break;
	}
	level->wave++;
	level->next_wave = now + WAVE_BEATS[zone] * BEAT;
}
