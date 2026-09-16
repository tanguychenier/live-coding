#include <math.h>
#include <string.h>

#include "thing.h"
#include "light.h"
#include "render.h"
#include "screen.h"
#include "sound.h"
#include "sprite.h"
#include "story.h"
#include "texture.h"
#include "world.h"

struct thing things[THINGS_MAX];

// the sheet is loaded once. it has five angles per walking step and four
// steps, and the three missing angles are taken in the mirror, the way games
// did in 1993 to fit on a floppy.
#define ANGLES 5
#define WALK_STEPS    4
// five angles, four steps, and one more row for the body on the floor,
// because a dead thing that stays standing cancels the only feedback the game
// gives when a shot lands
#define POSES  (WALK_STEPS + 1)
static struct sprite sheet, sheet_tough, sheet_boss, sheet_crew;

static void load_sheet(void)
{
	static int tried;
	if (!tried++) {
		sprite_load(&sheet, "art/thing.tecs");
		sprite_load(&sheet_tough, "art/heavy.tecs");
		sprite_load(&sheet_boss, "art/boss.tecs");
		sprite_load(&sheet_crew, "art/crew.tecs");
	}
}

// two beasts, one piece of code. what tells them apart is four numbers, and
// reading them here rather than scattering the test through the loop is what
// lets a third arrive without breaking anything.
static double reach_of(const struct thing *t)
{
	return t->boss ? BOSS_REACH : t->tough ? TOUGH_REACH : THING_REACH;
}

static double damage_of(const struct thing *t)
{
	return t->boss ? BOSS_DAMAGE : t->tough ? TOUGH_DAMAGE : THING_DAMAGE;
}

static double speed_of(const struct thing *t)
{
	// the ones in the battle move fast, because it is a melee, not a patrol
	return t->boss ? BOSS_SPEED : t->tough ? TOUGH_SPEED
		: t->roam ? 3.1 : THING_SPEED;
}

static double size_of(const struct thing *t)
{
	return t->boss ? BOSS_SIZE : t->tough ? TOUGH_SIZE : 1.0;
}

// the neon is turquoise in service, cold white at the machines and amber in
// the hold. the level file already says which one, so we only read it.
unsigned int zone_tint(double x, double y)
{
	for (int dy = -2; dy <= 2; dy++)
		for (int dx = -2; dx <= 2; dx++) {
			int k = wall_kind((int)x + dx, (int)y + dy);
			if (k == ALARM_TEXTURE)
				return 0xf0a53c;
			if (k == COLD_TEXTURE)
				return 0xcfe4ff;
		}
	return NEON_TUBE;
}

void things_clear(void)
{
	memset(things, 0, sizeof things);
}

void things_from_level(void)
{
	for (int y = 0; y < map_height; y++)
		for (int x = 0; x < map_width; x++) {
			char c = map[y][x];
			int i = -1;
			if (c == 'x' || c == 'y' || c == 'w' || c == 'Z'
			    || c == 'X' || c == 'n' || c == 'h')
				i = thing_add(x + 0.5, y + 0.5);
			if (i < 0)
				continue;
			if (c == 'y') {
				// a body on the floor: the level places them,
				// and they say what happened before us
				things[i].state = THING_DEAD;
				things[i].health = 0.0;
			} else if (c == 'w') {
				things[i].roam = 1;
				things[i].roam_x = x + 0.5;
				things[i].roam_y = y + 0.5;
				things[i].roam_to = 1.2 + (x % 3) * 0.5;
				things[i].roam_wob = 0.9;
			} else if (c == 'Z') {
				things[i].boss = 1;
				things[i].health = BOSS_HEALTH;
			} else if (c == 'X') {
				things[i].tough = 1;
				things[i].health = TOUGH_HEALTH;
			} else if (c == 'h') {
				things[i].crewman = 1;
				things[i].dir_x = -1.0;
				things[i].dir_y = 0.0;
			} else if (c == 'n') {
				// he lies on the floor, still breathing, and he has two or
				// three lines left before he stops
				things[i].crew = 1;
				things[i].state = THING_DEAD;
				things[i].health = 0.0;
				things[i].line = 1;
			}
		}
}

int thing_add(double x, double y)
{
	for (int i = 0; i < THINGS_MAX; i++)
		if (!things[i].used) {
			things[i] = (struct thing){ .x = x, .y = y, .dir_x = 1.0,
				.health = THING_HEALTH, .used = 1,
				.weave = i * 1.7 };
			return i;
		}
	return -1;
}

