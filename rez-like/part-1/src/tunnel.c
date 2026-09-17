#include <math.h>

#include "tunnel.h"

// the hue is a position, not a time, so a place always has its colour and
// the eye sees where it is going by the colour ahead
struct light tunnel_colour(const struct rail *rail, double t, const struct palette *pal)
{
	int zone = rail_zone_at(rail, t);
	double start = rail->zone_start[zone], end = rail->zone_start[zone + 1];
	double along = end > start ? (t - start) / (end - start) : 0.0;
	return light_hue(pal->hue + pal->hue_span * along);
}

void tunnel_draw(const struct camera *cam, const struct rail *rail, double t_eye,
		 double now, const struct palette *pal)
{
	(void)now;
	double end = rail_end(rail);
	// rings sit on a fixed grid of t, so they do not slide with the eye
	double first = floor((t_eye - RING_BEHIND) / RING_SPACING) * RING_SPACING;
	struct vec prev[RING_SIDES];
	int have_prev = 0;
	int ring_index = (int)floor(first / RING_SPACING + 0.5);
	for (double t = first; t < t_eye + RING_AHEAD && t <= end;
	     t += RING_SPACING, ring_index++) {
		struct vec centre = rail_at(rail, t), forward, right, up;
		rail_frame(rail, t, &forward, &right, &up);
		struct light colour = tunnel_colour(rail, t, pal);
		int strong = ring_index % STRONG_EVERY == 0;
		double gain = strong ? 1.0 : WEAK_RING;
		struct vec ring[RING_SIDES];
		for (int i = 0; i < RING_SIDES; i++) {
			double angle = 2.0 * M_PI * i / RING_SIDES;
			ring[i] = add(centre, add(scale(right, TUNNEL_RADIUS * cos(angle)),
						  scale(up, TUNNEL_RADIUS * sin(angle))));
		}
		struct light lit = light_scale(colour, gain);
		for (int i = 0; i < RING_SIDES; i++)
			draw_line(cam, ring[i], ring[(i + 1) % RING_SIDES], lit);
		if (have_prev) {
			// the long lines are fainter than the rings, they are the
			// rails of the tunnel, and every third one is lit a little more
			for (int i = 0; i < RING_SIDES; i++) {
				double gain = i % 3 == 0 ? RAIL_LIT : RAIL_DIM;
				draw_line(cam, prev[i], ring[i], light_scale(colour, gain));
			}
		}
		for (int i = 0; i < RING_SIDES; i++)
			prev[i] = ring[i];
		have_prev = 1;
	}
}
