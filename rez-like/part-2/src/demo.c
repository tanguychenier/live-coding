#include <math.h>
#include <time.h>

#include "demo.h"
#include "thing.h"

// when it last let go, when it started holding, and what the hand is up to
static double let_go_at, hold_since;
static unsigned int dice;
static unsigned int aimed;          // the serial of the target it is going for
static double react_until;          // the new target is not seen until then
static double arrived_at;           // when it reached the target, for the overshoot
static double overshoot;            // how long it keeps going past it
static int chain_goal;              // how many it means to mark this hold
static double hold_max;             // how long it will hold at most
static unsigned int missed;         // a target it does not see this hold

static double roll(void)
{
	dice ^= dice << HAND_DICE_SHIFT_A;
	dice ^= dice >> HAND_DICE_SHIFT_B;
	dice ^= dice << HAND_DICE_SHIFT_C;
	return (double)(dice & HAND_DICE_MASK) / (HAND_DICE_MASK + 1.0);
}

static double between(double low, double high)
{
	return low + (high - low) * roll();
}

void demo_reset(void)
{
	let_go_at = -DEMO_REACTION;
	hold_since = 0.0;
	aimed = 0;
	react_until = 0.0;
	arrived_at = -1.0;
	chain_goal = LOCKS_MAX;
	hold_max = DEMO_HOLD_MAX;
	missed = 0;
	if (dice == 0)
		dice = (unsigned int)time(NULL) | 1u;
}

static int is_locked(const struct player *player, unsigned int serial)
{
	for (int i = 0; i < player->locks; i++)
		if (player->locked[i] == serial)
			return 1;
	return 0;
}

// the nearest thing on screen that is not yet marked. the one the hand is
// already going for counts as nearer, so that a group does not make the
// sight jump from one to the other
static int pick(const struct player *player, const struct camera *cam, double *tx, double *ty)
{
	int best = -1;
	double best_score = 1e30;
	for (int i = 0; i < THINGS_MAX; i++) {
		const struct thing *thing = &things[i];
		double x, y;
		if (!thing->used || is_locked(player, thing->serial))
			continue;
		if (thing->serial == missed)
			continue;
		if (!thing_on_screen(cam, thing, &x, &y))
			continue;
		if (x < 0 || y < 0 || x >= view_width || y >= view_height)
			continue;
		if (length(sub(thing->at, cam->eye)) > DEMO_ENGAGE)
			continue;
		double dist = hypot(x - player->cursor_x, y - player->cursor_y);
		if (thing->serial == aimed)
			dist *= HAND_STICK;
		if (dist < best_score) {
			best_score = dist;
			best = i;
			*tx = x;
			*ty = y;
		}
	}
	return best;
}

// a hold begins. the hand decides how many it wants, how long it will
// wait, and which one it will not see
static void begin_hold(double now)
{
	hold_since = now;
	chain_goal = roll() < HAND_FULL_ODDS ? LOCKS_MAX
		: HAND_CHAIN_MIN + (int)(roll() * (HAND_CHAIN_MAX - HAND_CHAIN_MIN + 1));
	hold_max = between(HAND_HOLD_MIN, HAND_HOLD_MAX);
	missed = 0;
	if (roll() < HAND_MISS_ODDS) {
		// one of the things there is, drawn at random
		int count = things_count();
		int pick_index = (int)(roll() * count);
		for (int i = 0; i < THINGS_MAX; i++)
			if (things[i].used && pick_index-- == 0)
				missed = things[i].serial;
	}
}

void demo_drive(const struct player *player, const struct camera *cam, struct keys *keys,
		double now)
{
	keys->left = keys->right = keys->up = keys->down = 0;
	keys->fire = 0;
	double tx = DEMO_REST_X, ty = DEMO_REST_Y;
	int target = pick(player, cam, &tx, &ty);
	unsigned int serial = target >= 0 ? things[target].serial : 0;
	// the hand notices a target late, and not the same each time. one more
	// target in a group it is already looking at is seen at once
	if (serial != aimed) {
		if (aimed == 0)
			react_until = now + between(HAND_REACT_MIN, HAND_REACT_MAX);
		aimed = serial;
		arrived_at = -1.0;
		overshoot = between(HAND_OVERSHOOT_MIN, HAND_OVERSHOOT_MAX);
	}
	int noticed = now >= react_until;
	double dx = tx - player->cursor_x, dy = ty - player->cursor_y;
	double tolerance = DEMO_TOLERANCE * draw_scale();
	int there = fabs(dx) <= tolerance && fabs(dy) <= tolerance;
	// the hand keeps pushing a moment past the target, then settles on it
	if (target >= 0 && there && arrived_at < 0.0)
		arrived_at = now;
	int pushing = arrived_at >= 0.0 && now < arrived_at + overshoot;
	if (noticed && (!there || pushing)) {
		keys->right = dx > tolerance || (pushing && dx > 0);
		keys->left = dx < -tolerance || (pushing && dx < 0);
		keys->down = dy > tolerance || (pushing && dy > 0);
		keys->up = dy < -tolerance || (pushing && dy < 0);
	}
	if (player->holding) {
		// let go when the hands are full, when there is nothing left to
		// mark, or when it has held long enough
		if (player->locks >= chain_goal || target < 0 || now - hold_since > hold_max) {
			let_go_at = now;
			return;
		}
		keys->fire = 1;
		return;
	}
	if (target >= 0 && noticed && now - let_go_at > DEMO_REACTION) {
		keys->fire = 1;
		begin_hold(now);
	}
}
