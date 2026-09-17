#include <math.h>

#include "rail.h"

// the point before the first and the one after the last are the ends
// themselves, so the curve starts and stops without a kink
static struct vec point_at(const struct rail *rail, int i)
{
	if (i < 0)
		i = 0;
	if (i >= rail->count)
		i = rail->count - 1;
	return rail->point[i];
}

struct vec rail_at(const struct rail *rail, double t)
{
	if (t < 0.0)
		t = 0.0;
	if (t > rail_end(rail))
		t = rail_end(rail);
	int i = (int)t;
	double u = t - i, uu = u * u, uuu = u * u * u;
	struct vec p0 = point_at(rail, i - 1), p1 = point_at(rail, i);
	struct vec p2 = point_at(rail, i + 1), p3 = point_at(rail, i + 2);
	// the four weights of catmull rom, a cubic in u for each of the four
	// neighbours. they add up to one, so the curve stays between the points
	double w0 = -0.5 * uuu + uu - 0.5 * u;
	double w1 = 1.5 * uuu - 2.5 * uu + 1.0;
	double w2 = -1.5 * uuu + 2.0 * uu + 0.5 * u;
	double w3 = 0.5 * uuu - 0.5 * uu;
	return add(add(scale(p0, w0), scale(p1, w1)),
		   add(scale(p2, w2), scale(p3, w3)));
}

struct vec rail_forward(const struct rail *rail, double t)
{
	// a small step ahead and one behind, the direction is the line between
	const double step = 0.01;
	return unit(sub(rail_at(rail, t + step), rail_at(rail, t - step)));
}

void rail_frame(const struct rail *rail, double t, struct vec *forward,
		struct vec *right, struct vec *up)
{
	*forward = rail_forward(rail, t);
	// up is the world's up bent square to the rail. a rail never goes
	// straight up in this level, so the cross product never collapses
	*right = unit(cross(*forward, vec(0, 1, 0)));
	*up = cross(*right, *forward);
}

double rail_end(const struct rail *rail)
{
	return rail->count - 1.0;
}

int rail_zone_at(const struct rail *rail, double t)
{
	int zone = 0;
	for (int z = 1; z < rail->zones; z++)
		if (t >= rail->zone_start[z])
			zone = z;
	return zone;
}

// the rail is flown like a craft. a heading turns and pitches by what each
// move asks, and every point is one spacing further along the heading
void rail_build(struct rail *rail, int zones, const struct move *const moves[],
		const int move_counts[])
{
	rail->zones = zones;
	rail->count = 0;
	double yaw = 0.0, pitch = 0.0;
	struct vec at = vec(0, 0, 0);
	for (int z = 0; z < zones; z++) {
		rail->zone_start[z] = rail->count;
		for (int m = 0; m < move_counts[z]; m++) {
			const struct move *move = &moves[z][m];
			for (int p = 0; p < move->points && rail->count < RAIL_POINTS_MAX; p++) {
				rail->point[rail->count] = at;
				rail->count++;
				yaw += move->turn;
				pitch += move->pitch;
				struct vec heading = vec(sin(yaw) * cos(pitch), sin(pitch),
							 cos(yaw) * cos(pitch));
				at = add(at, scale(heading, RAIL_SPACING));
			}
		}
	}
	rail->zone_start[zones] = rail->count;
}
