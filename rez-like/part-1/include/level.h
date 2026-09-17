#ifndef LEVEL_H
#define LEVEL_H

#include "draw.h"
#include "palette.h"
#include "rail.h"

// the level is a timeline in bars of music. a zone lasts so many bars and
// flies the rail at its own pace, and the rail is laid to be exactly that
// long, so that the place and the moment always agree
#define UPLINK_BARS      64      // two minutes, the tunnel that teaches
// how far ahead of the eye a wave appears, in rail units. far enough to be
// seen coming, near enough to be in the fog's reach
#define WAVE_AHEAD       1.8
// the gap between two waves, in beats. never more than a few seconds
// without a target, that was the rule
#define UPLINK_WAVE_BEATS   5.0
// the first wave waits this long after the zone begins, in beats
#define FIRST_WAVE_BEATS    4.0

void level_build_rail(struct rail *rail);
// when a zone begins, in seconds on the music clock
double level_zone_time(int zone);
// how fast the eye flies at a place on the rail, in rail units per second
double level_speed(const struct rail *rail, double t);
int level_zone(double now);
// where the eye is on the rail at a moment
double level_t_eye(const struct rail *rail, double now);

#endif
