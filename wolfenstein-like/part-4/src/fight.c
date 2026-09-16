#include <math.h>
#include <stdio.h>
#include <string.h>

#include "fight.h"
#include "light.h"
#include "render.h"
#include "screen.h"
#include "sound.h"
#include "sprite.h"
#include "story.h"
#include "text.h"
#include "thing.h"

struct fight fight;

void fight_reset(void)
{
	memset(&fight, 0, sizeof fight);
	fight.health = PLAYER_HEALTH;
	fight.ammo = AMMO_START;
	fight.has_gun = 0;      // we start with a bar, not with a gun
}

// what is in hand, decided in one place. the player's own pick comes
// first, and the gun only comes out when it can fire: an empty weapon in
// hand is a trap, not a choice.
int fight_gun_out(void)
{
	return fight.has_gun && fight.ammo > 0 && fight.chosen != 1;
}

void fight_heal(double amount)
{
	fight.health += amount;
	if (fight.health > PLAYER_HEALTH)
		fight.health = PLAYER_HEALTH;
}

void fight_take(double damage)
{
	if (damage <= 0.0)
		return;
	fight.health -= damage;
	fight.hurt = 0.45;
	if (fight.health < 0.0)
		fight.health = 0.0;
}

void fight_step(double elapsed)
{
	if (fight.flash > 0.0)
		fight.flash -= elapsed;
	if (fight.spark > 0.0)
		fight.spark -= elapsed;
	if (fight.hitmark > 0.0)
		fight.hitmark -= elapsed;
	if (fight.hurt > 0.0)
		fight.hurt -= elapsed;
}

// the swing is a cone, and that is the whole point of a bar: it forgives
// the aim and charges you the distance. you have to walk up to the thing
// you do not know yet.
int fight_strike(const struct player *player, double now)
{
	double wait = fight_gun_out() ? GUN_COOLDOWN : PIPE_COOLDOWN;
	if (now - fight.last_hit < wait)
		return 0;
	fight.last_hit = now;
	int gun = fight_gun_out();
	if (gun) {
		fight.ammo--;
		fight.flash = 0.08;
		sound_play(SFX_SHOT, 0.0);
		fight.emptied = fight.ammo == 0;
		// a gunshot carries. that is what makes rounds cost more
		// than their count: you win one fight and wake two.
		things_hear(player->x, player->y, GUNSHOT_CARRIES, now);
	} else {
		sound_play(SFX_PUNCH, 0.0);
	}

	// the round stops at the first wall, because a bullet does not go through
	// partitions, and the impact shows where it stopped
	double wall = wall_depth ? wall_depth[view_width / 2] : VERY_FAR;
	struct thing *best = NULL;
	double nearest = gun ? GUN_RANGE : PIPE_REACH;
	if (gun && nearest > wall)
		nearest = wall;
	for (int i = 0; i < THINGS_MAX; i++) {
		struct thing *t = &things[i];
		if (!t->used || t->state == THING_DEAD)
			continue;
		double dx = t->x - player->x, dy = t->y - player->y;
		double d = sqrt(dx * dx + dy * dy);
		if (d > nearest || d < 0.001)
			continue;
		if (gun) {
			// a bullet misses or lands, never "about right". a
			// five degree cone forgives fifty pixels point
			// blank and three metres across a room: measure
			// the round to the body, not the angle.
			double ahead = dx * player->dir_x + dy * player->dir_y;
			if (ahead <= 0.0)
				continue;
			double across = fabs(dx * -player->dir_y
					    + dy * player->dir_x);
			double radius = t->boss ? 0.62 : t->tough ? 0.46 : 0.36;
			if (across > radius)
				continue;
		} else if ((dx * player->dir_x + dy * player->dir_y) / d < PIPE_ARC) {
			// the bar sweeps wide, and that is its whole point
			continue;
		}
		nearest = d;
		best = t;
	}
	// the spark means one thing with a gun and another with a bar. a
	// round always lands somewhere, on the beast or on the wall, and
	// the impact says where. a bar cutting air lights nothing at all:
	// a burst mid screen on a miss reads as gunfire.
	if (gun) {
		fight.spark = 0.11;
		fight.spark_depth = best ? nearest
			: (wall < GUN_RANGE ? wall : GUN_RANGE);
	} else if (best) {
		fight.spark = 0.07;
		fight.spark_depth = nearest;
	}
	if (!best)
		return 0;
	best->health -= (gun ? GUN_DAMAGE : PIPE_DAMAGE) * thing_weak(best);
	fight.hitmark = 0.16;
	// it takes the blow: a quarter second where it stops coming, and
	// that is what gives melee its rhythm
	best->stagger = now + (gun ? 0.16 : 0.26);
	best->hurt_at = now;
	sound_play(SFX_IMPACT, nearest);
	if (best->health <= 0.0) {
		best->state = THING_DEAD;
		fight.put_down++;
		sound_play(SFX_DIE, nearest);
		// the boss falls with all its weight, and the whole deck feels it
		if (best->boss)
			story_hit(now, 1.8, best->x, best->y);
	} else {
		// being hit wakes it, even if it had not seen you
		if (best->state == THING_IDLE)
			best->state = THING_ALERT;
		best->since = now;
	}
	return 1;
}