// can it see him? a straight line through the grid, and the first wall stops
// it. this is the same walk the rays do, kept simple on purpose
static int sees(const struct thing *t, const struct player *p)
{
	double dx = p->x - t->x, dy = p->y - t->y;
	double d = sqrt(dx * dx + dy * dy);
	if (d > THING_SIGHT)
		return 0;
	int steps = (int)(d * 8.0) + 1;
	for (int i = 1; i < steps; i++) {
		double f = (double)i / steps;
		if (is_wall((int)(t->x + dx * f), (int)(t->y + dy * f)))
			return 0;
	}
	return 1;
}

static void step_towards(struct thing *t, double tx, double ty, double elapsed)
{
	double dx = tx - t->x, dy = ty - t->y;
	double d = sqrt(dx * dx + dy * dy);
	if (d < 0.001)
		return;
	dx /= d; dy /= d;
	// it turns, it does not snap: a thing that pivots on the spot is a cursor
	double turn = 6.0 * elapsed;
	t->dir_x += (dx - t->dir_x) * turn;
	t->dir_y += (dy - t->dir_y) * turn;
	double n = sqrt(t->dir_x * t->dir_x + t->dir_y * t->dir_y);
	if (n > 0.0) { t->dir_x /= n; t->dir_y /= n; }

	double move = speed_of(t) * elapsed;
	// each axis on its own, so a shoulder on a wall keeps sliding, and the
	// shoulder is what we test, not the centre
	double side_x = t->dir_x > 0.0 ? THING_SHOULDER : -THING_SHOULDER;
	double side_y = t->dir_y > 0.0 ? THING_SHOULDER : -THING_SHOULDER;
	if (!is_wall((int)(t->x + t->dir_x * move + side_x), (int)t->y))
		t->x += t->dir_x * move;
	if (!is_wall((int)t->x, (int)(t->y + t->dir_y * move + side_y)))
		t->y += t->dir_y * move;
	t->stride += move * 1.7;
}

