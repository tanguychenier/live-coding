#include <math.h>
#include <string.h>

#include "boss.h"
#include "mesh.h"
#include "particle.h"
#include "sound.h"
#include "world.h"

void boss_reset(struct boss *boss)
{
	memset(boss, 0, sizeof *boss);
	boss->flash_at = -BOSS_FLASH;
}

// the health of the bar counts every hit that the fight asks for
static const int PHASE_HITS[BOSS_PHASES] = {
	BOSS_NODES * BOSS_NODE_HEALTH, BOSS_POINTS * BOSS_POINT_HEALTH, BOSS_INNER_HEALTH,
	BOSS_FURY_HEALTH };

static struct vec node_place(const struct boss *boss, int i, double now)
{
	// two rings of four, tilted against each other, turning
	int ring = i / 4;
	double angle = 2 * M_PI * (i % 4) / 4 + now * BOSS_NODE_RATE * (ring ? -1.0 : 1.0);
	double tilt = ring ? BOSS_NODE_TILT : -BOSS_NODE_TILT;
	struct vec flat = vec(cos(angle), 0.0, sin(angle));
	struct vec tilted = vec(flat.x, flat.z * sin(tilt), flat.z * cos(tilt));
	return add(boss->centre, scale(tilted, BOSS_NODE_ORBIT));
}

// the corners of the shell, as the shell turns
static struct vec corner_place(const struct boss *boss, int corner)
{
	const struct mesh *mesh = mesh_of(SHAPE_ICOSA);
	return mesh_place(mesh->point[corner], boss->centre, BOSS_CORE_SIZE, boss->spin,
			  boss->spin * BOSS_PITCH_RATIO, 0.0);
}

static void start_phase(struct boss *boss, int phase, double now)
{
	boss->phase = phase;
	boss->phase_at = now;
	boss->flash_at = now;
	boss->changed = 1;
	boss->open = 0.0;
	sound_hit(HIT_BOSS, 0, sound_next_step(now));
	// a part that finds no room in the table counts as already gone, the
	// fight goes on without it rather than stopping
	switch (phase) {
	case 0:
		for (int i = 0; i < BOSS_NODES; i++) {
			struct thing *part = thing_place(SHAPE_OCTA, MOTION_HELD, node_place(boss, i, now),
						      BOSS_NODE_SIZE, BOSS_NODE_HEALTH, LIGHT_BOSS_NODE);
			boss->node[i] = part ? part->serial : 0;
			if (part)
				part->worth = BOSS_NODE_WORTH;
		}
		boss->next_fire = sound_next_step(now + BOSS_NODE_FIRE_BEATS * BEAT);
		break;
	case 1:
		for (int i = 0; i < BOSS_POINTS; i++) {
			struct thing *part = thing_place(SHAPE_OCTA, MOTION_HELD, corner_place(boss, i * 2),
						      BOSS_POINT_SIZE, BOSS_POINT_HEALTH, LIGHT_BOSS_OPEN);
			boss->point[i] = part ? part->serial : 0;
			if (part)
				part->worth = BOSS_POINT_WORTH;
		}
		boss->next_fire = sound_next_step(now + BOSS_FAN_BEATS * BEAT);
		break;
	case 2: {
		struct thing *part = thing_place(SHAPE_DIAMOND, MOTION_HELD, boss->centre,
					      BOSS_INNER_SIZE, BOSS_INNER_HEALTH, LIGHT_BOSS_OPEN);
		boss->inner = part ? part->serial : 0;
		if (part) {
			part->worth = BOSS_INNER_WORTH;
			part->shielded = 1;
		}
		boss->next_drone = sound_next_step(now + BOSS_DRONE_BEATS * BEAT);
		break;
	}
	default: {
		struct thing *part = thing_place(SHAPE_DIAMOND, MOTION_HELD, boss->centre,
					      BOSS_INNER_SIZE, BOSS_FURY_HEALTH, LIGHT_BOSS_OPEN);
		boss->inner = part ? part->serial : 0;
		if (part)
			part->worth = BOSS_FURY_WORTH;
		boss->next_fire = sound_next_step(now + BOSS_FURY_BEATS * BEAT);
		break;
	}
	}
}

