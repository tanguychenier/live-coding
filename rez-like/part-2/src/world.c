#include <math.h>

#include "mesh.h"
#include "thing.h"
#include "tunnel.h"
#include "world.h"

double world_hash(int first, int second, int third)
{
	// three integers stirred into one, the way a hash table does it. the
	// multipliers are large primes, the shifts spread the high bits down
	unsigned int mixed = (unsigned int)first * HASH_PRIME_A + (unsigned int)second * HASH_PRIME_B
		+ (unsigned int)third * HASH_PRIME_C;
	mixed = (mixed ^ (mixed >> HASH_SHIFT_A)) * HASH_PRIME_D;
	mixed ^= mixed >> HASH_SHIFT_B;
	return (double)(mixed & HASH_MASK) / (HASH_MASK + 1.0);
}

static double floor_y;

void world_set_floor(double y)
{
	floor_y = y;
}

// ---------------------------------------------------------------- the plain
static void box(const struct camera *cam, struct vec base, double half_width, double height,
		struct light colour)
{
	struct vec corner[8];
	for (int i = 0; i < 8; i++)
		corner[i] = add(base, vec((i & 1) ? half_width : -half_width,
					  (i & 2) ? height : 0.0,
					  (i & 4) ? half_width : -half_width));
	for (int i = 0; i < 8; i++)
		for (int bit = 1; bit < 8; bit <<= 1)
			if (!(i & bit))
				draw_line(cam, corner[i], corner[i | bit], colour);
	// the top is a pane, so that the monolith reads as a solid
	draw_triangle(cam, corner[2], corner[3], corner[7], colour);
	draw_triangle(cam, corner[2], corner[7], corner[6], colour);
}

static void plain_draw(const struct camera *cam, const struct rail *rail, double t_eye,
		       double now, const struct palette *pal)
{
	struct light colour = tunnel_colour(rail, t_eye, pal);
	struct light grid = light_scale(colour, GRID_LIGHT);
	double ex = cam->eye.x, ez = cam->eye.z;
	// the grid, lines along the rail and across it, on a fixed lattice so
	// that they stand still while the eye flies over them
	double x0 = floor((ex - GRID_REACH) / GRID_STEP) * GRID_STEP;
	for (double x = x0; x <= ex + GRID_REACH; x += GRID_STEP)
		draw_line(cam, vec(x, floor_y, ez - GRID_BEHIND),
			  vec(x, floor_y, ez + GRID_REACH), grid);
	double z0 = floor((ez - GRID_BEHIND) / GRID_STEP) * GRID_STEP;
	for (double z = z0; z <= ez + GRID_REACH; z += GRID_STEP)
		draw_line(cam, vec(ex - GRID_REACH, floor_y, z),
			  vec(ex + GRID_REACH, floor_y, z), grid);
	// the monoliths, one per cell when the hash says so, and never on the
	// rail's path
	int cx0 = (int)floor((ex - MONO_REACH) / MONO_CELL);
	int cx1 = (int)floor((ex + MONO_REACH) / MONO_CELL);
	int cz0 = (int)floor((ez - GRID_BEHIND) / MONO_CELL);
	int cz1 = (int)floor((ez + MONO_REACH) / MONO_CELL);
	for (int cz = cz0; cz <= cz1; cz++)
		for (int cx = cx0; cx <= cx1; cx++) {
			if (world_hash(cx, cz, 1) > MONO_ODDS)
				continue;
			double x = (cx + world_hash(cx, cz, 2)) * MONO_CELL;
			double z = (cz + world_hash(cx, cz, 3)) * MONO_CELL;
			double rail_x = rail_at(rail, z / RAIL_SPACING).x;
			if (fabs(x - rail_x) < MONO_CLEAR)
				continue;
			double height = MONO_HEIGHT_MIN + (MONO_HEIGHT_MAX - MONO_HEIGHT_MIN) * world_hash(cx, cz, 4);
			double half_width = MONO_WIDTH_MIN + (MONO_WIDTH_MAX - MONO_WIDTH_MIN) * world_hash(cx, cz, 5);
			// each one pulses on its own beat, a little, like a light on a tower
			double pulse = 1.0 - MONO_PULSE + MONO_PULSE * sin(now * MONO_PULSE_RATE
								       + world_hash(cx, cz, 6) * 2.0 * M_PI);
			box(cam, vec(x, floor_y, z), half_width, height, light_scale(colour, pulse));
		}
	// the lines in the sky, for the speed
	struct light sky = light_scale(colour, SKY_LINE_LIGHT);
	for (int i = -SKY_LINES; i <= SKY_LINES; i++)
		draw_line(cam, vec(ex + i * SKY_LINE_X, floor_y + SKY_LINE_Y, ez - GRID_BEHIND),
			  vec(ex + i * SKY_LINE_X, floor_y + SKY_LINE_Y, ez + GRID_REACH), sky);
}

