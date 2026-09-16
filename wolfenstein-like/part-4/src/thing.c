#include <math.h>
#include <string.h>

#include "thing.h"
#include "light.h"
#include "render.h"
#include "screen.h"
#include "sprite.h"
#include "world.h"

struct thing things[THINGS_MAX];

// the sheet, loaded once. five angles per step of the walk, four steps: the
// three missing angles come back mirrored, the way games did in 1993 to fit
// on a floppy.
#define ANGLES     5
#define WALK_STEPS 4
static struct sprite sheet, sheet_tough;

static void load_sheets(void)
{
	static int tried;
	if (!tried++) {
		sprite_load(&sheet, "art/thing.tecs");
		sprite_load(&sheet_tough, "art/heavy.tecs");
	}
}

// two beasts, one piece of code. what tells them apart is four numbers, and
// reading them here rather than scattering the test through the loop is what
// lets a third arrive without breaking anything.
static double reach_of(const struct thing *t)
{
	return t->tough ? TOUGH_REACH : THING_REACH;
}

static double damage_of(const struct thing *t)
{
	return t->tough ? TOUGH_DAMAGE : THING_DAMAGE;
}

static double speed_of(const struct thing *t)
{
	return t->tough ? TOUGH_SPEED : THING_SPEED;
}

static double size_of(const struct thing *t)
{
	return t->tough ? TOUGH_SIZE : 1.0;
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
			if (c == 'x' || c == 'y' || c == 'X')
				i = thing_add(x + 0.5, y + 0.5);
			if (i < 0)
				continue;
			if (c == 'y') {
				// a body on the floor: the level places them,
				// and they say what happened before us
				things[i].state = THING_DEAD;
				things[i].health = 0.0;
			}
			if (c == 'X') {
				things[i].tough = 1;
				things[i].health = TOUGH_HEALTH;
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
	// each axis on its own, so a shoulder on a wall keeps sliding
	if (!is_wall((int)(t->x + t->dir_x * move * 1.6), (int)t->y))
		t->x += t->dir_x * move;
	if (!is_wall((int)t->x, (int)(t->y + t->dir_y * move * 1.6)))
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
		int visible = sees(t, player);
		if (visible) {
			t->last_seen_x = player->x;
			t->last_seen_y = player->y;
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
					// it calls, and the neighbours get up
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
			if (now > t->stagger) {
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
				// it tells you it is coming. a quarter of a
				// second of posture before it lands: that is
				// the window to step back.
			} else if (!visible
				   && fabs(t->x - t->last_seen_x) < 0.4
				   && fabs(t->y - t->last_seen_y) < 0.4) {
				// it reached where you were and you are not
				// there: it loses you. breaking the line of
				// sight has to work, or the doors teach nothing
				t->state = THING_IDLE;
				t->since = now;
				t->silent = 0;
			}
			break;
		}
		case THING_STRIKE:
			// the blow lands at the end of the swing, not at the start:
			// that quarter second is the window to step back
			if (now - t->since > 0.45) {
				if (distance < reach_of(t) + 0.3)
					damage += damage_of(t);
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

	load_sheets();
	for (int n = 0; n < count; n++) {
		struct thing *t = &things[order[n]];
		const struct sprite *f = t->tough && sheet_tough.pixels
			? &sheet_tough : &sheet;
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
		double cosa = t->dir_x * to_x + t->dir_y * to_y;
		double sina = t->dir_x * to_y - t->dir_y * to_x;
		double angle = atan2(sina, cosa);            // -pi to pi
		int sector = (int)floor((angle + M_PI) / (2 * M_PI) * 8.0 + 0.5) % 8;
		int mirror = 0;
		if (sector > 4) {                            // the three missing
			sector = 8 - sector;
			mirror = 1;
		}
		int row = t->state == THING_DEAD ? WALK_STEPS
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
				if ((c >> 24) < 128)
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

// a cry carries. one waking thing calls others: that is what makes a fight
// never be against a single one, and what makes one think before being seen.
void things_hear(double x, double y, double reach, double now)
{
	for (int i = 0; i < THINGS_MAX; i++) {
		struct thing *t = &things[i];
		if (!t->used || t->state != THING_IDLE)
			continue;
		if (hypot(t->x - x, t->y - y) > reach)
			continue;
		t->state = THING_ALERT;
		t->since = now;
		t->last_seen_x = x;
		t->last_seen_y = y;
	}
}