// ------------------------------------------------------------------ drawing
// both weapons are sheets, sculpted and photographed with the exact field
// of view of the game, glove included. a held weapon is a volume seen in
// perspective, and that does not work out when drawn flat.
static void draw_sheet(struct sprite *p, int frame, double lit,
			  int moving, int flash)
{
	if (!p->pixels)
		return;
	static double sway;
	sway += moving ? 0.10 : -sway * 0.10;
	double scale = view_height / 400.0;
	int bx = (int)(sin(sway) * 6 * scale);
	int by = (int)(fabs(cos(sway)) * 5 * scale);

	// walk the box only. the sheet covers the whole frame so the
	// perspective lands right, but it fills a quarter of it: without
	// this bound we read two hundred and fifty thousand pixels to lay
	// down sixty thousand, and the game loses ten frames a second.
	int ya = p->y0 * view_height / p->height + by;
	int yb = (p->y1 * view_height + p->height - 1) / p->height + by + 1;
	int xa = p->x0 * view_width / p->width + bx;
	int xb = (p->x1 * view_width + p->width - 1) / p->width + bx + 1;
	if (ya < 0) ya = 0;
	if (xa < 0) xa = 0;
	if (yb > view_height) yb = view_height;
	if (xb > view_width) xb = view_width;
	for (int y = ya; y < yb; y++) {
		int sy = (y - by) * p->height / view_height;
		if (sy < 0 || sy >= p->height)
			continue;
		for (int x = xa; x < xb; x++) {
			int sx = (x - bx) * p->width / view_width;
			unsigned int c = sprite_at(p, frame, sx, sy);
			if (!sprite_solid(c))
				continue;
			// it takes the light of the room you stand in.
			// rendered lit and pasted as is, it glowed like a
			// sticker in a black corridor.
			int r = (int)(((c >> 16) & 0xff) * lit);
			int g = (int)(((c >> 8) & 0xff) * lit);
			int b = (int)((c & 0xff) * lit);
			if (flash) {
				r += (255 - r) * 3 / 5;
				g += (255 - g) * 5 / 9;
				b += (255 - b) * 2 / 5;
			}
			view[y * view_width + x] =
				(unsigned int)((r << 16) | (g << 8) | b);
		}
	}
}

static struct sprite pipe_sheet, gun_sheet;

static void load_sheets(void)
{
	static int tried;
	if (!tried++) {
		sprite_load(&pipe_sheet, "art/pipe.tecs");
		sprite_load(&gun_sheet, "art/gun.tecs");
	}
}

static void draw_pipe(double lit, double now, int moving)
{
	// four frames: at rest, the blow, and two on the way back
	double since_hit = now - fight.last_hit;
	int frame = since_hit < PIPE_SWING * 0.40 ? 1
		: since_hit < PIPE_SWING * 0.70 ? 2
		: since_hit < PIPE_SWING ? 3 : 0;
	draw_sheet(&pipe_sheet, frame, lit, moving, 0);
}

static void draw_gun(double lit, double now, int moving)
{
	double since_hit = now - fight.last_hit;
	int frame = since_hit < 0.07 ? 1 : since_hit < 0.15 ? 2
		: since_hit < 0.26 ? 3 : 0;
	draw_sheet(&gun_sheet, frame, lit, moving, fight.flash > 0.0);
}