void boss_begin(struct boss *boss, struct vec centre, struct vec eye, struct vec forward,
		double now)
{
	boss->active = 1;
	boss->began = now;
	boss->centre = centre;
	// the orbit starts where the rail ends, going the way the rail went
	boss->axis_x = unit(sub(eye, centre));
	boss->axis_z = unit(forward);
	start_phase(boss, 0, now);
}

void boss_eye(const struct boss *boss, double now, struct vec *eye, struct vec *at,
	      struct vec *motion, struct vec *bend)
{
	double angle = (now - boss->began) * BOSS_ORBIT_RATE;
	struct vec on_ring = add(scale(boss->axis_x, cos(angle)), scale(boss->axis_z, sin(angle)));
	*eye = add(boss->centre, add(scale(on_ring, ORBIT_RADIUS_EYE), vec(0, ORBIT_HEIGHT, 0)));
	// the eye turns from the way it came to the core over a couple of seconds
	struct vec tangent = add(scale(boss->axis_x, -sin(angle)), scale(boss->axis_z, cos(angle)));
	struct vec ahead = add(*eye, scale(tangent, ORBIT_RADIUS_EYE));
	double turned = (now - boss->began) / ORBIT_TURN_IN;
	turned = turned < 0.0 ? 0.0 : turned > 1.0 ? 1.0 : turned;
	turned = turned * turned * (3.0 - 2.0 * turned);
	*at = mix(ahead, boss->centre, turned);
	// on a circle the motion is along the tangent and the bend points at the
	// centre, the rate squared times the radius
	*motion = scale(tangent, ORBIT_RADIUS_EYE * BOSS_ORBIT_RATE);
	*bend = scale(on_ring, -ORBIT_RADIUS_EYE * BOSS_ORBIT_RATE * BOSS_ORBIT_RATE);
}

// the shields. the nodes turn with their rings, and one of them fires
static void phase_shields(struct boss *boss, const struct sight *sight, double now)
{
	int left = 0;
	for (int i = 0; i < BOSS_NODES; i++) {
		struct thing *part = thing_by_serial(boss->node[i]);
		if (!part)
			continue;
		part->at = node_place(boss, i, now);
		left++;
	}
	if (now >= boss->next_fire) {
		// the next living node, round the ring
		for (int tries = 0; tries < BOSS_NODES; tries++) {
			struct thing *part = thing_by_serial(boss->node[boss->fired % BOSS_NODES]);
			boss->fired++;
			if (part) {
				thing_launch(part->at, sight, palette_of(ZONE_CORE)->bolt, SHAPE_BOLT,
					     BOLT_SIZE, BOSS_BOLT_SPEED, 0.0);
				break;
			}
		}
		boss->next_fire = sound_next_step(now + BOSS_NODE_FIRE_BEATS * BEAT);
	}
	if (left == 0)
		start_phase(boss, 1, now);
}

// the weak points ride the shell, and the core throws fans of three
static void phase_points(struct boss *boss, const struct sight *sight, double now)
{
	int left = 0;
	for (int i = 0; i < BOSS_POINTS; i++) {
		struct thing *part = thing_by_serial(boss->point[i]);
		if (!part)
			continue;
		part->at = corner_place(boss, i * 2);
		left++;
	}
	if (now >= boss->next_fire) {
		sound_hit(HIT_WARN, 0, sound_next_step(now));
		for (int i = -1; i <= 1; i++)
			thing_launch(boss->centre, sight, palette_of(ZONE_CORE)->bolt, SHAPE_BOLT,
				     BOLT_SIZE, BOSS_BOLT_SPEED, i * BOSS_FAN_SPREAD);
		boss->next_fire = sound_next_step(now + BOSS_FAN_BEATS * BEAT);
	}
	if (left == 0)
		start_phase(boss, 2, now);
}

