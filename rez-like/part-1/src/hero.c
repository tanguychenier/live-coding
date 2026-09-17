#include <math.h>
#include <string.h>

#include "hero.h"
#include "mesh.h"
#include "palette.h"
#include "particle.h"
#include "sound.h"

void hero_reset(struct hero *hero)
{
	memset(hero, 0, sizeof *hero);
	hero->thrust_at = -HERO_THRUST;
}

// the pose of this instant. the arms go forward on a release, and the
// chest rises and falls with the beat
static struct vec pose(const struct hero *hero, struct vec local, int index, double now)
{
	double thrust = 1.0 - (now - hero->thrust_at) / HERO_THRUST;
	if (thrust < 0.0) thrust = 0.0;
	if (thrust > 1.0) thrust = 1.0;
	thrust = sin(thrust * M_PI);
	if (index == HERO_HAND_R || index == HERO_HAND_L) {
		// the hands sweep forward, still apart
		struct vec forward = vec(local.x * HERO_THRUST_IN, local.y + HERO_THRUST_UP,
					 local.z + HERO_THRUST_OUT);
		local = mix(local, forward, thrust);
	}
	if (index == HERO_ELBOW_R || index == HERO_ELBOW_L)
		local = mix(local, vec(local.x * HERO_ELBOW_IN, local.y + HERO_ELBOW_UP,
				       local.z + HERO_ELBOW_OUT), thrust);
	local.y += HERO_BOB * exp(-sound_beat_phase(now) * BEAT_DECAY);
	return local;
}

// from the mesh to the world, through the bank, the pitch, and the frame of
// the camera, so that the figure always sits in the same place on screen
static struct vec place(const struct hero *hero, struct vec local)
{
	double cr = cos(hero->bank), sr = sin(hero->bank);
	double x = local.x * cr - local.y * sr, y = local.x * sr + local.y * cr, z = local.z;
	double cp = cos(HERO_PITCH), sp = sin(HERO_PITCH);
	double y2 = y * cp - z * sp, z2 = y * sp + z * cp;
	return add(hero->at, add(scale(hero->right, x * HERO_SIZE),
			      add(scale(hero->up, y2 * HERO_SIZE), scale(hero->forward, z2 * HERO_SIZE))));
}

void hero_update(struct hero *hero, const struct camera *cam, double cursor_x,
		 double cursor_y, int released, double elapsed, double now)
{
	if (released)
		hero->thrust_at = now;
	double want_x = (cursor_x / view_width - 0.5) * 2.0;
	double want_y = (0.5 - cursor_y / view_height) * 2.0;
	double ease = fmin(1.0, HERO_FOLLOW * elapsed);
	hero->lean_x += (want_x - hero->lean_x) * ease;
	hero->lean_y += (want_y - hero->lean_y) * ease;
	hero->bank += (-hero->lean_x * HERO_BANK - hero->bank) * ease;
	hero->right = cam->right;
	hero->up = cam->up;
	hero->forward = cam->forward;
	hero->at = add(cam->eye, add(scale(cam->forward, HERO_AHEAD),
		add(scale(cam->right, hero->lean_x * HERO_LEAN),
		    scale(cam->up, hero->lean_y * HERO_LEAN_UP - HERO_BELOW))));
	const struct mesh *mesh = mesh_of(SHAPE_HERO);
	struct vec right = place(hero, pose(hero, mesh->point[HERO_HAND_R], HERO_HAND_R, now));
	struct vec left = place(hero, pose(hero, mesh->point[HERO_HAND_L], HERO_HAND_L, now));
	hero->hands = mix(right, left, 0.5);
	// the sparks off the feet, so that it is seen to fly
	hero->trail_due += elapsed * HERO_TRAIL_RATE;
	while (hero->trail_due >= 1.0) {
		hero->trail_due -= 1.0;
		int foot = hero->trail_due > 0.5 ? HERO_FOOT_R : HERO_FOOT_L;
		struct vec at = place(hero, pose(hero, mesh->point[foot], foot, now));
		particle_add(at, scale(hero->forward, -HERO_TRAIL_SPEED), HERO_TRAIL_LIFE,
			     HERO_TRAIL_SIZE, LIGHT_HERO_TRAIL);
	}
}

void hero_draw(const struct hero *hero, const struct camera *cam, double now)
{
	const struct mesh *mesh = mesh_of(SHAPE_HERO);
	struct vec placed[MESH_POINTS_MAX];
	for (int i = 0; i < mesh->points; i++)
		placed[i] = place(hero, pose(hero, mesh->point[i], i, now));
	// the body goes white-hot when a chain leaves
	double thrust = 1.0 - (now - hero->thrust_at) / HERO_THRUST;
	struct light body = LIGHT_HERO;
	if (thrust > 0.0)
		body = light_scale(body, 1.0 + thrust);
	for (int i = 0; i < mesh->faces; i++)
		draw_triangle(cam, placed[mesh->face[i][0]], placed[mesh->face[i][1]],
			      placed[mesh->face[i][2]], body);
	for (int i = 0; i < mesh->edges; i++)
		draw_line(cam, placed[mesh->edge[i][0]], placed[mesh->edge[i][1]], body);
	// a point of light in each hand, where the shots come from, and a wake
	// behind each, the way a wing tip draws one
	double glow = HERO_HAND_GLOW + HERO_HAND_FLARE * (thrust > 0 ? thrust : 0);
	draw_point(cam, placed[HERO_HAND_R], body, glow);
	draw_point(cam, placed[HERO_HAND_L], body, glow);
	struct vec back = scale(hero->forward, -HERO_WAKE);
	struct light wake = light_scale(body, HERO_WAKE_LIGHT);
	draw_line(cam, placed[HERO_HAND_R], add(placed[HERO_HAND_R], back), wake);
	draw_line(cam, placed[HERO_HAND_L], add(placed[HERO_HAND_L], back), wake);
}