void fight_draw(const struct player *player, double now, int moving)
{
	int w = view_width, h = view_height;

	// the muzzle flash lights the room. one frame of white over
	// everything, falling off with the distance the wall is at: without
	// it the shot is a sound and a hole, with it the corridor exists
	if (fight.flash > 0.0 && wall_depth)
		for (int x = 0; x < w; x++) {
			double k = 0.55 / (1.0 + wall_depth[x] * 0.8);
			for (int y = 0; y < h; y++) {
				unsigned int c = view[y * w + x];
				int r = (int)(((c >> 16) & 0xff) + 255 * k);
				int g = (int)(((c >> 8) & 0xff) + 245 * k);
				int b = (int)((c & 0xff) + 220 * k);
				view[y * w + x] = (unsigned int)
					(((r > 255 ? 255 : r) << 16)
					| ((g > 255 ? 255 : g) << 8)
					| (b > 255 ? 255 : b));
			}
		}

	// being hit: the edges go dark red. not the middle, the edges, because
	// that is where the eye notices without losing sight of the room
	if (fight.hurt > 0.0) {
		double k = fight.hurt / 0.45;
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				double ex = fabs(2.0 * x / w - 1.0);
				double ey = fabs(2.0 * y / h - 1.0);
				double edge = fmax(ex, ey);
				if (edge < 0.55)
					continue;
				double f = (edge - 0.55) / 0.45 * k;
				unsigned int c = view[y * w + x];
				int r = (int)(((c >> 16) & 0xff) * (1 - f) + 190 * f);
				int g = (int)(((c >> 8) & 0xff) * (1 - f * 0.9));
				int b = (int)((c & 0xff) * (1 - f * 0.9));
				view[y * w + x] = (unsigned int)((r << 16) | (g << 8) | b);
			}
	}

	load_sheets();
	double lit = 0.34 + 0.62 * lit_at(player->x, player->y);
	if (lit > 1.0)
		lit = 1.0;
	if (fight_gun_out())
		draw_gun(lit, now, moving);
	else
		draw_pipe(lit, now, moving);

	// the sight only shows with the gun out. drawn all the time it
	// promises an aim a steel bar does not have, and you think you hold
	// a pistol before finding one. four strokes around a hole, and only
	// when there is a round to place.
	int cx = view_width / 2, cy = HORIZON;
	int u = view_width / HUD_DIVISOR;
	if (u < 1)
		u = 1;
	// every stroke carries its shadow. a sight of one colour vanishes
	// the moment it lands on a wall of the same value, which is what a
	// grey corridor is made of. it also spreads and turns amber for a
	// sixth of a second on a hit: the only immediate word that it told.
	int landed = fight.hitmark > 0.0;
	unsigned int stroke = landed ? 0xf0c060 : 0xd2e2ee;
	int across = landed ? 2 * u : 0;
	for (int k = 3 * u + across; fight_gun_out() && k <= 8 * u + across; k++)
		for (int side = 0; side < 4; side++) {
			int dx = side == 0 ? -1 : side == 1 ? 1 : 0;
			int dy = side == 2 ? -1 : side == 3 ? 1 : 0;
			int x = cx + dx * k, y = cy + dy * k;
			if (x < 1 || y < 1 || x >= view_width - 1 || y >= view_height - 1)
				continue;
			// the shadow first, across the stroke, then the stroke
			view[(y + (dx ? 1 : 0)) * view_width + x + (dy ? 1 : 0)] = 0x0a0e14;
			view[y * view_width + x] = stroke;
		}

	// and the impact where the round stopped: wide up close, a dot far
	// away, and it lasts a tenth of a second
	if (fight.spark > 0.0) {
		double d = fight.spark_depth < 0.5 ? 0.5 : fight.spark_depth;
		// iron on flesh makes a short spark, not a burst
		int r = (int)((fight_gun_out() ? 14 : 7) * u / d);
		if (r < u)
			r = u;
		if (r > 16 * u)
			r = 16 * u;
		double k = fight.spark / 0.11;
		for (int y = cy - r; y <= cy + r; y++)
			for (int x = cx - r; x <= cx + r; x++) {
				if (x < 0 || y < 0 || x >= view_width || y >= view_height)
					continue;
				double dx = (double)(x - cx) / r, dy = (double)(y - cy) / r;
				double q = dx * dx + dy * dy;
				if (q > 1.0)
					continue;
				double f2 = (1.0 - q) * k;
				unsigned int c = view[y * view_width + x];
				int rr = (int)(((c >> 16) & 0xff) + (255 - ((c >> 16) & 0xff)) * f2);
				int gg = (int)(((c >> 8) & 0xff) + (226 - ((c >> 8) & 0xff)) * f2);
				int bb = (int)((c & 0xff) + (150 - (int)(c & 0xff)) * f2 * 0.7);
				view[y * view_width + x] =
					(unsigned int)((rr << 16) | (gg << 8) | bb);
			}
	}
}