double things_update(const struct player *player, double elapsed, double now)
{
	double damage = 0.0;
	for (int i = 0; i < THINGS_MAX; i++) {
		struct thing *t = &things[i];
		if (!t->used || t->state == THING_DEAD)
			continue;
		double dx = player->x - t->x, dy = player->y - t->y;
		double distance = sqrt(dx * dx + dy * dy);
		// they fight, they do not stroll, and they will never see us. the
		// sideways back and forth is what makes it look like the beast
		// dodges, because a beast that moves in a straight line only looks
		// like a beast walking.
		if (t->roam) {
			double but = t->roam_x
				+ t->roam_to * sin(now * 2.4 + t->roam_x);
			double side = t->roam_y
				+ t->roam_wob * sin(now * 3.3 + t->roam_x * 3.0);
			step_towards(t, but, side, elapsed);
			// they also look at what they attack, not at us, because a beast
			// that steps back while facing the camera looks like a figurine
			// posing. the gaze is kept next to the walking direction, never
			// inside it, or it would steer the beast.
			if (t->roam_wob > 0.0) {
				t->look_x = 1.0;
				t->look_y = 0.0;
			} else {
				t->look_x = t->look_y = 0.0;
			}
			// it has reached the corridor, so it is out and we remove it
			if (t->leaving
			    && hypot(t->x - t->roam_x, t->y - t->roam_y) < 0.8)
				t->used = 0;
			continue;
		}
		// the crew holds its position and looks west. if they saw us through
		// the gap they would leave their post and come for us, and the scene
		// would fall apart.
		if (t->crewman)
			continue;
		int visible = sees(t, player);
		if (visible) {
			t->last_seen_x = player->x;
			t->last_seen_y = player->y;
		}

		// the pattern of the boss comes before everything else, because while
		// it is in one of its beats it does nothing else
		if (t->boss && t->state != THING_IDLE && t->state != THING_DEAD) {
			double age = now - t->move_at;
			switch (t->move) {
			case BOSS_WIND:
				if (age > BOSS_WIND_TIME) {
					t->move = BOSS_CHARGE;
					t->move_at = now;
					// it aims where you are now and it will not correct its
					// course, and that is what makes the charge something you
					// can dodge
					t->last_seen_x = player->x;
					t->last_seen_y = player->y;
				}
				continue;
			case BOSS_CHARGE: {
				double vx = t->last_seen_x - t->x;
				double vy = t->last_seen_y - t->y;
				double d = sqrt(vx * vx + vy * vy);
				if (d > 0.05) {
					double row = BOSS_SPEED * 3.1 * elapsed;
					if (!is_wall((int)(t->x + vx / d * row * 1.6), (int)t->y))
						t->x += vx / d * row;
					if (!is_wall((int)t->x, (int)(t->y + vy / d * row * 1.6)))
						t->y += vy / d * row;
					t->dir_x = vx / d;
					t->dir_y = vy / d;
					t->stride += row * 2.2;
				}
				if (distance < BOSS_REACH && now > t->next_strike) {
					damage += BOSS_CHARGE_HIT;
					t->next_strike = now + 1.0;
					sound_play(SFX_HURT, 0.0);
				}
				if (age > BOSS_CHARGE_TIME || d < 0.05) {
					t->move = BOSS_REST;
					t->move_at = now;
				}
				continue;
			}
			case BOSS_REST:
				// it is winded and it stands still. that is the window, and
				// it lasts long enough to be taken.
				if (age > BOSS_REST_TIME) {
					t->move = BOSS_WALK;
					t->move_at = now;
				}
				continue;
			case BOSS_SLAM:
				if (age > BOSS_WIND_TIME) {
					if (distance < BOSS_SLAM_REACH && visible)
						damage += BOSS_SLAM_HIT;
					sound_play(SFX_IMPACT, 0.0);
					// the floor takes the blow, so the picture does too,
					// because a blow that size that shakes nothing has no
					// weight
					story_hit(now, 1.3, t->x, t->y);
					t->move = BOSS_REST;
					t->move_at = now;
				}
				continue;
			case BOSS_WALK:
			default:
				if (age > BOSS_WALK_TIME && visible && distance < 12.0) {
					// under a third of its health it changes register, and it
					// goes for the floor instead of the legs
					int bottom = t->health < BOSS_HEALTH / 3.0;
					t->move = (bottom && distance < BOSS_SLAM_REACH)
						? BOSS_SLAM : BOSS_WIND;
					t->move_at = now;
					sound_play(SFX_GROWL, distance);
				}
				break;
			}
		}

		switch (t->state) {
		case THING_IDLE:
			// it does not charge the moment it sees you. it
			// wakes, it shrieks, and then it comes: that second
			// of warning turns a scare into a fight
			if (visible) {
				t->state = THING_ALERT;
				t->since = now;
				if (!t->silent) {
					sound_play(SFX_SHRIEK, distance);
					t->silent = 1;
					things_hear(t->x, t->y, 7.0, now);
				}
			}
			break;
		case THING_ALERT:
			if (now - t->since > 0.8) {
				t->state = THING_HUNT;
				t->since = now;
			}
			break;
		case THING_HUNT: {
			// it weaves as it closes. straight at you it is played
			// by walking backwards; across, it has to be followed.
			// the offset is worked out here and thrown away: writing
			// it into last_seen would pile up frame after frame.
			double bx = t->last_seen_x, by = t->last_seen_y;
			if (now > t->stagger && !t->boss) {
				double margin = distance > 3.0 ? 1.1 : distance * 0.35;
				double ox = -(player->y - t->y), oy = player->x - t->x;
				double n = sqrt(ox * ox + oy * oy);
				if (n > 0.001) {
					double k = margin * sin(now * 2.3 + t->weave);
					bx += ox / n * k;
					by += oy / n * k;
				}
			}
			// it stops at arm's length. without that it walks into
			// the player, drops under the nearest drawing distance,
			// and the blows come out of nowhere.
			if (distance > reach_of(t) * 0.85 && now > t->stagger)
				step_towards(t, bx, by, elapsed);
			if (distance < reach_of(t) && visible && now > t->next_strike) {
				t->state = THING_STRIKE;
				t->since = now;
				// it announces its blow with a quarter second of noise and
				// posture before it lands. that is the window to step back,
				// and without it you would lose health without ever knowing
				// where it came from.
				sound_play(SFX_PUNCH, distance);
			} else if (!visible
				   && fabs(t->x - t->last_seen_x) < 0.4
				   && fabs(t->y - t->last_seen_y) < 0.4) {
				// it reached where you were and you are not
				// there: it loses you. breaking the line of
				// sight has to work, or the doors teach nothing
				t->state = THING_IDLE;
				t->since = now;
				t->silent = 0;
				sound_play(SFX_GROWL, distance);
			}
			break;
		}
		case THING_STRIKE:
			// the blow takes a moment to come down, and that moment is the
			// time the player has to step back
			if (now - t->since > (t->boss ? BOSS_WINDUP : THING_WINDUP)) {
				if (distance < reach_of(t) + STRIKE_SLACK) {
					damage += damage_of(t);
					sound_play(SFX_HURT, 0.0);
				}
				t->state = THING_HUNT;
				t->since = now;
				t->next_strike = now + THING_GAP;
			}
			break;
		default:
			break;
		}
	}
	return damage;
}

