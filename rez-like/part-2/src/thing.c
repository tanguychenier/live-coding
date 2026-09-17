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

void things_scatter(void)
{
	for (int i = 0; i < THINGS_MAX; i++)
		if (things[i].used) {
			particles_burst(things[i].at, HIT_SPARKS, HIT_SPEED, things[i].colour);
			things[i].used = 0;
		}
}

static struct thing *free_slot(void)
{
	for (int i = 0; i < THINGS_MAX; i++)
		if (!things[i].used) {
			memset(&things[i], 0, sizeof things[i]);
			things[i].used = 1;
			things[i].worth = 1;
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
	// a turret fires a while after it appears, so that it is seen first
	thing->fire_at = -1.0;
	return thing;
}

struct thing *thing_place(enum shape shape, enum motion motion, struct vec at, double size,
			  int health, struct light colour)
{
	struct thing *thing = free_slot();
	if (!thing)
		return NULL;
	thing->shape = shape;
	thing->motion = motion;
	thing->at = at;
	thing->size = size;
	thing->health = health;
	thing->colour = colour;
	return thing;
}

// where the eye will be after so long. a way that bends is a circle, and
// the circle is followed round, which is exact on the orbit of the core
static struct vec eye_later(const struct sight *sight, double after)
{
	double speed = length(sight->motion);
	double pull = length(sight->bend);
	if (speed <= 0.0 || pull <= 0.0)
		return add(sight->eye, scale(sight->motion, after));
	double radius = speed * speed / pull;
	double angle = after * pull / speed;
	struct vec inward = unit(sight->bend);
	struct vec along = unit(sight->motion);
	struct vec centre = add(sight->eye, scale(inward, radius));
	return add(centre, add(scale(inward, -radius * cos(angle)), scale(along, radius * sin(angle))));
}

// where a bolt flying from here at this speed meets the eye
static struct vec aim_at_eye(struct vec from, const struct sight *sight, double speed)
{
	struct vec aim = sight->eye;
	for (int step = 0; step < AIM_STEPS; step++)
		aim = eye_later(sight, length(sub(aim, from)) / speed);
	return aim;
}

void thing_launch(struct vec from, const struct sight *sight, struct light colour,
		  enum shape shape, double size, double speed, double spread)
{
	struct vec dir = unit(sub(aim_at_eye(from, sight, speed), from));
	if (spread != 0.0) {
		// turned around the world's up, which is enough for a fan
		double cs = cos(spread), sn = sin(spread);
		dir = vec(dir.x * cs - dir.z * sn, dir.y, dir.x * sn + dir.z * cs);
	}
	struct thing *bolt = thing_place(shape, MOTION_BOLT, from, size, 1, colour);
	if (!bolt)
		return;
	bolt->velocity = scale(dir, speed);
}

void thing_fire(struct vec from, const struct sight *sight, struct light colour)
{
	thing_launch(from, sight, colour, SHAPE_BOLT, BOLT_SIZE, BOLT_SPEED, 0.0);
}

// a bolt flies on, bending toward where it will meet the eye while the eye
// is still ahead of it. returns 1 when it reaches the eye
static int fly_bolt(struct thing *thing, const struct sight *sight, double elapsed)
{
	struct vec delta = sub(thing->at, sight->eye);
	double ahead = dot(delta, sight->forward);
	if (ahead > 0.0) {
		double speed = length(thing->velocity);
		struct vec aim = aim_at_eye(thing->at, sight, speed);
		struct vec toward = scale(unit(sub(aim, thing->at)), speed);
		thing->velocity = add(thing->velocity, scale(sub(toward, thing->velocity),
							     fmin(1.0, BOLT_HOMING * elapsed)));
		thing->velocity = scale(unit(thing->velocity), speed);
	}
	thing->at = add(thing->at, scale(thing->velocity, elapsed));
	delta = sub(thing->at, sight->eye);
	ahead = dot(delta, sight->forward);
	if (length(delta) < BOLT_HIT) {
		thing->used = 0;
		particles_burst(thing->at, HIT_SPARKS, HIT_SPEED, thing->colour);
		return 1;
	}
	if (ahead < -BOLT_PAST)
		thing->used = 0;
	return 0;
}

// a thing comes down the rail at its speed. a turret that has come close
// enough keeps pace with the eye for a while instead, so that its bolts
// have a distance to cross
static void keep_pace(struct thing *thing, double t_eye, double elapsed, double now)
{
	int pacing = thing->motion == MOTION_SHOOTER && thing->t - t_eye <= TURRET_KEEP
		&& (thing->leave_at == 0.0 || now < thing->leave_at);
	if (!pacing) {
		thing->t -= thing->speed * elapsed;
		return;
	}
	if (thing->leave_at == 0.0)
		thing->leave_at = now + TURRET_STAY * BEAT;
	thing->t = t_eye + TURRET_KEEP;
}

// a turret takes aim, warns, and fires on the beat. the warning is what
// gives the player the time to shoot it first
static void turret(struct thing *thing, const struct sight *sight, double now)
{
	if (thing->fire_at < 0.0) {
		thing->fire_at = sound_next_step(now + TURRET_FIRST * BEAT);
		return;
	}
	if (!thing->aiming && now >= thing->fire_at - TURRET_AIM) {
		thing->aiming = 1;
		sound_hit(HIT_WARN, 0, sound_next_step(now));
	}
	if (now >= thing->fire_at) {
		thing_fire(thing->at, sight, thing->colour);
		thing->aiming = 0;
		thing->fire_at = sound_next_step(now + TURRET_BEATS * BEAT);
	}
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
	if (thing->shielded)
		return 0;
	thing->health--;
	thing->hurt_at = now;
	if (thing->health > 0) {
		particles_burst(thing->at, HIT_SPARKS, HIT_SPEED, LIGHT_HIT);
		return 0;
	}
	particles_burst(thing->at, BURST_SPARKS, BURST_SPEED, thing->colour);
	thing->used = 0;
	return thing->worth;
}

int things_update(const struct rail *rail, double t_eye, const struct sight *sight,
		  double elapsed, double now)
{
	int reached = 0;
	for (int i = 0; i < THINGS_MAX; i++) {
		struct thing *thing = &things[i];
		if (!thing->used)
			continue;
		thing->spin += elapsed * (SPIN_RATE + SPIN_VARIETY * (i % 3));
		if (thing->motion == MOTION_HELD)
			continue;
		if (thing->motion == MOTION_BOLT) {
			reached += fly_bolt(thing, sight, elapsed);
			continue;
		}
		keep_pace(thing, t_eye, elapsed, now);
		double u = thing->u, v = thing->v;
		switch (thing->motion) {
		case MOTION_HOVER:
		case MOTION_SHOOTER:
			// it breathes in place, slightly, so that it lives
			v += HOVER_DRIFT * sin(now * HOVER_RATE + thing->phase);
			break;
		case MOTION_ORBIT:
			// it goes round the tunnel wall, a moon of the rail
			u = ORBIT_RADIUS * cos(now * ORBIT_RATE + thing->phase);
			v = ORBIT_RADIUS * sin(now * ORBIT_RATE + thing->phase);
			break;
		case MOTION_CROSS:
			// it sweeps from one wall to the other, and back
			u = CROSS_SWING * sin(now * CROSS_RATE + thing->phase);
			v = thing->v + CROSS_BOB * sin(now * CROSS_BOB_RATE + thing->phase);
			break;
		case MOTION_DIVE: {
			// it comes at the eye and closes on the middle as it does
			double closing = (thing->t - t_eye) / DIVE_CLOSE;
			if (closing < 0) closing = 0;
			if (closing > 1) closing = 1;
			u = thing->u * closing;
			v = thing->v * closing - (1.0 - closing) * EYE_DROP / TUNNEL_RADIUS;
			break;
		}
		default:
			break;
		}
		thing->at = thing_world(rail, thing->t, u, v);
		if (thing->motion == MOTION_SHOOTER)
			turret(thing, sight, now);
		if (thing->t < t_eye + GONE_BEHIND) {
			thing->used = 0;
			// only what dives at the eye can hurt it. the rest flies past,
			// and the price of missing it is the score, not the health
			if (thing->motion == MOTION_DIVE) {
				particles_burst(thing->at, HIT_SPARKS, HIT_SPEED, thing->colour);
				reached++;
			}
		}
	}
	return reached;
}

void things_draw(const struct camera *cam, double now)
{
	for (int i = 0; i < THINGS_MAX; i++) {
		const struct thing *thing = &things[i];
		if (!thing->used)
			continue;
		struct light lit = light_scale(thing->colour, thing->shielded ? SHIELD_DIM : THING_GLOW);
		// a hit washes it white for an instant, so the shot is seen to land
		if (now - thing->hurt_at < HIT_FLASH && thing->hurt_at > 0)
			lit = LIGHT_HIT;
		// a turret taking aim blinks, fast, the way a warning does
		if (thing->aiming && sin(now * AIM_BLINK) > 0.0)
			lit = LIGHT_HIT;
		// locked, it burns a little brighter and pulses
		if (thing->locked)
			lit = light_scale(lit, LOCK_PULSE + LOCK_PULSE_SWING * sin(now * LOCK_PULSE_RATE));
		double yaw = thing->spin, pitch = thing->spin * SPIN_PITCH, roll = 0.0;
		int faces = 1;
		if (thing->motion == MOTION_GATE) {
			yaw = 0; pitch = 0; roll = thing->spin * GATE_ROLL;
			faces = 0;
		}
		if (thing->shape == SHAPE_MANTA) {
			yaw = M_PI; pitch = 0; roll = MANTA_ROLL * sin(now * MANTA_ROCK + thing->phase);
		}
		if (thing->shape == SHAPE_SPINDLE) {
			yaw = thing->spin * SPINDLE_YAW; pitch = 0; roll = 0;
		}
		if (thing->motion == MOTION_BOLT) {
			// a bolt points where it flies
			struct vec dir = unit(thing->velocity);
			yaw = atan2(dir.x, dir.z);
			pitch = -asin(dir.y);
			roll = thing->spin * BOLT_ROLL;
		}
		mesh_draw(cam, mesh_of(thing->shape), thing->at, thing->size, yaw, pitch, roll, lit, faces);
		if (thing->motion == MOTION_BOLT)
			particle_add(thing->at, scale(thing->velocity, -BOLT_TRAIL_DRIFT), BOLT_TRAIL_LIFE,
				     BOLT_TRAIL_SIZE, thing->colour);
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
		if (things[i].used && things[i].motion != MOTION_BOLT)
			count++;
	return count;
}