// ---------------------------------------------------------------- the hud
// one bar and one number at the bottom, nothing else. a hud with health,
// rounds, a compass and a minimap stops being read after two minutes.
// health changes colour before it changes length, so it warns early.
void fight_hud(double now)
{
	int unit = view_width / HUD_DIVISOR;
	if (unit < 1)
		unit = 1;
	int margin = 10 * unit, tall = 8 * unit, wide = 110 * unit;
	int y = view_height - margin - tall;
	double part = fight.health / PLAYER_HEALTH;
	if (part < 0.0)
		part = 0.0;
	unsigned int filled = part > 0.55 ? 0x6fd8c0
		: part > 0.25 ? 0xd8b04a : 0xc4423a;
	// under a quarter it beats. a still red bar sinks into the scenery
	// after ten seconds, a pulsing one never does.
	if (part <= 0.25) {
		double k = 0.62 + 0.38 * sin(now * 9.0);
		filled = (unsigned int)((int)(((filled >> 16) & 0xff) * k) << 16
			| (int)(((filled >> 8) & 0xff) * k) << 8
			| (int)((filled & 0xff) * k));
	}
	for (int i = 0; i < tall; i++)
		for (int j = 0; j < wide; j++) {
			int px = margin + j, py = y + i;
			if (px >= view_width || py >= view_height || px < 0 || py < 0)
				continue;
			int edge = i < unit || i >= tall - unit
				|| j < unit || j >= wide - unit;
			view[py * view_width + px] = edge ? 0x2b3440
				: (j < (int)(wide * part) ? filled : 0x141a22);
		}
	// both weapons, their key, and the key that swings, at all times. a
	// player who missed the teaching line has no way left to know how
	// to strike. three lines bottom right answer that once and for all.
	int t2 = unit * 2;
	int line_h = (GLYPH_H + 3) * t2;
	int bottom = view_height - margin;
	char line[28];
	int gun = fight_gun_out();

	snprintf(line, sizeof line, "1 PIPE");
	int l1 = (int)strlen(line) * (GLYPH_W + 1) * t2;
	draw_text(view_width - margin - l1, bottom - line_h * 3, line,
		gun ? 0x55606e : 0xe8e4da, t2);
	// the second line does not exist until the gun is found. greyed out
	// it still read as "you have a pistol", and the player hunted for
	// the key to draw it. what you do not own is not shown at all.
	if (fight.has_gun) {
		snprintf(line, sizeof line, "2 GUN %d", fight.ammo);
		int l2 = (int)strlen(line) * (GLYPH_W + 1) * t2;
		draw_text(view_width - margin - l2, bottom - line_h * 2, line,
			gun ? 0xf0c060 : 0x55606e, t2);
	}
	const char *attack = "CTRL  ATTACK";
	int l3 = (int)strlen(attack) * (GLYPH_W + 1) * unit;
	draw_text(view_width - margin - l3, bottom - line_h, attack, 0x6a7b8c, unit);
}

void fight_dim(double amount)
{
	double keep = 1.0 - amount;
	for (int i = 0; i < view_width * view_height; i++) {
		unsigned int c = view[i];
		int r = (int)(((c >> RED_SHIFT) & CHANNEL) * keep);
		int g = (int)(((c >> GREEN_SHIFT) & CHANNEL) * keep);
		int b = (int)((c & CHANNEL) * keep);
		view[i] = (unsigned int)((r << RED_SHIFT) | (g << GREEN_SHIFT) | b);
	}
}