// ---------------------------------------------------------------- the swarm
void world_void(const struct camera *cam, const struct rail *rail, double t_eye,
		double now, const struct palette *pal, double gain)
{
	if (gain <= 0.0)
		return;
	struct light colour = light_scale(tunnel_colour(rail, t_eye, pal), gain);
	// the shell, a solid so big the eye is inside it, turning slowly
	mesh_draw(cam, mesh_of(SHAPE_ICOSA), cam->eye, SHELL_RADIUS, now * SHELL_TURN,
		  now * SHELL_TURN * 0.7, 0.0, light_scale(colour, SHELL_LIGHT), 0);
	// the dust, one mote per cell, fixed in the world
	int cx0 = (int)floor((cam->eye.x - DUST_REACH) / DUST_CELL);
	int cx1 = (int)floor((cam->eye.x + DUST_REACH) / DUST_CELL);
	int cy0 = (int)floor((cam->eye.y - DUST_REACH) / DUST_CELL);
	int cy1 = (int)floor((cam->eye.y + DUST_REACH) / DUST_CELL);
	int cz0 = (int)floor((cam->eye.z - DUST_REACH * DUST_BEHIND) / DUST_CELL);
	int cz1 = (int)floor((cam->eye.z + DUST_REACH) / DUST_CELL);
	for (int cz = cz0; cz <= cz1; cz++)
		for (int cy = cy0; cy <= cy1; cy++)
			for (int cx = cx0; cx <= cx1; cx++) {
				struct vec at = vec((cx + world_hash(cx, cy, cz)) * DUST_CELL,
						    (cy + world_hash(cy, cz, cx)) * DUST_CELL,
						    (cz + world_hash(cz, cx, cy)) * DUST_CELL);
				if (length(sub(at, cam->eye)) < DUST_NEAR)
					continue;
				draw_point(cam, at, light_scale(colour, DUST_LIGHT), DUST_SIZE);
			}
	// the streaks along the rail, each in its own place around it
	double first = floor((t_eye - STREAK_BEHIND) / STREAK_SPACING) * STREAK_SPACING;
	for (double t = first; t < t_eye + STREAK_AHEAD && t <= rail_end(rail); t += STREAK_SPACING) {
		int cell = (int)floor(t / STREAK_SPACING + 0.5);
		double angle = world_hash(cell, 7, 0) * 2.0 * M_PI;
		double radius = STREAK_RADIUS_MIN
			+ (STREAK_RADIUS_MAX - STREAK_RADIUS_MIN) * world_hash(cell, 8, 0);
		struct vec centre = rail_at(rail, t), forward, right, up;
		rail_frame(rail, t, &forward, &right, &up);
		struct vec at = add(centre, add(scale(right, radius * cos(angle)),
						scale(up, radius * sin(angle))));
		draw_line(cam, at, add(at, scale(forward, STREAK_LENGTH)),
			  light_scale(colour, STREAK_LIGHT * (0.5 + world_hash(cell, 9, 0))));
	}
}

