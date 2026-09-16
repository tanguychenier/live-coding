#include <math.h>
#include <stdio.h>
#include <time.h>

#include "demo.h"
#include "fight.h"
#include "thing.h"

static double route_x[ROUTE_MAX], route_y[ROUTE_MAX];
static int points, leg;
// how close it has come to the point so far, and when it last got closer
static double best_yet, better_at;
// the die, whether the first point has been drawn yet, and the pause at a
// point, with the moment it walks on, the moment it stops turning its head,
// and which way it turns
static unsigned int dice;
static int started, glance;
static double linger_until, glance_until;

static double roll(void)
{
	dice ^= dice << DICE_SHIFT_A;
	dice ^= dice >> DICE_SHIFT_B;
	dice ^= dice << DICE_SHIFT_C;
	return (double)(dice & DICE_MASK) / (DICE_MASK + 1.0);
}

void demo_reset(void)
{
	leg = 0;
	best_yet = -1.0;
	better_at = 0.0;
	started = 0;
	linger_until = glance_until = 0.0;
}

int demo_load(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f)
		return 0;
	points = 0;
	double x, y;
	while (points < ROUTE_MAX && fscanf(f, "%lf %lf", &x, &y) == 2) {
		route_x[points] = x;
		route_y[points] = y;
		points++;
	}
	fclose(f);
	dice = (unsigned int)time(NULL);
	demo_reset();
	return points;
}

// the angle from where the eyes point to where the target is. positive is
// to the right, the way the rotation goes, and it is what the arrow keys
// have to undo
static double bearing(const struct player *p, double x, double y)
{
	double dx = x - p->x, dy = y - p->y;
	return atan2(p->dir_x * dy - p->dir_y * dx, p->dir_x * dx + p->dir_y * dy);
}

// a straight line to it, with no wall in between
static int clear_to(const struct player *p, double x, double y)
{
	double dx = x - p->x, dy = y - p->y;
	double d = hypot(dx, dy);
	for (double t = 0.3; t < d; t += 0.3)
		if (is_wall((int)(p->x + dx / d * t), (int)(p->y + dy / d * t)))
			return 0;
	return 1;
}

static const struct thing *nearest_live(const struct player *p)
{
	const struct thing *best = 0;
	double nearest = DEMO_ENGAGE;
	for (int i = 0; i < THINGS_MAX; i++) {
		const struct thing *t = &things[i];
		if (!t->used || t->state == THING_DEAD)
			continue;
		double d = hypot(t->x - p->x, t->y - p->y);
		if (d < nearest && clear_to(p, t->x, t->y)) {
			nearest = d;
			best = t;
		}
	}
	return best;
}

// where it starts is not always the first point. among the first few, one
// that is in plain sight, drawn at random, so that two runs do not walk the
// same line from the first second
static void pick_start(const struct player *p)
{
	int seen[DEMO_START_SPAN], n = 0;
	for (int i = 0; i < DEMO_START_SPAN && i < points; i++)
		if (clear_to(p, route_x[i], route_y[i]))
			seen[n++] = i;
	leg = n > 0 ? seen[(int)(roll() * n)] : 0;
}

// at a point it sometimes stops and looks around a little, the way someone
// who is not in a hurry does before walking on
static void next_point(double now)
{
	leg++;
	best_yet = -1.0;
	better_at = now;
	if (roll() < DEMO_LINGER_ODDS) {
		glance = roll() < EVEN_ODDS ? -1 : 1;
		glance_until = now + DEMO_GLANCE;
		linger_until = glance_until + DEMO_LINGER_MIN
			+ roll() * (DEMO_LINGER_MAX - DEMO_LINGER_MIN);
	}
}

// face it. in reach, swing; with the bar, walk in first
static void fight_it(const struct player *p, struct keys *keys,
		     const struct thing *t, double now)
{
	double turn = bearing(p, t->x, t->y);
	double d = hypot(t->x - p->x, t->y - p->y);
	double aim = atan2(DEMO_BODY, d);
	keys->right = turn > aim;
	keys->left = turn < -aim;
	double reach = fight_gun_out() ? GUN_RANGE : PIPE_REACH;
	double stop = fight_gun_out() ? GUN_RANGE : DEMO_STANDOFF;
	if (fabs(turn) < aim && d < reach)
		keys->hit = 1;
	keys->forward = fabs(turn) < DEMO_WALK_CONE && d > stop;
	// a fight is not time lost on the route
	better_at = now;
}

// nothing in the way: turn to the next point, and walk when it is ahead
static void follow_route(const struct player *p, struct keys *keys, double now)
{
	if (now < linger_until) {
		keys->left = glance < 0 && now < glance_until;
		keys->right = glance > 0 && now < glance_until;
		better_at = now;
		return;
	}
	double turn = bearing(p, route_x[leg], route_y[leg]);
	double d = hypot(route_x[leg] - p->x, route_y[leg] - p->y);
	keys->right = turn > DEMO_TURN_TOL;
	keys->left = turn < -DEMO_TURN_TOL;
	keys->forward = fabs(turn) < DEMO_WALK_CONE;
	if (d < DEMO_ARRIVED) {
		next_point(now);
	} else if (best_yet < 0.0 || d < best_yet - DEMO_ARRIVED / 4) {
		best_yet = d;
		better_at = now;
	} else if (now - better_at > DEMO_PATIENCE) {
		next_point(now);
	}
}

void demo_drive(const struct player *p, struct keys *keys, double now)
{
	keys->forward = keys->back = keys->left = keys->right = 0;
	if (leg >= points)
		return;
	if (!started) {
		pick_start(p);
		started = 1;
	}
	const struct thing *t = nearest_live(p);
	if (t)
		fight_it(p, keys, t, now);
	else
		follow_route(p, keys, now);
	if (door_ahead(p))
		keys->push = 1;
}