// ------------------------------------------------------------------ drawing
// drawing a body from code was tried and dropped: distance fields give a
// shape that reads in ascii and a smear on screen. a sheet metal pattern
// can be computed, a body cannot. so we sculpt a sheet and project it.

void things_draw(const struct player *player, double now)
{
	// the camera matrix, inverted once for all of them
	double det = player->plane_x * player->dir_y - player->dir_x * player->plane_y;
	if (fabs(det) < 1e-9 || !wall_depth)
		return;
	double inv = 1.0 / det;

	// farthest first, so a near one covers a far one
	int order[THINGS_MAX], count = 0;
	for (int i = 0; i < THINGS_MAX; i++)
		if (things[i].used)
			order[count++] = i;
	for (int a = 0; a < count; a++)
		for (int b = a + 1; b < count; b++) {
			struct thing *p = &things[order[a]], *q = &things[order[b]];
			double da = (p->x - player->x) * (p->x - player->x)
				+ (p->y - player->y) * (p->y - player->y);
			double db = (q->x - player->x) * (q->x - player->x)
				+ (q->y - player->y) * (q->y - player->y);
			if (db > da) { int t = order[a]; order[a] = order[b]; order[b] = t; }
		}

	load_sheet();
	for (int n = 0; n < count; n++) {
		struct thing *t = &things[order[n]];
		const struct sprite *f = t->boss && sheet_boss.pixels ? &sheet_boss
			: t->tough && sheet_tough.pixels ? &sheet_tough
			: (t->crewman || t->crew) && sheet_crew.pixels ? &sheet_crew
			: &sheet;
		if (!f->pixels)
			continue;
		double rx = t->x - player->x, ry = t->y - player->y;
		double side = inv * (player->dir_y * rx - player->dir_x * ry);
		double depth = inv * (-player->plane_y * rx + player->plane_x * ry);
		if (depth < 0.35)
			continue;

		int middle = (int)((view_width / 2) * (1 + side / depth));
		// the feet stay on the floor whatever the size. the ground at
		// that depth is half a height below the horizon: start there
		// and go up. centring on the horizon floats anything that is
		// not exactly one corridor tall.
		double full = view_height / depth;
		// while it winds up it throws itself forward: a tenth bigger,
		// and that reads as a lunge
		double lunge = t->state == THING_STRIKE ? 1.10 : 1.0;
		int height = (int)(full * size_of(t) * lunge);
		int width = height * f->width / f->height;
		int top = (int)(HORIZON + full * 0.5) - height;

		// which angle we see of it: between the way it faces and the way
		// we look at it. head on when it comes, from behind when it
		// leaves. that is what makes it live in the world instead of
		// turning with the camera.
		double to_x = -rx, to_y = -ry;
		double n2 = sqrt(to_x * to_x + to_y * to_y);
		if (n2 > 0.0) { to_x /= n2; to_y /= n2; }
		// what it looks at, which is not always where it walks
		double fx = t->look_x || t->look_y ? t->look_x : t->dir_x;
		double fy = t->look_x || t->look_y ? t->look_y : t->dir_y;
		double cosa = fx * to_x + fy * to_y;
		double sina = fx * to_y - fy * to_x;
		double angle = atan2(sina, cosa);            // -pi a pi
		int sector = (int)floor((angle + M_PI) / (2 * M_PI) * 8.0 + 0.5) % 8;
		int mirror = 0;
		if (sector > 4) {                            // the three missing ones
			sector = 8 - sector;
			mirror = 1;
		}
		int row = t->state == THING_DEAD ? WALK_STEPS
			: t->crewman ? (now < t->fire_until ? 1 : 0)
			: ((int)(t->stride * 2.0) % WALK_STEPS);
		int frame = row * ANGLES + sector;
		if (frame >= f->frames)
			frame = sector % f->frames;

		// the contact shadow, and it is what puts it on the floor.
		// without it a thing drawn on the right pixel still looks like
		// it flies: the eye hunts for where it meets the ground and
		// does not find it.
		int ground = (int)(HORIZON + full * 0.5);
		int half_x = width / 3, half_y = (int)(full * 0.075) + 1;
		for (int y = ground - half_y; y <= ground + half_y; y++) {
			if (y < 0 || y >= view_height)
				continue;
			for (int x = middle - half_x; x <= middle + half_x; x++) {
				if (x < 0 || x >= view_width || depth >= wall_depth[x])
					continue;
				double ox = (double)(x - middle) / half_x;
				double oy = (double)(y - ground) / half_y;
				double r2 = ox * ox + oy * oy;
				if (r2 > 1.0)
					continue;
				double k = (1.0 - r2) * 0.55;
				unsigned int c = view[y * view_width + x];
				int r = (int)(((c >> 16) & 0xff) * (1 - k));
				int g = (int)(((c >> 8) & 0xff) * (1 - k));
				int b = (int)((c & 0xff) * (1 - k));
				view[y * view_width + x] =
					(unsigned int)((r << 16) | (g << 8) | b);
			}
		}

		// its floor of light is higher than the room's. a beast as dark
		// as the wall behind it is only seen as it swings, and that is
		// not tension, it is unfair.
		double light = 0.46 + 0.54 * lit_at(t->x, t->y);
		// it just took a hit: one washed-out frame, and one knows the
		// shot landed
		int hit = t->state != THING_DEAD && now - t->hurt_at < 0.09;
		for (int x = middle - width / 2; x < middle + width / 2; x++) {
			if (x < 0 || x >= view_width || depth >= wall_depth[x])
				continue;
			int sx = (x - (middle - width / 2)) * f->width / width;
			if (mirror)
				sx = f->width - 1 - sx;
			for (int y = top; y < top + height; y++) {
				if (y < 0 || y >= view_height)
					continue;
				int sy = (y - top) * f->height / height;
				unsigned int c = sprite_at(f, frame, sx, sy);
				if (!sprite_solid(c))
					continue;
				// it takes the light of the square it stands on,
				// or it glows in the dark like a sticker
				int r = (int)(((c >> 16) & 0xff) * light);
				int g = (int)(((c >> 8) & 0xff) * light);
				int b = (int)((c & 0xff) * light);
				if (hit) {
					r += (255 - r) / 2; g += (255 - g) / 2;
					b += (255 - b) / 2;
				}
				view[y * view_width + x] =
					(unsigned int)((r << 16) | (g << 8) | b);
			}
		}
	}
}

