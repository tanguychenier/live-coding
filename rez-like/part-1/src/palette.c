#include "palette.h"

// the hue is a place on the colour wheel from zero to one, red at zero,
// green at a third, blue at two thirds. the tunnel is cold
static const struct palette PALETTES[ZONES] = {
	{ "UPLINK", 0.62, 0.16,
	  { 0.45, 0.4, 1.0 },
	  { 0.35, 0.9, 1.1 }, { 1.0, 0.85, 0.4 }, { 1.3, 0.5, 0.4 },
	  { 0.0, 0.0, 0.01 }, { 0.01, 0.0, 0.03 }, 38.0 },
};

const struct palette *palette_of(int zone)
{
	return &PALETTES[zone < 0 ? 0 : zone >= ZONES ? ZONES - 1 : zone];
}
