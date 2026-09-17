#include <math.h>
#include <string.h>

#include "font.h"
#include "level.h"
#include "palette.h"
#include "particle.h"
#include "player.h"
#include "sound.h"
#include "thing.h"

void player_reset(struct player *player)
{
	memset(player, 0, sizeof *player);
	player->cursor_x = view_width / 2.0;
	player->cursor_y = view_height / 2.0;
	player->health = HEALTH_MAX;
	player->alive = 1;
}

static int already_locked(const struct player *player, unsigned int serial)
{
	for (int i = 0; i < player->locks; i++)
		if (player->locked[i] == serial)
			return 1;
	return 0;
}

// the cursor follows the mouse when it moves, and the arrows otherwise. the
// two never fight, the last one that moved wins
static void move_cursor(struct player *player, const struct keys *keys, double elapsed)
{
	static int last_mouse_x = -1, last_mouse_y = -1;
	if (keys->mouse_x != last_mouse_x || keys->mouse_y != last_mouse_y) {
		if (last_mouse_x >= 0) {
			player->cursor_x = keys->mouse_x;
			player->cursor_y = keys->mouse_y;
		}
		last_mouse_x = keys->mouse_x;
		last_mouse_y = keys->mouse_y;
	}
	double speed = CURSOR_SPEED * draw_scale();
	player->cursor_x += (keys->right - keys->left) * speed * elapsed;
	player->cursor_y += (keys->down - keys->up) * speed * elapsed;
	if (player->cursor_x < 0) player->cursor_x = 0;
	if (player->cursor_y < 0) player->cursor_y = 0;
	if (player->cursor_x > view_width - 1) player->cursor_x = view_width - 1;
	if (player->cursor_y > view_height - 1) player->cursor_y = view_height - 1;
}

// fire held, every target under the cursor gets a lock, once, up to eight.
// each lock plays a note, one degree higher than the one before
static void mark(struct player *player, const struct camera *cam, double now)
{
	for (int i = 0; i < THINGS_MAX && player->locks < LOCKS_MAX; i++) {
		const struct thing *thing = &things[i];
		double x, y;
		if (!thing->used || thing->shielded || already_locked(player, thing->serial))
			continue;
		if (!thing_on_screen(cam, thing, &x, &y))
			continue;
		double dist = hypot(x - player->cursor_x, y - player->cursor_y);
		double depth = fmax(1.0, length(sub(thing->at, cam->eye)));
		double reach = (LOCK_RADIUS + thing->size * LOCK_SIZE_GAIN / depth) * draw_scale();
		if (dist < reach) {
			sound_hit(HIT_LOCK, player->locks, sound_next_step(now));
			player->locked[player->locks++] = thing->serial;
		}
	}
}

// fire let go, one shot per lock, each on its own sixteenth, so that they
// are seen and heard leaving one after the other
static void release(struct player *player, double now)
{
	double first = sound_next_step(now);
	for (int i = 0; i < player->locks; i++) {
		for (int slot = 0; slot < SHOTS_MAX; slot++) {
			struct shot *shot = &player->shots[slot];
			if (shot->used)
				continue;
			shot->used = 1;
			shot->flying = 0;
			shot->target = player->locked[i];
			shot->launch = first + i * STEP;
			shot->land = shot->launch + SHOT_FLIGHT_STEPS * STEP;
			shot->note = i;
			shot->side = (i % 3) - 1;
			sound_hit(HIT_SHOT, i, shot->launch);
			break;
		}
	}
	if (player->locks > 0) {
		player->chain = player->locks;
		player->chain_at = now;
		player->released_at = now;
		player->released = 1;
		player->released_full = player->locks == LOCKS_MAX;
		// a full chain gives back a point of health, mastery is rewarded
		if (player->released_full && player->health < HEALTH_MAX)
			player->health++;
		if (player->chain > player->best_chain)
			player->best_chain = player->chain;
	}
	player->locks = 0;
}

// a shot in flight, from the hand to where its target is now. it curves out
// and back, so that eight of them fan instead of stacking on one line
static struct vec shot_at(const struct shot *shot, const struct thing *thing, double now)
{
	double part = (now - shot->launch) / (shot->land - shot->launch);
	if (part < 0) part = 0;
	if (part > 1) part = 1;
	struct vec straight = mix(shot->from, thing->at, part);
	double bend = sin(part * M_PI) * SHOT_CURVE;
	return add(straight, vec(bend * shot->side, bend * SHOT_CURVE_UP * (shot->note % 2 ? 1 : -1), 0));
}

static void land(struct player *player, struct shot *shot, struct thing *thing, double now)
{
	int was_gate = thing->motion == MOTION_GATE;
	int worth = thing_hurt(thing, now);
	if (worth > 0) {
		player->kills++;
		// the score rewards the chain, eight at once are worth far more
		// than eight one by one
		player->score += (long)SCORE_KILL * player->chain * player->chain * worth;
		sound_hit(HIT_KILL, shot->note, shot->land);
		if (was_gate) {
			// a gate gives back a point of health, it is the breath of
			// the level
			player->score += SCORE_GATE;
			sound_hit(HIT_GATE, 0, shot->land);
			if (player->health < HEALTH_MAX)
				player->health++;
		}
	} else {
		sound_hit(HIT_LOCK, shot->note, shot->land);
	}
}

// a shot leaves from where the hand is when its sixteenth comes, not from
// where it was when fire was let go, the hand has moved since
static void fly_shots(struct player *player, struct vec from, double now)
{
	for (int slot = 0; slot < SHOTS_MAX; slot++) {
		struct shot *shot = &player->shots[slot];
		if (!shot->used)
			continue;
		struct thing *thing = thing_by_serial(shot->target);
		if (!thing) {
			shot->used = 0;
			continue;
		}
		if (now < shot->launch)
			continue;
		if (!shot->flying) {
			shot->flying = 1;
			shot->from = from;
		}
		if (now >= shot->land) {
			shot->used = 0;
			land(player, shot, thing, now);
		}
	}
}

