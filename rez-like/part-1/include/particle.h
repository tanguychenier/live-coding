#ifndef PARTICLE_H
#define PARTICLE_H

#include "draw.h"
#include "vec.h"

// sparks. a thing that dies bursts into them, a shot leaves a trail of
// them, and they fade as they fly
#define PARTICLES_MAX   1600
// the die, its seed and the shifts of xorshift32. a roll keeps this many
// of its low bits
#define DICE_SEED       12345u
#define DICE_SHIFT_A    13
#define DICE_SHIFT_B    17
#define DICE_SHIFT_C    5
#define DICE_BITS       16
#define DICE_MASK       ((1u << DICE_BITS) - 1)
#define SPARK_DRAG      1.8
#define SPARK_LIFE      0.9
#define SPARK_SIZE      0.05     // in world units, before it shrinks with age
// a burst is not even, each spark takes its own share of the speed, the
// life and the size, from this much to this much more
#define SPARK_SPEED_MIN 0.4
#define SPARK_SPEED_VAR 0.8
#define SPARK_LIFE_MIN  0.6
#define SPARK_LIFE_VAR  0.6
#define SPARK_SIZE_MIN  0.6
#define SPARK_SIZE_VAR  0.8

void particles_clear(void);
// a burst of sparks at a point, flying out in every direction
void particles_burst(struct vec at, int count, double speed, struct light colour);
// one spark with its own velocity, for trails
void particle_add(struct vec at, struct vec velocity, double life, double size,
		  struct light colour);
void particles_update(double elapsed);
void particles_draw(const struct camera *cam);

#endif
