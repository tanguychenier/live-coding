#ifndef PALETTE_H
#define PALETTE_H

#include "draw.h"

// every colour of the game lives here, as light in floating point. a
// component above one is allowed, it is light that the bloom will spread

// the zones, one for now, the tunnel
#define ZONES        1
#define ZONE_UPLINK  0

struct palette {
	const char *name;          // written on the screen when the zone starts
	double hue, hue_span;      // the scenery turns through this slice of the wheel
	struct light rails;        // the long lines of the scenery, fainter
	struct light enemy;        // the common target
	struct light enemy_alt;    // its companion colour, every other one
	struct light bolt;         // what the enemies fire at the eye
	struct light sky_top;      // the background, a gradient, top and bottom
	struct light sky_bottom;
	double fog;                // at this depth a line keeps a third of its light
};

const struct palette *palette_of(int zone);

// the colours that do not change with the zone
#define LIGHT_WHITE        light(1.0, 1.0, 1.0)
#define LIGHT_HERO         light(0.4, 0.6, 0.95)       // the pilot, ice blue
#define LIGHT_HERO_FACE    light(0.10, 0.14, 0.22)    // the faint skin of his body
#define LIGHT_HERO_TRAIL   light(0.5, 0.8, 1.3)
#define LIGHT_CURSOR       light(0.7, 0.85, 1.0)
#define LIGHT_CURSOR_HOT   light(1.3, 1.3, 1.5)       // while it marks
#define LIGHT_LOCK         light(1.5, 1.1, 0.5)       // the bracket on a marked target
#define LIGHT_SHOT         light(1.9, 1.6, 0.9)
#define LIGHT_HIT          light(1.6, 1.6, 1.6)       // a target washed white by a hit

#endif
