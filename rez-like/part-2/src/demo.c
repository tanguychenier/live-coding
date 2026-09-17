#include <math.h>
#include <string.h>
#include <time.h>

#include "demo.h"
#include "thing.h"

// when it last let go, when it started holding, and what the hand is up to
static double let_go_at, hold_since;
static int hand;
static unsigned int dice;
static unsigned int aimed;          // the serial of the target it is going for
static double react_until;          // the new target is not seen until then
static double arrived_at;           // when it reached the target, for the overshoot
static double overshoot;            // how long it keeps going past it
static int chain_goal;              // how many it means to mark this hold
static double hold_max;             // how long it will hold at most
static unsigned int missed;         // a target it does not see this hold
static int blind;                   // it is not looking at what dives, this hold
static int core_deaths;             // how many times it died at the core
static int started;                 // it has pressed start once
static int finished;                // its run is over, it leaves the keys for good
static double retry_wait;
static int letters;                 // of its name, typed so far

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
	blind = 0;
	letters = 0;
	if (dice == 0)
		dice = (unsigned int)time(NULL) | 1u;
}

void demo_hand(void)
{
	hand = 1;
}

int demo_is_hand(void)
{
	return hand;
}

static int is_locked(const struct player *player, unsigned int serial)
{
	for (int i = 0; i < player->locks; i++)
		if (player->locked[i] == serial)
			return 1;
	return 0;
}

// the hand is dying on purpose, its first time in the third phase of the
// core. it has eyes only for the open core, the drones come at it unseen,
// and it is slow. if the drones have not done it after a while, it plays
// for real
static int dying(const struct boss *boss, double now)
{
	return hand && boss && boss->active && boss->phase == HAND_DEATH_PHASE && core_deaths == 0
		&& now - boss->phase_at < HAND_DEATH_GIVE_UP;
}

// the nearest thing on screen that is not yet marked. a bolt or a diver
// comes first, whatever the distance, unless the hand is blind to it this
// hold. the one the hand is already going for counts as nearer, so that a
// group does not make the sight jump. dying, the hand sees only the core
static int pick(const struct player *player, const struct camera *cam, double *tx, double *ty,
		const struct boss *boss, double now, int *threat_picked)
{
	int best = -1;
	double best_score = 1e30;
	int greedy = dying(boss, now);
	for (int i = 0; i < THINGS_MAX; i++) {
		const struct thing *thing = &things[i];
		double x, y;
		if (!thing->used || thing->shielded || is_locked(player, thing->serial))
			continue;
		if (thing->serial == missed)
			continue;
		if (greedy && thing->motion != MOTION_HELD)
			continue;
		int threat = thing->motion == MOTION_BOLT || thing->motion == MOTION_DIVE;
		if (blind && threat)
			threat = 0;
		if (!thing_on_screen(cam, thing, &x, &y))
			continue;
		if (x < 0 || y < 0 || x >= view_width || y >= view_height)
			continue;
		if (!threat && length(sub(thing->at, cam->eye)) > DEMO_ENGAGE)
			continue;
		double dist = hypot(x - player->cursor_x, y - player->cursor_y);
		if (thing->serial == aimed)
			dist *= HAND_STICK;
		if (threat)
			dist -= view_width;
		if (dist < best_score) {
			best_score = dist;
			best = i;
			*tx = x;
			*ty = y;
			*threat_picked = threat;
		}
	}
	return best;
}

// a hold begins. the hand decides how many it wants, how long it will
// wait, which one it will not see, and whether it looks at what dives
static void begin_hold(double now, int zone)
{
	hold_since = now;
	if (!hand) {
		chain_goal = LOCKS_MAX;
		hold_max = DEMO_HOLD_MAX;
		return;
	}
	chain_goal = roll() < HAND_FULL_ODDS ? LOCKS_MAX
		: HAND_CHAIN_MIN + (int)(roll() * (HAND_CHAIN_MAX - HAND_CHAIN_MIN + 1));
	hold_max = between(HAND_HOLD_MIN, HAND_HOLD_MAX);
	missed = 0;
	if (roll() < HAND_MISS_ODDS) {
		// one of the things there is, drawn at random
		int count = things_count();
		int pick_index = (int)(roll() * count);
		for (int i = 0; i < THINGS_MAX; i++)
			if (things[i].used && pick_index-- == 0 && things[i].motion != MOTION_HELD)
				missed = things[i].serial;
	}
	blind = zone == HAND_BLIND_ZONE && roll() < HAND_BLIND_ODDS;
}

