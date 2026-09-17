#include <math.h>
#include <string.h>

#include "particle.h"

struct spark {
	int used;
	struct vec at, velocity;
	double life, age, size;
	struct light colour;
};

static struct spark sparks[PARTICLES_MAX];
static unsigned int dice = DICE_SEED;

// a small die of our own, so that a burst never looks the same twice. the
// three shifts are the ones of xorshift32, a generator that fits in a line
static double roll(void)
{
	dice ^= dice << DICE_SHIFT_A;
	dice ^= dice >> DICE_SHIFT_B;
	dice ^= dice << DICE_SHIFT_C;
	return (double)(dice & DICE_MASK) / (DICE_MASK + 1.0);
}

void particles_clear(void)
{
	memset(sparks, 0, sizeof sparks);
}

void particle_add(struct vec at, struct vec velocity, double life, double size,
		  struct light colour)
{
	for (int i = 0; i < PARTICLES_MAX; i++) {
		struct spark *spark = &sparks[i];
		if (spark->used)
			continue;
		spark->used = 1;
		spark->at = at;
		spark->velocity = velocity;
		spark->life = life;
		spark->age = 0.0;
		spark->size = size;
		spark->colour = colour;
		return;
	}
}

void particles_burst(struct vec at, int count, double speed, struct light colour)
{
	for (int i = 0; i < count; i++) {
		// a direction on the sphere, evenly, not bunched at the poles
		double height = roll() * 2.0 - 1.0;
		double angle = roll() * 2.0 * M_PI;
		double ring = sqrt(1.0 - height * height);
		struct vec dir = vec(ring * cos(angle), ring * sin(angle), height);
		double part = speed * (SPARK_SPEED_MIN + SPARK_SPEED_VAR * roll());
		particle_add(at, scale(dir, part), SPARK_LIFE * (SPARK_LIFE_MIN + SPARK_LIFE_VAR * roll()),
			     SPARK_SIZE * (SPARK_SIZE_MIN + SPARK_SIZE_VAR * roll()), colour);
	}
}

void particles_update(double elapsed)
{
	for (int i = 0; i < PARTICLES_MAX; i++) {
		struct spark *spark = &sparks[i];
		if (!spark->used)
			continue;
		spark->age += elapsed;
		if (spark->age >= spark->life) {
			spark->used = 0;
			continue;
		}
		spark->at = add(spark->at, scale(spark->velocity, elapsed));
		spark->velocity = scale(spark->velocity, 1.0 - SPARK_DRAG * elapsed);
	}
}

void particles_draw(const struct camera *cam)
{
	for (int i = 0; i < PARTICLES_MAX; i++) {
		const struct spark *spark = &sparks[i];
		if (!spark->used)
			continue;
		double part = 1.0 - spark->age / spark->life;
		struct light lit = light(spark->colour.r * part, spark->colour.g * part, spark->colour.b * part);
		draw_point(cam, spark->at, lit, spark->size * (0.5 + part));
	}
}
