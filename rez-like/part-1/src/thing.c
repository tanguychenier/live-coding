#include <math.h>
#include <string.h>

#include "palette.h"
#include "particle.h"
#include "sound.h"
#include "thing.h"

struct thing things[THINGS_MAX];
static unsigned int serials;

void things_clear(void)
{
	memset(things, 0, sizeof things);
}

static struct thing *free_slot(void)
{
	for (int i = 0; i < THINGS_MAX; i++)
		if (!things[i].used) {
			memset(&things[i], 0, sizeof things[i]);
			things[i].used = 1;
			things[i].phase = (double)i * PHASE_SPREAD;
			things[i].serial = ++serials;
			return &things[i];
		}
	return NULL;
}

struct thing *thing_by_serial(unsigned int serial)
{
	if (serial == 0)
		return NULL;
	for (int i = 0; i < THINGS_MAX; i++)
		if (things[i].used && things[i].serial == serial)
			return &things[i];
	return NULL;
}

struct thing *thing_spawn(enum shape shape, enum motion motion, double t, double u,
			  double v, double speed, double size, int health,
			  struct light colour)
{
	struct thing *thing = free_slot();
	if (!thing)
		return NULL;
	thing->shape = shape;
	thing->motion = motion;
	thing->t = t;
	thing->u = u;
	thing->v = v;
	thing->speed = speed;
	thing->size = size;
	thing->health = health;
	thing->colour = colour;
	return thing;
}

struct vec thing_world(const struct rail *rail, double t, double u, double v)
{
	struct vec centre = rail_at(rail, t), forward, right, up;
	rail_frame(rail, t, &forward, &right, &up);
	return add(centre, add(scale(right, u * TUNNEL_RADIUS),
			       scale(up, v * TUNNEL_RADIUS)));
}

int thing_hurt(struct thing *thing, double now)
{
	thing->health--;
	thing->hurt_at = now;
	if (thing->health > 0) {
		particles_burst(thing->at, HIT_SPARKS, HIT_SPEED, LIGHT_HIT);
		return 0;
	}
	particles_burst(thing->at, BURST_SPARKS, BURST_SPEED, thing->colour);
	thing->used = 0;
	return 1;
}

void things_update(const struct rail *rail, double t_eye, double elapsed, double now)
{
	for (int i = 0; i < THINGS_MAX; i++) {
		struct thing *thing = &things[i];
		if (!thing->used)
			continue;
		thing->spin += elapsed * (SPIN_RATE + SPIN_VARIETY * (i % 3));
		thing->t -= thing->speed * elapsed;
		double u = thing->u, v = thing->v;
		switch (thing->motion) {
		case MOTION_HOVER:
			// it breathes in place, slightly, so that it lives
			v += HOVER_DRIFT * sin(now * HOVER_RATE + thing->phase);
			break;
		case MOTION_ORBIT:
			// it goes round the tunnel wall, a moon of the rail
			u = ORBIT_RADIUS * cos(now * ORBIT_RATE + thing->phase);
			v = ORBIT_RADIUS * sin(now * ORBIT_RATE + thing->phase);
			break;
		}
		thing->at = thing_world(rail, thing->t, u, v);
		// past the eye it is gone, and the price of missing it is the score
		if (thing->t < t_eye + GONE_BEHIND)
			thing->used = 0;
	}
}

void things_draw(const struct camera *cam, double now)
{
	for (int i = 0; i < THINGS_MAX; i++) {
		const struct thing *thing = &things[i];
		if (!thing->used)
			continue;
		struct light lit = light_scale(thing->colour, THING_GLOW);
		// a hit washes it white for an instant, so the shot is seen to land
		if (now - thing->hurt_at < HIT_FLASH && thing->hurt_at > 0)
			lit = LIGHT_HIT;
		// locked, it burns a little brighter and pulses
		if (thing->locked)
			lit = light_scale(lit, LOCK_PULSE + LOCK_PULSE_SWING * sin(now * LOCK_PULSE_RATE));
		mesh_draw(cam, mesh_of(thing->shape), thing->at, thing->size, thing->spin,
			  thing->spin * SPIN_PITCH, 0.0, lit, 1);
	}
}

int thing_on_screen(const struct camera *cam, const struct thing *thing,
		    double *x, double *y)
{
	double depth;
	return draw_project(cam, thing->at, x, y, &depth);
}

int things_count(void)
{
	int count = 0;
	for (int i = 0; i < THINGS_MAX; i++)
		if (things[i].used)
			count++;
	return count;
}