int player_hurt(struct player *player, int count, double now)
{
	if (count <= 0 || now - player->hurt_at < HURT_GRACE)
		return 0;
	player->hurt_at = now;
	player->health -= THING_DAMAGE;
	sound_hit(HIT_HURT, 0, sound_next_step(now));
	if (player->health <= 0) {
		player->health = 0;
		player->alive = 0;
	}
	return 1;
}

void player_update(struct player *player, const struct keys *keys, double elapsed)
{
	move_cursor(player, keys, elapsed);
	player->holding = player->alive && (keys->fire || keys->mouse_down);
}

// fire held, the sight marks, fire let go, the shots leave, and the shots in
// flight fly on. from is where they leave, the hand of the pilot
void player_aim(struct player *player, const struct camera *cam, struct vec from, double now)
{
	player->released = 0;
	player->released_full = 0;
	// the locks only hold on things that still exist
	int kept = 0;
	for (int i = 0; i < player->locks; i++)
		if (thing_by_serial(player->locked[i]))
			player->locked[kept++] = player->locked[i];
	player->locks = kept;
	for (int i = 0; i < THINGS_MAX; i++)
		things[i].locked = 0;
	if (player->holding)
		mark(player, cam, now);
	else if (player->was_holding)
		release(player, now);
	player->was_holding = player->holding;
	for (int i = 0; i < player->locks; i++) {
		struct thing *thing = thing_by_serial(player->locked[i]);
		if (thing)
			thing->locked = 1;
	}
	fly_shots(player, from, now);
}

// the cursor is a ring with four gaps, a sight and not a plate, and it grows
// while it marks. every locked target gets a small bracket, so the eye
// knows what the release will hit
static void draw_cursor(const struct player *player, const struct camera *cam, double now)
{
	double x = player->cursor_x, y = player->cursor_y;
	double px = draw_scale();
	double radius = LOCK_RADIUS * px
		* (player->holding ? 1.0 + CURSOR_PULSE * sin(now * CURSOR_PULSE_RATE) : CURSOR_REST);
	struct light lit = player->holding ? LIGHT_CURSOR_HOT : LIGHT_CURSOR;
	const int segments = 48, gap_every = 12, gap_size = 2;
	for (int i = 0; i < segments; i++) {
		double a0 = 2 * M_PI * i / segments, a1 = 2 * M_PI * (i + 1) / segments;
		if (i % gap_every >= gap_every - gap_size)
			continue;
		draw_line_2d(x + radius * cos(a0), y + radius * sin(a0),
			     x + radius * cos(a1), y + radius * sin(a1), lit);
	}
	// a dot in the middle, so the eye has a point to aim
	draw_line_2d(x - px, y, x + px, y, lit);
	for (int i = 0; i < player->locks; i++) {
		double tx, ty;
		const struct thing *thing = thing_by_serial(player->locked[i]);
		if (!thing || !thing_on_screen(cam, thing, &tx, &ty))
			continue;
		double size = (BRACKET_SIZE + BRACKET_SWING * sin(now * BRACKET_RATE + i)) * px;
		double arm = size * BRACKET_ARM;
		struct light mark_light = LIGHT_LOCK;
		draw_line_2d(tx - size, ty - size, tx - arm, ty - size, mark_light);
		draw_line_2d(tx - size, ty - size, tx - size, ty - arm, mark_light);
		draw_line_2d(tx + size, ty - size, tx + arm, ty - size, mark_light);
		draw_line_2d(tx + size, ty - size, tx + size, ty - arm, mark_light);
		draw_line_2d(tx - size, ty + size, tx - arm, ty + size, mark_light);
		draw_line_2d(tx - size, ty + size, tx - size, ty + arm, mark_light);
		draw_line_2d(tx + size, ty + size, tx + arm, ty + size, mark_light);
		draw_line_2d(tx + size, ty + size, tx + size, ty + arm, mark_light);
		// the number of the lock, small, above the bracket
		char digit[2] = { (char)('1' + i), 0 };
		font_write(tx + size + LOCK_DIGIT_GAP * px, ty - size - LOCK_DIGIT_UP * px, digit,
			   LOCK_DIGIT_SIZE * px, mark_light);
	}
}

void player_draw(const struct player *player, const struct camera *cam, double now)
{
	// the shots in flight. each draws the whole of its path from the hand,
	// faint where it left and bright at its head, so that eight of them
	// are seen as eight threads of light going to eight targets
	for (int slot = 0; slot < SHOTS_MAX; slot++) {
		const struct shot *shot = &player->shots[slot];
		const struct thing *thing = shot->used ? thing_by_serial(shot->target) : NULL;
		if (!thing || !shot->flying)
			continue;
		double since = now - shot->launch;
		struct vec tail = shot_at(shot, thing, shot->launch);
		for (int k = 1; k <= SHOT_TRAIL_STEPS; k++) {
			double part = (double)k / SHOT_TRAIL_STEPS;
			struct vec head = shot_at(shot, thing, shot->launch + since * part);
			double bright = SHOT_TRAIL_DIM + (1.0 - SHOT_TRAIL_DIM) * part * part;
			draw_line(cam, tail, head, light_scale(LIGHT_SHOT, bright));
			tail = head;
		}
		draw_point(cam, tail, LIGHT_SHOT, SHOT_HEAD_SIZE);
		particle_add(tail, vec(0, 0, 0), SHOT_SPARK_LIFE, SHOT_SPARK_SIZE, LIGHT_SHOT);
	}
	draw_cursor(player, cam, now);
}
