#ifndef VEC_H
#define VEC_H

#include <math.h>

// three numbers, and the handful of operations a wireframe world needs.
// everything is passed by value, the compiler keeps them in registers
struct vec {
	double x, y, z;
};

static inline struct vec vec(double x, double y, double z)
{
	return (struct vec){ x, y, z };
}

static inline struct vec add(struct vec a, struct vec b)
{
	return vec(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline struct vec sub(struct vec a, struct vec b)
{
	return vec(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline struct vec scale(struct vec a, double k)
{
	return vec(a.x * k, a.y * k, a.z * k);
}

static inline double dot(struct vec a, struct vec b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline struct vec cross(struct vec a, struct vec b)
{
	return vec(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
		   a.x * b.y - a.y * b.x);
}

static inline double length(struct vec a)
{
	return sqrt(dot(a, a));
}

static inline struct vec unit(struct vec a)
{
	double n = length(a);
	return n > 0.0 ? scale(a, 1.0 / n) : a;
}

// a point between a and b, t going from zero at a to one at b
static inline struct vec mix(struct vec a, struct vec b, double t)
{
	return add(a, scale(sub(b, a), t));
}

#endif