// the boss bar only shows once the boss has seen us. shown from the start it
// would announce the fight before the room, while shown when it wakes, it
// becomes the start of the fight.
double boss_health(void)
{
	for (int i = 0; i < THINGS_MAX; i++) {
		struct thing *t = &things[i];
		if (t->used && t->boss && t->state != THING_IDLE
		    && t->state != THING_DEAD)
			return t->health / BOSS_HEALTH;
	}
	return -1.0;
}

// its death is a separate question. if we read dead as health at zero, the
// bar would stay on screen, empty, after the last blow, still saying that
// there is something to kill.
int boss_down(void)
{
	for (int i = 0; i < THINGS_MAX; i++)
		if (things[i].used && things[i].boss
		    && things[i].state == THING_DEAD)
			return 1;
	return 0;
}

// while it rests, it takes double damage. that is the whole contract of the
// fight, you take the charge or you dodge it, and you get paid right after. a
// noise also carries far, and that is what makes shooting expensive, because
// the gun saves time but the price is the room next door waking up.
void things_hear(double x, double y, double reach, double now)
{
	for (int i = 0; i < THINGS_MAX; i++) {
		struct thing *t = &things[i];
		if (!t->used || t->roam || t->crewman || t->crew
		    || t->state != THING_IDLE)
			continue;
		if (hypot(t->x - x, t->y - y) > reach)
			continue;
		t->state = THING_ALERT;
		t->since = now;
		t->last_seen_x = x;
		t->last_seen_y = y;
	}
}

int boss_resting(void)
{
	for (int i = 0; i < THINGS_MAX; i++)
		if (things[i].used && things[i].boss
		    && things[i].state != THING_DEAD
		    && things[i].move == BOSS_REST)
			return 1;
	return 0;
}

