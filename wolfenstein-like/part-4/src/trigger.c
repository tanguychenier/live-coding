#include <math.h>
#include <string.h>

#include "trigger.h"
#include "sound.h"
#include "story.h"
#include "thing.h"
#include "world.h"

// a cell, a radius, and what happens when you walk in. each one plays only
// once, because a scene that plays twice is not a scene any more.
enum act { ACT_NONE, ACT_WAKE, ACT_SHAKE, ACT_BLAST };

struct trigger {
	double x, y, radius;
	const char *text;          // what the game writes, or NULL
	int sfx;                  // a sound, or -1
	double reach;              // a quelle distance on l'entend
	enum act act;
	double rx, ry;            // where the act is aimed
	int done;
};

// the order of this table is the order of the game. each line is a place
// where something happens, and nothing happens anywhere else. the player
// walks, the station answers, and he thinks he caused all of it, which is
// true.
static struct trigger table[] = {
	// the corridor. we leave the airlock and the station keeps getting
	// wrecked around us, without ever taking the keys away from us.
	{ 12.5, 6.0, 1.8, "DECK THREE  GANGWAY",        -1,         0,  ACT_NONE, 0, 0, 0 },
	{ 16.5, 6.0, 1.6, NULL,                 SFX_IMPACT,  1.5, ACT_SHAKE, 0, 0, 0 },
	{ 19.5, 6.0, 1.6, "HULL BREACH  TWO DECKS UP",   -1,         0,  ACT_NONE, 0, 0, 0 },
	{ 22.5, 6.0, 1.6, NULL,                 SFX_SHRIEK, 11.0, ACT_NONE, 0, 0, 0 },
	// at the end of the corridor we hear a fight behind the door. that is
	// what makes you go and look, and that door is the one that will refuse.
	{ 26.0, 6.0, 1.8, "SOMEONE IS STILL FIGHTING",  SFX_SHOT,  9.0, ACT_NONE, 0, 0, 0 },

	// the workshop, where the first thing stands in full light, facing us,
	// alone, so that you learn to fight on a clean case
	{ 14.0, 11.0, 1.6, "WORKSHOP",                   -1,         0,  ACT_NONE, 0, 0, 0 },
	{ 15.5, 15.0, 2.6, "CTRL TO SWING",              -1,         0, ACT_WAKE, 17.5, 17.5, 0 },
	{ 21.0, 16.0, 2.6, NULL,                         -1,         0, ACT_WAKE, 23.5, 15.5, 0 },

	// the machine room, where they come one after another. the gun lies in
	// the middle of the room, so you see it before you need it and you run to
	// it afterwards.
	{ 33.0, 16.0, 2.0, "MACHINE DECK",               -1,         0, ACT_WAKE, 38.5, 12.5, 0 },
	{ 40.0, 15.0, 2.6, NULL,                  SFX_GROWL,  6.0, ACT_WAKE, 45.5, 13.5, 0 },
	{ 43.0, 19.0, 2.6, NULL,                         -1,         0, ACT_WAKE, 43.5, 20.5, 0 },

	// the hold, in amber light, where what slept in there gets up in three
	// beats
	{ 41.0, 26.0, 2.0, "CARGO HOLD  EMERGENCY POWER", -1,        0,  ACT_NONE, 0, 0, 0 },
	{ 40.0, 29.5, 3.0, NULL,                  SFX_SHRIEK, 5.0, ACT_WAKE, 36.5, 30.5, 0 },
	{ 44.0, 32.0, 3.0, NULL,                         -1,         0, ACT_WAKE, 43.5, 32.5, 0 },
	{ 46.0, 31.0, 3.0, NULL,                         -1,         0, ACT_WAKE, 46.5, 29.5, 0 },
	{ 38.0, 35.0, 3.0, NULL,                         -1,         0, ACT_WAKE, 38.5, 36.5, 0 },

	// the bottom. first the thick bulkhead, then the roar, then the thing
	// itself. the shake here is not an effect, it is the thing getting up.
	{ 26.5, 33.0, 1.8, "IT IS BEHIND THIS ONE",      -1,          0, ACT_NONE, 0, 0, 0 },
	{ 22.0, 33.0, 2.2, NULL,                  SFX_GROWL,  2.0, ACT_SHAKE, 0, 0, 0 },
	{ 20.0, 33.0, 2.5, "IT ATE THE CREW",            -1,          0, ACT_WAKE, 12.5, 33.5, 0 },
	{ 17.0, 33.0, 3.0, NULL,                         -1,          0, ACT_WAKE, 9.5, 29.5, 0 },
	{ 15.0, 35.0, 3.0, NULL,                         -1,          0, ACT_WAKE, 9.5, 37.5, 0 },
};
#define TRIGGER_COUNT ((int)(sizeof table / sizeof *table))

void triggers_reset(void)
{
	for (int i = 0; i < TRIGGER_COUNT; i++)
		table[i].done = 0;
}

// wakes the one thing nearest to the point asked for, and only that one,
// because a room that gets up all at once looks like a switch
static void wake_nearest(double x, double y, double now)
{
	struct thing *pick = NULL;
	double nearest = 4.0;
	for (int i = 0; i < THINGS_MAX; i++) {
		struct thing *t = &things[i];
		if (!t->used || t->state == THING_DEAD || t->roam)
			continue;
		double d = hypot(t->x - x, t->y - y);
		if (d < nearest) {
			nearest = d;
			pick = t;
		}
	}
	if (pick && pick->state == THING_IDLE) {
		pick->state = THING_ALERT;
		pick->since = now;
		pick->last_seen_x = x;
		pick->last_seen_y = y;
	}
}

const char *triggers_step(const struct player *player, double now)
{
	for (int i = 0; i < TRIGGER_COUNT; i++) {
		struct trigger *d = &table[i];
		if (d->done)
			continue;
		if (hypot(player->x - d->x, player->y - d->y) > d->radius)
			continue;
		d->done = 1;
		if (d->sfx >= 0)
			sound_play((enum sfx)d->sfx, d->reach);
		switch (d->act) {
		case ACT_WAKE:
			wake_nearest(d->rx, d->ry, now);
			break;
		case ACT_SHAKE:
			story_hit(now, 1.0, player->x, player->y);
			break;
		case ACT_BLAST:
			story_hit(now, 0.5, player->x, player->y);
			break;
		default:
			break;
		}
		return d->text;
	}
	return NULL;
}
