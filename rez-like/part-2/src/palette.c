#include "palette.h"

// the hue is a place on the colour wheel from zero to one, red at zero,
// green at a third, blue at two thirds. the tunnel is cold
static const struct palette PALETTES[ZONES] = {
	{ "UPLINK", 0.62, 0.16,
	  { 0.45, 0.4, 1.0 },
	  { 0.35, 0.9, 1.1 }, { 1.0, 0.85, 0.4 }, { 1.3, 0.5, 0.4 },
	  { 0.0, 0.0, 0.01 }, { 0.01, 0.0, 0.03 }, 38.0 },
	{ "THE FIELD", 0.42, 0.10,
	  { 0.3, 0.9, 0.7 },
	  { 1.1, 0.5, 0.95 }, { 0.75, 1.0, 0.4 }, { 1.3, 0.6, 0.3 },
	  { 0.0, 0.0, 0.0 }, { 0.0, 0.05, 0.06 }, 70.0 },
	{ "SWARM", 0.78, 0.14,
	  { 0.9, 0.4, 1.0 },
	  { 1.2, 0.6, 0.3 }, { 0.4, 0.9, 1.2 }, { 1.3, 0.3, 0.5 },
	  { 0.01, 0.0, 0.02 }, { 0.02, 0.0, 0.05 }, 90.0 },
	{ "CORE", 0.98, 0.08,
	  { 1.1, 0.45, 0.3 },
	  { 1.3, 1.1, 0.9 }, { 1.2, 0.4, 0.3 }, { 1.4, 0.9, 0.3 },
	  { 0.03, 0.0, 0.0 }, { 0.05, 0.01, 0.0 }, 80.0 },
};

const struct palette *palette_of(int zone)
{
	return &PALETTES[zone < 0 ? 0 : zone >= ZONES ? ZONES - 1 : zone];
}
