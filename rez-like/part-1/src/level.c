#include <math.h>
#include <string.h>

#include "level.h"
#include "sound.h"

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
