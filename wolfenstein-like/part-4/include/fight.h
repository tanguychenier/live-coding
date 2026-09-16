#ifndef FIGHT_H
#define FIGHT_H

#include "world.h"

// what the player can do about it. a steel bar torn off a bulkhead first:
// hands would need flesh, and flesh done badly ruins everything around it.
// a bar is drawn with the same words as the walls, and it says by itself
// what it is for.
#define PLAYER_HEALTH   100.0
#define PIPE_REACH      2.05
#define PIPE_DAMAGE     34.0
#define PIPE_ARC        0.55      // how wide the swing is, in cosine
#define PIPE_COOLDOWN   0.62
#define PIPE_SWING      0.34      // how long the swing takes to pass

// and what one finds later. a sidearm kills at a distance and costs rounds:
// it changes the range one plays at, not only the damage. it comes empty,
// and the rounds are somewhere else.
#define GUN_DAMAGE      60.0
#define GUN_COOLDOWN    0.42
#define GUN_RANGE       14.0
#define GUNSHOT_CARRIES 10.0      // how far a shot is heard, in cells
// the gun starts empty. up close you can still hit with the stock, and the
// rounds are somewhere in the hold.
#define AMMO_START      0
#define AMMO_PACK       10        // what one crate holds
#define MED_PACK        40.0

struct fight {
	double health;
	int ammo;
	int has_gun;
	int put_down;
	int chosen;           // 1 the bar, 2 the gun, 0: let the game pick
	double last_hit;      // when the player last swung
	double flash;         // how long the muzzle flash still lights the room
	double hurt;          // how long the screen still shows he was hit
	// where the bullet landed. with no point of impact one shoots in the
	// dark: nothing says whether the wall, the beast, or nothing at all
	// was hit.
	double spark;
	double spark_depth;
	// the magazine has just run dry. the game says so once, and the hand goes
	// back to the bar on its own.
	int emptied;
	// the blow has landed, and the sight shows it for a fraction of a second.
	double hitmark;
};

extern struct fight fight;

void fight_reset(void);
// returns 1 when something was hit, so the caller can say so
int  fight_strike(const struct player *player, double now);
void fight_take(double damage);
void fight_step(double elapsed);
// what one holds is always on screen. that is what makes a first person
// view: without it the player does not even know he can swing. "moving"
// says whether the weapon sways with the walk, "now" times the swing.
void fight_draw(const struct player *player, double now, int moving);
// health, rounds, both weapons and the key that swings
void fight_hud(double now);
// is the sidearm out
int  fight_gun_out(void);
// patch up
void fight_heal(double amount);
// the level is over: the whole view goes dark, by this much, and the
// words are written on top. the edges alone are for taking a hit.
void fight_dim(double amount);

#endif