// the shell opens and closes on the bar, the inner core is only soft while
// it is open, and drones come out of it
static void phase_shell(struct boss *boss, const struct sight *sight, double elapsed, double now)
{
	struct thing *inner = thing_by_serial(boss->inner);
	if (!inner) {
		start_phase(boss, 3, now);
		return;
	}
	double bars = (now - boss->phase_at) / BAR;
	int open = ((int)floor(bars / BOSS_OPEN_BARS)) % 2 == 1;
	// it slides open and shut instead of snapping
	double want = open ? 1.0 : 0.0;
	boss->open += (want - boss->open) * fmin(1.0, BOSS_SHELL_SLIDE * elapsed);
	inner->shielded = boss->open < 0.5;
	inner->at = boss->centre;
	if (now >= boss->next_drone) {
		if (!inner->shielded) {
			sound_hit(HIT_WARN, 0, sound_next_step(now));
			thing_launch(boss->centre, sight, palette_of(ZONE_CORE)->enemy_alt, SHAPE_TETRA,
				     BOSS_DRONE_SIZE, BOSS_DRONE_SPEED, 0.0);
		}
		boss->next_drone = sound_next_step(now + BOSS_DRONE_BEATS * BEAT);
	}
}

// the naked core. open all the time, firing all the time
static void phase_fury(struct boss *boss, const struct sight *sight, double now)
{
	struct thing *inner = thing_by_serial(boss->inner);
	if (!inner) {
		boss->dead = 1;
		boss->dead_at = now;
		particles_burst(boss->centre, BOSS_DEATH_SPARKS, BOSS_DEATH_SPEED, LIGHT_BOSS_OPEN);
		sound_hit(HIT_BOSS, 0, sound_next_step(now));
		sound_hit(HIT_GATE, 0, sound_next_step(now));
		return;
	}
	boss->open = 1.0;
	inner->at = boss->centre;
	if (now >= boss->next_fire) {
		sound_hit(HIT_WARN, 0, sound_next_step(now));
		thing_launch(boss->centre, sight, palette_of(ZONE_CORE)->bolt, SHAPE_BOLT,
			     BOLT_SIZE, BOSS_BOLT_SPEED, BOSS_FURY_SPREAD);
		thing_launch(boss->centre, sight, palette_of(ZONE_CORE)->bolt, SHAPE_BOLT,
			     BOLT_SIZE, BOSS_BOLT_SPEED, -BOSS_FURY_SPREAD);
		boss->next_fire = sound_next_step(now + BOSS_FURY_BEATS * BEAT);
	}
}

void boss_update(struct boss *boss, const struct sight *sight, double elapsed, double now)
{
	if (!boss->active || boss->dead)
		return;
	boss->spin += elapsed * BOSS_SPIN * (1.0 + BOSS_SPIN_GAIN * boss->phase);
	switch (boss->phase) {
	case 0: phase_shields(boss, sight, now); break;
	case 1: phase_points(boss, sight, now); break;
	case 2: phase_shell(boss, sight, elapsed, now); break;
	default: phase_fury(boss, sight, now); break;
	}
}

double boss_health(const struct boss *boss)
{
	if (!boss->active)
		return -1.0;
	if (boss->dead)
		return 0.0;
	int total = 0, left = 0;
	for (int p = 0; p < BOSS_PHASES; p++) {
		total += PHASE_HITS[p];
		if (p > boss->phase)
			left += PHASE_HITS[p];
	}
	switch (boss->phase) {
	case 0:
		for (int i = 0; i < BOSS_NODES; i++) {
			struct thing *part = thing_by_serial(boss->node[i]);
			left += part ? part->health : 0;
		}
		break;
	case 1:
		for (int i = 0; i < BOSS_POINTS; i++) {
			struct thing *part = thing_by_serial(boss->point[i]);
			left += part ? part->health : 0;
		}
		break;
	default: {
		struct thing *part = thing_by_serial(boss->inner);
		left += part ? part->health : 0;
		break;
	}
	}
	return (double)left / total;
}