// ---------------------------------------------------------------- the arena
struct vec world_arena_centre(const struct rail *rail)
{
	// the rail ends on the orbit, going straight ahead, so the centre is
	// one orbit radius to the side of its last point
	double end = rail_end(rail);
	struct vec forward, right, up;
	rail_frame(rail, end, &forward, &right, &up);
	return add(rail_at(rail, end), scale(right, ORBIT_RADIUS_EYE));
}

// a point on a sphere, from its two angles, around the pole and down from
// it
static struct vec on_sphere(struct vec centre, double radius, double around, double down)
{
	return add(centre, vec(radius * sin(down) * cos(around), radius * cos(down),
			       radius * sin(down) * sin(around)));
}

void world_arena(const struct camera *cam, struct vec centre, double now,
		 const struct palette *pal, double open)
{
	struct light colour = light_scale(light_hue(pal->hue), ARENA_LIGHT);
	double turn = now * ARENA_TURN;
	// the sphere opens as the fight goes, its radius growing with the
	// boss's anger, so the last phase is fought in a bigger room
	double radius = ARENA_RADIUS * (1.0 + ARENA_GROWTH * open);
	for (int lat = 1; lat < ARENA_LATITUDES; lat++) {
		double down = M_PI * lat / ARENA_LATITUDES;
		double lit = lat == ARENA_LATITUDES / 2 ? 1.0 : ARENA_WEAK;
		for (int seg = 0; seg < ARENA_SEGMENTS; seg++) {
			double a0 = 2 * M_PI * seg / ARENA_SEGMENTS + turn;
			double a1 = 2 * M_PI * (seg + 1) / ARENA_SEGMENTS + turn;
			draw_line(cam, on_sphere(centre, radius, a0, down),
				  on_sphere(centre, radius, a1, down), light_scale(colour, lit));
		}
	}
	for (int lon = 0; lon < ARENA_LONGITUDES; lon++) {
		double around = 2 * M_PI * lon / ARENA_LONGITUDES + turn;
		for (int seg = 0; seg < ARENA_SEGMENTS; seg++) {
			double d0 = M_PI * seg / ARENA_SEGMENTS, d1 = M_PI * (seg + 1) / ARENA_SEGMENTS;
			draw_line(cam, on_sphere(centre, radius, around, d0),
				  on_sphere(centre, radius, around, d1),
				  light_scale(colour, ARENA_MERIDIAN));
		}
	}
	// the inner ring, tilted, turning the other way
	for (int seg = 0; seg < ARENA_SEGMENTS; seg++) {
		double a0 = 2 * M_PI * seg / ARENA_SEGMENTS - turn * 2.0;
		double a1 = 2 * M_PI * (seg + 1) / ARENA_SEGMENTS - turn * 2.0;
		struct vec q0 = vec(ARENA_RING_RADIUS * cos(a0),
				    ARENA_RING_RADIUS * sin(a0) * sin(ARENA_RING_TILT),
				    ARENA_RING_RADIUS * sin(a0) * cos(ARENA_RING_TILT));
		struct vec q1 = vec(ARENA_RING_RADIUS * cos(a1),
				    ARENA_RING_RADIUS * sin(a1) * sin(ARENA_RING_TILT),
				    ARENA_RING_RADIUS * sin(a1) * cos(ARENA_RING_TILT));
		draw_line(cam, add(centre, q0), add(centre, q1),
			  light_scale(pal->enemy_alt, ARENA_RING_LIGHT));
	}
}

void world_draw(const struct camera *cam, const struct rail *rail, double t_eye,
		double now, int zone, const struct palette *pal)
{
	switch (zone) {
	case ZONE_UPLINK:
		// the tunnel, and the void where it opens
		tunnel_draw(cam, rail, t_eye, now, pal);
		world_void(cam, rail, t_eye, now, pal, rail_open(rail, t_eye));
		break;
	case ZONE_FIELD:
		plain_draw(cam, rail, t_eye, now, pal);
		break;
	case ZONE_SWARM:
		world_void(cam, rail, t_eye, now, pal, 1.0);
		break;
	default:
		world_arena(cam, world_arena_centre(rail), now, pal, 0.0);
		break;
	}
}
