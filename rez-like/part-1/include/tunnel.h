#ifndef TUNNEL_H
#define TUNNEL_H

#include "draw.h"
#include "palette.h"
#include "rail.h"

// the tunnel is rings of light strung along the rail, joined by long lines.
// nothing is stored, every ring is computed from the rail when it is drawn
#define RING_SIDES      12
// rings are this far apart along the rail, in rail units, a rail unit being
// the distance between two points of the curve
#define RING_SPACING    0.12
// how far ahead of the eye the rings are drawn, in rail units. beyond it the
// fog has eaten everything anyway
#define RING_AHEAD      2.6
#define RING_BEHIND     0.15
// the tunnel radius is the unit of a place across the rail, and the eye
// sits below the rail by this much of it, the floor nearer than the ceiling
#define TUNNEL_RADIUS   6.0
#define EYE_DROP        1.2
// the tunnel breathes on the beat, by this much of its radius
#define BREATH          0.08
// every this many rings, one is brighter. it gives the eye a sense of speed
#define STRONG_EVERY    4
// the weak rings and the long lines, as fractions of the zone's light
#define WEAK_RING       0.45
#define RAIL_LIT        0.5
#define RAIL_DIM        0.22
// a strong ring flashes white on the kick, for this much of the beat, and
// this bright at the kick itself
#define FLASH_PHASE     0.25
#define FLASH_GAIN      0.6

void tunnel_draw(const struct camera *cam, const struct rail *rail, double t_eye,
		 double now, const struct palette *pal);
// the colour of the scenery at t, turning through the zone's slice of hues
struct light tunnel_colour(const struct rail *rail, double t, const struct palette *pal);

#endif