int boss_phase_changed(struct boss *boss)
{
	int changed = boss->changed;
	boss->changed = 0;
	return changed;
}

void boss_draw(const struct boss *boss, const struct camera *cam, double now,
	       const struct palette *pal)
{
	if (!boss->active)
		return;
	double flash = 1.0 - (now - boss->flash_at) / BOSS_FLASH;
	if (flash < 0.0)
		flash = 0.0;
	if (boss->dead) {
		// the shell flies apart, each edge on its own line, and is gone
		double gone = (now - boss->dead_at) / BOSS_DEATH;
		if (gone >= 1.0)
			return;
		const struct mesh *mesh = mesh_of(SHAPE_ICOSA);
		for (int i = 0; i < mesh->edges; i++) {
			struct vec from = corner_place(boss, mesh->edge[i][0]);
			struct vec to = corner_place(boss, mesh->edge[i][1]);
			struct vec away = scale(unit(sub(mix(from, to, 0.5), boss->centre)), gone * BOSS_SHARD_FLIGHT);
			draw_line(cam, add(from, away), add(to, away), light_scale(LIGHT_BOSS_OPEN, 1.0 - gone));
		}
		return;
	}
	// the shell, dull while it cannot be hurt, hot when it can, and split
	// in two along the spin axis when open
	struct light shell = boss->phase >= 1 ? LIGHT_BOSS_OPEN : LIGHT_BOSS;
	if (boss->phase >= 2)
		shell = light_mix(LIGHT_BOSS, LIGHT_BOSS_OPEN, boss->open);
	shell = light_scale(shell, 1.0 + flash * BOSS_FLASH_GAIN);
	// the fury throbs on the beat
	double throb = boss->phase == BOSS_PHASES - 1
		? 1.0 + BOSS_THROB * exp(-sound_beat_phase(now) * BEAT_DECAY) : 1.0;
	const struct mesh *mesh = mesh_of(SHAPE_ICOSA);
	struct vec placed[MESH_POINTS_MAX];
	for (int i = 0; i < mesh->points; i++) {
		struct vec local = mesh->point[i];
		struct vec at = mesh_place(local, boss->centre, BOSS_CORE_SIZE * throb, boss->spin,
					   boss->spin * BOSS_PITCH_RATIO, 0.0);
		double side = local.y >= 0.0 ? 1.0 : -1.0;
		placed[i] = add(at, vec(0.0, side * boss->open * BOSS_SPLIT, 0.0));
	}
	// what joins the two halves is left out once they part, so that the
	// shell splits into two caps instead of stretching
	int parted = boss->open > BOSS_PARTED;
	for (int i = 0; i < mesh->faces; i++) {
		double ya = mesh->point[mesh->face[i][0]].y, yb = mesh->point[mesh->face[i][1]].y;
		double yc = mesh->point[mesh->face[i][2]].y;
		if (parted && ((ya >= 0) != (yb >= 0) || (ya >= 0) != (yc >= 0)))
			continue;
		draw_triangle(cam, placed[mesh->face[i][0]], placed[mesh->face[i][1]],
			      placed[mesh->face[i][2]], shell);
	}
	for (int i = 0; i < mesh->edges; i++) {
		double ya = mesh->point[mesh->edge[i][0]].y, yb = mesh->point[mesh->edge[i][1]].y;
		if (parted && (ya >= 0) != (yb >= 0))
			continue;
		draw_line(cam, placed[mesh->edge[i][0]], placed[mesh->edge[i][1]], shell);
	}
	// the spokes to the shields, so that they are seen to belong to it
	if (boss->phase == 0)
		for (int i = 0; i < BOSS_NODES; i++) {
			struct thing *part = thing_by_serial(boss->node[i]);
			if (part)
				draw_line(cam, boss->centre, part->at, light_scale(pal->rails, BOSS_SPOKE_LIGHT));
		}
	// the heart of light in the middle
	draw_point(cam, boss->centre, light_scale(shell, BOSS_HEART_LIGHT),
		   BOSS_HEART_SIZE + BOSS_HEART_OPEN * boss->open);
}
