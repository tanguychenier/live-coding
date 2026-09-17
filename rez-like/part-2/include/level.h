#ifndef LEVEL_H
#define LEVEL_H

#include "draw.h"
#include "palette.h"
#include "rail.h"

// the level is a timeline in bars of music. a zone lasts so many bars and
// flies the rail at its own pace, and the rail is laid to be exactly that
// long, so that the place and the moment always agree
#define UPLINK_BARS      64      // two minutes, the tunnel that teaches
#define FIELD_BARS       80      // two and a half, the open plain
#define SWARM_BARS       80      // two and a half, the storm of small things
#define APPROACH_BARS    4       // the straight run into the core, a breath
// the eye's place on the rail is read from a table of the whole level,
// this many entries a second, filled once by flying the rail at the pace
// of each move
#define TABLE_HZ         20
#define TABLE_MAX        12000
// how far ahead of the eye a wave appears, in rail units. far enough to be
// seen coming, near enough to be in the fog's reach
#define WAVE_AHEAD       1.8
// the gap between two waves, in beats. never more than a few seconds
// without a target, that was the rule
#define UPLINK_WAVE_BEATS   5.0
#define FIELD_WAVE_BEATS    4.0
#define SWARM_WAVE_BEATS    3.0
// the first wave waits this long after the zone begins, in beats
#define FIRST_WAVE_BEATS    4.0
// what dives at the eye comes at this many rail units per second on top of
// the eye's own speed, and appears this much further ahead than the rest,
// so that there are four seconds to mark it
#define DIVE_SPEED       0.12
#define DIVE_AHEAD       0.6
// a gate is worth this much, on top of the chain
#define SCORE_GATE       500

struct level {
	int zone;
	int wave;
	double next_wave;        // on the music clock
	double zone_started;
	long zone_score;         // the score when the zone began, for a retry
};

void level_build_rail(struct rail *rail);
// when a zone begins, in seconds on the music clock
double level_zone_time(int zone);
// how fast the eye flies at a place on the rail, in rail units per second
double level_speed(const struct rail *rail, double t);
int level_zone(double now);
// where the eye is on the rail at a moment
double level_t_eye(const struct rail *rail, double now);
void level_reset(struct level *level, double now, long score);
// spawns what the zone wants, when it wants it
void level_update(struct level *level, const struct rail *rail, double t_eye,
		  double now, long score);

#endif