void demo_drive(const struct player *player, const struct camera *cam, struct keys *keys,
		double now, int zone, const struct boss *boss)
{
	keys->left = keys->right = keys->up = keys->down = 0;
	keys->fire = 0;
	double tx = DEMO_REST_X, ty = DEMO_REST_Y;
	if (hand) {
		tx += HAND_WANDER * view_width * sin(now * HAND_WANDER_RATE_X);
		ty += HAND_WANDER * view_height * cos(now * HAND_WANDER_RATE_Y);
	}
	int threat = 0;
	int target = pick(player, cam, &tx, &ty, boss, now, &threat);
	unsigned int serial = target >= 0 ? things[target].serial : 0;
	// the hand notices a target late, and not the same each time. one more
	// in a group it is already looking at is seen at once, and something
	// flying at it is seen fast, whatever else is going on
	if (hand && serial != aimed) {
		int fresh = aimed == 0;
		aimed = serial;
		if (fresh || threat) {
			double slow = boss && boss->active && !threat ? HAND_REACT_CORE : 0.0;
			if (dying(boss, now))
				slow += HAND_DEATH_SLOW;
			react_until = now + between(HAND_REACT_MIN, HAND_REACT_MAX) + slow;
		}
		arrived_at = -1.0;
		overshoot = between(HAND_OVERSHOOT_MIN, HAND_OVERSHOOT_MAX);
	}
	int noticed = !hand || now >= react_until;
	double dx = tx - player->cursor_x, dy = ty - player->cursor_y;
	double tolerance = DEMO_TOLERANCE * draw_scale();
	int there = fabs(dx) <= tolerance && fabs(dy) <= tolerance;
	// the hand keeps pushing a moment past the target, then settles on it
	if (hand && target >= 0 && there && arrived_at < 0.0)
		arrived_at = now;
	int pushing = hand && arrived_at >= 0.0 && now < arrived_at + overshoot;
	if (noticed && (!there || pushing)) {
		keys->right = dx > tolerance || (pushing && dx > 0);
		keys->left = dx < -tolerance || (pushing && dx < 0);
		keys->down = dy > tolerance || (pushing && dy > 0);
		keys->up = dy < -tolerance || (pushing && dy < 0);
	}
	int holding = player->holding;
	if (holding) {
		// let go when the hands are full, when there is nothing left to
		// mark, when it has held long enough, or when something marked is
		// about to hit
		int panic = 0;
		for (int i = 0; i < player->locks; i++) {
			const struct thing *marked = thing_by_serial(player->locked[i]);
			if (marked && (marked->motion == MOTION_BOLT || marked->motion == MOTION_DIVE)
			    && length(sub(marked->at, cam->eye)) < DEMO_PANIC)
				panic = 1;
		}
		// dying, it hangs on to its mark on the core until the hold runs
		// out, so that the core is not shot down before the drones come
		int empty = target < 0 && !dying(boss, now);
		if (player->locks >= chain_goal || empty || now - hold_since > hold_max || panic) {
			let_go_at = now;
			return;
		}
		keys->fire = 1;
		return;
	}
	if (target >= 0 && noticed && now - let_go_at > DEMO_REACTION) {
		keys->fire = 1;
		begin_hold(now, zone);
	}
}

// it presses start once. after its run it stays on the title with the
// table, whatever brought the title back, and never touches a key again
void demo_title(struct keys *keys, double since)
{
	if (!started && !finished && since > HAND_START_WAIT) {
		started = 1;
		keys->enter = 1;
	}
}

// the signal is lost. the hand lets go of everything at once, and the
// death counts the moment the screen is up
void demo_dead(struct keys *keys, double since, int at_core)
{
	keys->fire = keys->left = keys->right = keys->up = keys->down = 0;
	if (retry_wait <= 0.0) {
		retry_wait = between(HAND_RETRY_MIN, HAND_RETRY_MAX);
		if (at_core)
			core_deaths++;
	}
	if (since > retry_wait) {
		retry_wait = 0.0;
		keys->enter = 1;
	}
}

void demo_won(struct keys *keys, double since)
{
	if (since > HAND_WON_WAIT) {
		finished = 1;
		keys->enter = 1;
	}
}

void demo_name(struct keys *keys, double since)
{
	int wanted = (int)(since / HAND_LETTER_GAP);
	size_t length = strlen(HAND_NAME);
	if (letters < (int)length && wanted > letters) {
		keys->typed = HAND_NAME[letters];
		letters++;
	} else if (letters >= (int)length && wanted > letters + 1) {
		keys->enter = 1;
	}
}