double thing_weak(const struct thing *t)
{
	return t->boss && t->move == BOSS_REST ? 2.0 : 1.0;
}

// ----------------------------------------------------------- the survivors
// three lines, and the last one never comes out whole. we never see them
// standing: the game starts afterwards.
static const char *LINES[][3] = {
	{ "DO NOT GO LEFT", "IT CAME THROUGH THE WALL", "TELL THEM WE" },
	{ "TOOK THE BADGE DOWN TO THE HOLD", "IT IS STILL DOWN THERE", "..." },
	{ "SEVENTEEN OF US CAME UP HERE", "I AM THE SEVENTEENTH", "" },
};
#define LINES_COUNT ((int)(sizeof LINES / sizeof *LINES))

const char *crew_speak(const struct player *player, double now)
{
	int rank = 0;
	for (int i = 0; i < THINGS_MAX; i++) {
		struct thing *t = &things[i];
		if (!t->used || !t->crew)
			continue;
		int mine = rank++ % LINES_COUNT;
		if (t->line > 3 || now < t->next_line)
			continue;
		if (hypot(t->x - player->x, t->y - player->y) > 2.8)
			continue;
		const char *word = LINES[mine][t->line - 1];
		t->line++;
		t->next_line = now + 4.2;
		if (t->line > 3) {
			// he stops halfway through, and that is all we will ever know
			sound_play(SFX_DIE, 1.0);
			t->next_line = now + NEVER;
		}
		return word && *word ? word : NULL;
	}
	return NULL;
}

// one of them falls during the scene. we hear someone die behind the door,
// and if nobody fell on screen the sound would be just a sound. the one that
// falls is the closest to us, so it is the biggest in the gap.
void things_roam_kill(void)
{
	struct thing *pick = NULL;
	double nearest = VERY_FAR;
	for (int i = 0; i < THINGS_MAX; i++) {
		struct thing *t = &things[i];
		if (!t->used || !t->roam || t->state == THING_DEAD)
			continue;
		if (t->x < nearest) {
			nearest = t->x;
			pick = t;
		}
	}
	if (pick) {
		pick->state = THING_DEAD;
		pick->health = 0.0;
	}
}

// after the blast, the ones seen through the gap leave. during the scene they
// close in on the crew instead of pacing.
void things_roam_advance(void)
{
	for (int i = 0; i < THINGS_MAX; i++)
		if (things[i].used && things[i].roam
		    && things[i].state != THING_DEAD) {
			things[i].roam_x += 4.5;
			things[i].roam_to = 0.5;
		}
}

// the two crewmen, in the order they appear in the level. the scene calls
// them by number, because that is easier to read than a search by position,
// and there will never be three of them.
static struct thing *crewman_at(int which)
{
	int rank = 0;
	for (int i = 0; i < THINGS_MAX; i++)
		if (things[i].used && things[i].crewman && rank++ == which)
			return &things[i];
	return NULL;
}

// when a round passes close, the beast flashes white for an instant. it is
// the detail that says the shots land somewhere, and that the scene is not a
// backdrop.
void things_roam_graze(double now)
{
	static int swell;
	int rank = 0;
	for (int i = 0; i < THINGS_MAX; i++) {
		struct thing *t = &things[i];
		if (!t->used || !t->roam || t->state == THING_DEAD)
			continue;
		if (rank++ == swell % 3)
			t->hurt_at = now;
	}
	swell++;
}

void crew_fire(int which, double now)
{
	struct thing *t = crewman_at(which);
	if (!t || t->state == THING_DEAD)
		return;
	t->fire_until = now + 0.16;
	sound_play(SFX_SHOT, 8.0);
}

void crew_fall(int which)
{
	struct thing *t = crewman_at(which);
	if (!t)
		return;
	t->state = THING_DEAD;
	t->health = 0.0;
}

void things_roam_away(void)
{
	// they leave by a real exit, and then they no longer exist, because a
	// beast treading against a bulkhead in full view is not frightening any
	// more. the level gives them a corridor to the north, outside what the
	// gap lets us see, they take it, and once inside they are removed.
	for (int i = 0; i < THINGS_MAX; i++)
		if (things[i].used && things[i].roam) {
			things[i].roam_x = 38.5;
			things[i].roam_y = 1.5;
			things[i].roam_to = 0.0;
			things[i].roam_wob = 0.0;
			things[i].leaving = 1;
		}
}
