#include <math.h>
#include <stdio.h>
#include <string.h>

#include "story.h"
#include "render.h"
#include "render.h"
#include "screen.h"
#include "sound.h"
#include "text.h"

// ---------------------------------------------------------------- the rules
// a shake is an accent, never a state: short, decaying, along one axis. text
// is read where the eye already is, low and centre, one line at a time. an
// alarm is a source, not a filter: it lights the edges, the rest keeps its colour.

// the log tells what happens, it does not explain. what to do is said by the
// situation, never by the ticker.
static const char *LOG[] = {
	"KEEP STATION  DECK 3",
	"HULL BREACH  SECTIONS 7 12 14",
	"CREW CHANNEL  NO REPLY",
	"QUARANTINE  ENFORCED",
	"EVACUATION  DENIED",
};
#define LOG_COUNT ((int)(sizeof LOG / sizeof *LOG))

// ------------------------------------------------------------- the film
// what the bars are for: they open a space where the game is allowed to
// speak, and the player knows without being told that he does not have the
// keys meanwhile. five lines, one idea each, and the last one sets him walking.
static const char *OPENING[] = {
	"KEEP STATION    DECK THREE",
	"THE CREW STOPPED ANSWERING AT 0400",
	"QUARANTINE CAME DOWN AN HOUR LATER",
	"YOU WOKE UP ALONE IN THE AIRLOCK",
	"SOMETHING ELSE WOKE UP TOO",
};
#define OPENING_COUNT ((int)(sizeof OPENING / sizeof *OPENING))

// the second line counts what was done. closing lines that say the same thing
// to everyone reward nobody, so this one gives the player the number of his
// own game, which is the only trace that will be left of it.
static char tally[48] = "NOTHING ELSE IS BREATHING DOWN HERE";
static const char *CLOSING[] = {
	"DECK THREE IS QUIET",
	tally,
	"TANSOFTWARE",
};
#define CLOSING_COUNT ((int)(sizeof CLOSING / sizeof *CLOSING))

enum film { FILM_IN, FILM_TEXT, FILM_OUT, FILM_PLAY,
	    FILM_END_IN, FILM_END, FILM_DONE, FILM_DEAD };
static enum film film;
static double film_at;

static double started;
static int line_shown = -1;
static double line_at;          // when the current line appeared
static double alarm;            // how much the beacon is on, 0 to 1
static int quiet;               // the opening is over

// one hit at a time. it has a direction and a start, and it dies out on its
// own.
static double hit_at;           // when the shake itself starts
static double hit_len = 0.34;   // and how long it lasts
static double rumble_at;        // when the warning sound was played
static double hit_dx, hit_dy;
static int hits_done;
static double next_hit;
static double dust_until;
static double dust_x, dust_y;

// the caller does not carry the clock, so we keep the last one we were given
static double last_now;

void story_begin(double now)
{
	started = now;
	line_at = now;
	line_shown = -1;              // the log waits for the end of the film
	film = FILM_IN;
	film_at = now;
	// two hits at the start, and that is all. a shake is an accent of the
	// opening, and if you repeat it, it becomes a state that is unbearable
	// after twenty seconds. the player has to walk in peace ten seconds after
	// being scared.
	next_hit = now + 2.0;
}

void story_shake_here(double x, double y) { dust_x = x; dust_y = y; }

void story_hit(double now, double force, double x, double y)
{
	rumble_at = now;
	// a shake makes no sound of its own. what makes a sound is what happens,
	// a blast or a shot, and the shake is only its consequence on screen. it
	// lands at once when the cause is close, and the half second of warning
	// is only for a distant hit.
	hit_at = now + (force > 1.5 ? 0.0 : 0.5);
	// a big shake lasts longer, it is not only wider, because a blast that
	// stops after a third of a second looks like a hiccup, not like a blast
	hit_len = 0.34 + 0.55 * fmax(0.0, force - 1.0);
	double angle = now * 1.7;
	hit_dx = cos(angle) * force;
	hit_dy = sin(angle) * 0.6 * force;
	dust_x = x;
	dust_y = y;
	dust_until = now + 1.4;
}

void story_update(double now, int doors_opened)
{
	double t = now - started;

	if (line_shown < 0 && film >= FILM_OUT) {
		line_shown = 0;
		line_at = now;
	}
	// we show one line at a time, and it stays long enough to be read
	if (line_shown >= 0 && line_shown < LOG_COUNT - 1
	    && now - line_at > 5.0) {
		line_shown++;
		line_at = now;
	}

	if (!quiet) {
		// the rumble comes first and then the blow, because that half second
		// is what turns a jolt into an impact
		if (hits_done < 2 && now > next_hit) {
			rumble_at = now;
			hit_at = now + 0.55;
			double angle = 1.1 + hits_done * 2.3;
			// the second hit is weaker than the first, because it is moving
			// away
			double force = hits_done ? 0.55 : 1.0;
			hit_dx = cos(angle) * force;
			hit_dy = sin(angle) * 0.6 * force;
			next_hit = now + 6.0;
			hits_done++;
		}
		if (rumble_at > 0.0 && now > hit_at && now < hit_at + 0.05)
			dust_until = now + 1.4;
		// the beacon turns, with a sharp rise and a slow fall rather than a
		// sine. it also fades on its own after the opening, whether he moves
		// or not, because an alarm that waits for the player is a switch.
		double phase = fmod(t, 2.6) / 2.6;
		double swell = phase < 0.12 ? phase / 0.12
			: pow(1.0 - (phase - 0.12) / 0.88, 2.2);
		double left_over = t < 18.0 ? 1.0 : fmax(0.0, (24.0 - t) / 6.0);
		alarm = swell * left_over;
		// the door only makes the end come sooner, it is not the switch of
		// the storm
		if (doors_opened > 0 || t > 24.0)
			quiet = 1;
	} else if (alarm > 0.001) {
		alarm *= 0.94;
	}
}

// ------------------------------------------------------------- the film
static void film_next(enum film next, double now)
{
	film = next;
	film_at = now;
}

static void film_step(double now)
{
	double age = now - film_at;
	switch (film) {
	case FILM_IN:     if (age > BARS_IN) film_next(FILM_TEXT, now); break;
	case FILM_TEXT:   if (age > FILM_LINE * OPENING_COUNT)
				  film_next(FILM_OUT, now);
			  break;
	case FILM_OUT:    if (age > BARS_OUT) film_next(FILM_PLAY, now); break;
	case FILM_END_IN: if (age > BARS_IN) film_next(FILM_END, now); break;
	case FILM_END:    if (age > FILM_LINE * CLOSING_COUNT)
				  film_next(FILM_DONE, now);
			  break;
	case FILM_DEAD:   break;
	default: break;
	}
}

double story_bars(void)
{
	double age = last_now - film_at;
	switch (film) {
	case FILM_IN:     return BARS_MAX * fmin(1.0, age / BARS_IN);
	case FILM_TEXT:   return BARS_MAX;
	case FILM_OUT:    return BARS_MAX * fmax(0.0, 1.0 - age / BARS_OUT);
	case FILM_END_IN: return BARS_MAX * fmin(1.0, age / BARS_IN);
	case FILM_END:
	case FILM_DONE:   return BARS_MAX;
	case FILM_DEAD:   return BARS_MAX * fmin(1.0, age / BARS_IN);
	default:          return 0.0;
	}
}

// the legs are given back when the bars start to leave, not when they are
// gone. the player walks while they retreat, and that overlap is what makes
// it feel like the game takes over from the film.
int story_control(void)
{
	return film == FILM_OUT || film == FILM_PLAY;
}

void story_death(double now)
{
	if (film != FILM_DEAD)
		film_next(FILM_DEAD, now);
}

int story_dead(void) { return film == FILM_DEAD; }

// the world freezes, loses its colour and turns red. a death screen that
// leaves the picture as it is does not say that we lost, it says that we are
// waiting. colour draining away is the fastest signal there is.
void story_draw_death(void)
{
	if (film != FILM_DEAD)
		return;
	double k = fmin(1.0, (last_now - film_at) / 1.1);
	for (int i = 0; i < view_width * view_height; i++) {
		unsigned int c = view[i];
		int r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
		int grey = (r * 30 + g * 59 + b * 11) / 100;
		r = (int)((r * (1 - k) + grey * k) * (1.0 - 0.38 * k) + 34 * k);
		g = (int)((g * (1 - k) + grey * k) * (1.0 - 0.72 * k));
		b = (int)((b * (1 - k) + grey * k) * (1.0 - 0.72 * k));
		view[i] = (unsigned int)(((r > 255 ? 255 : r) << 16)
			| ((g > 255 ? 255 : g) << 8) | (b > 255 ? 255 : b));
	}
	int e = view_width > 700 ? 6 : 4;
	draw_text_centered(view_height / 2 - GLYPH_H * e, "YOU DIED",
		0xc4423a, e);
	if (last_now - film_at > 1.3) {
		draw_text_centered(view_height / 2 + GLYPH_H * e / 2,
			"SPACE TO TRY AGAIN", 0xc8bcb4, e / 2);
		draw_text_centered(view_height / 2 + GLYPH_H * e / 2 + GLYPH_H * e,
			"ESC FOR THE MENU", 0x6a5f5a, e / 3);
	}
}

// during the text the camera sweeps the room very slowly, because a still
// shot looks like a dead picture and a moving one looks like film
double story_drift(void)
{
	if (film != FILM_IN && film != FILM_TEXT)
		return 0.0;
	return 0.085 * sin((last_now - started) * 0.31);
}

void story_skip(double now)
{
	if (film == FILM_IN || film == FILM_TEXT)
		film_next(FILM_OUT, now);
}

void story_score(int amount)
{
	if (amount > 0)
		snprintf(tally, sizeof tally, "%d OF THEM WILL NOT GET UP",
			amount);
}

void story_finish(double now)
{
	if (film < FILM_END_IN)
		film_next(FILM_END_IN, now);
}

int story_finished(void) { return film == FILM_DONE; }

// the opening film does not play again at every death, because the first
// viewing sets the place and the third one would make you close the game. the
// legs are given back at once and the station is put back the way it was.
void story_restart(double now)
{
	film = FILM_PLAY;
	film_at = now;
	started = now;
	line_shown = 0;
	line_at = now;
	quiet = 1;
	alarm = 0.0;
	hit_at = 0.0;
	rumble_at = 0.0;
	hits_done = 2;
	dust_until = 0.0;
}

void story_draw_film(void)
{
	// we open on black, with one second of fade before the first picture. it
	// says that it begins, instead of saying that the window just opened, and
	// it costs nothing.
	double opening = (last_now - started) / 0.9;
	if (opening < 1.0) {
		if (opening < 0.0)
			opening = 0.0;
		for (int i = 0; i < view_width * view_height; i++) {
			unsigned int c = view[i];
			view[i] = ((int)(((c >> 16) & 0xff) * opening) << 16)
				| ((int)(((c >> 8) & 0xff) * opening) << 8)
				| (int)((c & 0xff) * opening);
		}
	}
	double part = story_bars();
	if (part <= 0.0)
		return;
	int tall = (int)(view_height * part);
	for (int y = 0; y < tall; y++)
		for (int x = 0; x < view_width; x++) {
			view[y * view_width + x] = 0x000000;
			view[(view_height - 1 - y) * view_width + x] = 0x000000;
		}

	const char **lines = NULL;
	int amount = 0;
	double age = last_now - film_at;
	if (film == FILM_TEXT) { lines = OPENING; amount = OPENING_COUNT; }
	else if (film == FILM_END) { lines = CLOSING; amount = CLOSING_COUNT; }
	if (!lines)
		return;
	int n = (int)(age / FILM_LINE);
	if (n >= amount)
		return;
	// each line rises, holds, and fades, so it never blinks
	double within = age - n * FILM_LINE;
	double k = within < 0.5 ? within / 0.5
		: (within > FILM_LINE - 0.7 ? (FILM_LINE - within) / 0.7 : 1.0);
	if (k < 0.0)
		k = 0.0;
	int scale = view_width > 700 ? 3 : 2;
	unsigned int c = 0xd6e8e2;
	int r = (int)(((c >> 16) & 0xff) * k), g = (int)(((c >> 8) & 0xff) * k);
	int b = (int)((c & 0xff) * k);
	draw_text_centered(view_height / 2 - scale * 4, lines[n],
		(unsigned int)((r << 16) | (g << 8) | b), scale);
	if (film == FILM_TEXT && n == 0 && within > 1.6)
		draw_text_centered((int)(view_height * (1.0 - BARS_MAX) - 14),
			"SPACE TO SKIP", 0x3a4a52, 1);
}

// the blast lights up and then dies. a shake without light looks like a
// display fault, while with the warm glow through the gap you know that
// something blew up, and where.
static double blast_until;

void story_blast(double now) { blast_until = now + 0.55; }

void story_draw_blast(void)
{
	if (last_now > blast_until)
		return;
	// a blast is a slap, not a sepia filter. it goes up to white and falls
	// back fast, because spread over a second it looks like a colour setting
	// that somebody forgot to remove.
	double k = (blast_until - last_now) / 0.55;
	k = k * k * 1.15;
	if (k > 1.0)
		k = 1.0;
	for (int y = 0; y < view_height; y++)
		for (int x = 0; x < view_width; x++) {
			unsigned int c = view[y * view_width + x];
			int r = (int)(((c >> 16) & 0xff) + (255 - ((c >> 16) & 0xff)) * k);
			int g = (int)(((c >> 8) & 0xff) + (238 - ((c >> 8) & 0xff)) * k);
			int b = (int)((c & 0xff) + (206 - (int)(c & 0xff)) * k * 0.9);
			view[y * view_width + x] = (unsigned int)
				(((r > 255 ? 255 : r) << 16)
				 | ((g > 255 ? 255 : g) << 8)
				 | (b > 255 ? 255 : (b < 0 ? 0 : b)));
		}
}

static double age_of(double now) { return now - hit_at; }

static int offset_of(double axis, double now)
{
	if (hit_at <= 0.0)
		return 0;
	double age = age_of(now);
	if (age < 0.0 || age > hit_len)
		return 0;
	double left = 1.0 - age / hit_len;
	return (int)(SHAKE_MAX * axis * left * left * cos(age * 55.0));
}

void story_tick(double now) { last_now = now; film_step(now); }
int story_shake(int axis) { return offset_of(axis ? hit_dy : hit_dx, last_now); }

int story_opening_over(void) { return quiet; }

// the alarm lights the top of the view and the edges, and leaves the rest
// alone, because a wash over everything is a filter and it shows
unsigned int story_grade_at(unsigned int color, int y)
{
	if (alarm < 0.02)
		return color;
	// only the top third and only the far edges, because an alarm lights what
	// faces it, it does not repaint the room
	double up = 1.0 - (double)y / (view_height * 0.30);
	if (up < 0.0)
		up = 0.0;
	// there is no vignette, because darkening or tinting the edges of the
	// screen is the most recognisable filter there is, and the eye reads it
	// as an effect, not as light. the alarm only lights the ceiling.
	double k = alarm * up * 0.42;
	if (k < 0.01)
		return color;
	int r = (color >> 16) & 0xff, g = (color >> 8) & 0xff, b = color & 0xff;
	r = (int)(r + (255 - r) * k * 0.45);
	g = (int)(g * (1.0 - k * 0.30));
	b = (int)(b * (1.0 - k * 0.38));
	return (unsigned int)((r << 16) | (g << 8) | b);
}

// the dust is in the world, not on the glass. each grain has a position on
// the floor and a height, it is projected like a sprite, and it stays where
// it fell even when you turn around.
#define GRAINS 80

void story_draw_dust(const struct player *player, double now)
{
	if (now > dust_until || !wall_depth)
		return;
	double age = 1.4 - (dust_until - now);
	double det = player->plane_x * player->dir_y - player->dir_x * player->plane_y;
	if (fabs(det) < 1e-9)
		return;
	double inv = 1.0 / det;

	for (int i = 0; i < GRAINS; i++) {
		// the position is drawn once and for all, around the point where the
		// hit landed, so it depends neither on the screen nor on the camera
		unsigned int hsh = (unsigned int)(i * 2654435761u) ^ (unsigned int)dust_x;
		double a = (double)((hsh >> 5) % 6283) / 1000.0;
		double r = 1.0 + (double)((hsh >> 11) % 500) / 100.0;
		double wx = dust_x + cos(a) * r, wy = dust_y + sin(a) * r;
		// it falls from the ceiling, which is at 1.0, while the eye is at 0.5
		double speed_of = 0.9 + (double)((hsh >> 19) % 60) / 100.0;
		double height_left = 1.0 - age * age * speed_of;
		if (height_left < 0.02)
			continue;

		double rx = wx - player->x, ry = wy - player->y;
		double side = inv * (player->dir_y * rx - player->dir_x * ry);
		double depth = inv * (-player->plane_y * rx + player->plane_x * ry);
		if (depth < 0.3)
			continue;
		int x = (int)((view_width / 2) * (1 + side / depth));
		if (x < 0 || x >= view_width || depth >= wall_depth[x])
			continue;
		int y = (int)(HORIZON + (0.5 - height_left) * view_height / depth);
		if (y < 0 || y >= view_height)
			continue;
		int size_of = depth < 2.5 ? 2 : 1;
		for (int dy = 0; dy < size_of; dy++)
			for (int dx = 0; dx < size_of; dx++)
				if (x + dx < view_width && y + dy < view_height)
					view[(y + dy) * view_width + x + dx] = 0x7d8794;
	}
}

// one line, large, low and centred, and it fades. that is where the eye
// already is.
static int notice_visible;
void story_notice(int visible) { notice_visible = visible; }

void story_draw_log(void)
{
	// we show one text at a time, and the notice of the game comes before the
	// log
	if (notice_visible || line_shown < 0 || line_shown >= LOG_COUNT)
		return;
	double age = last_now - line_at;
	if (age > 4.6)
		return;
	double k = age < 0.35 ? age / 0.35
		: (age > 3.8 ? fmax(0.0, (4.6 - age) / 0.8) : 1.0);
	int scale = view_width > 700 ? 3 : 2;
	// the text sits just above where the weapon will be, not in a corner
	int y = (int)(view_height * 0.70);
	unsigned int base = 0x000000, fresh = 0xbfe9dd;
	int r = (int)(((fresh >> 16) & 0xff) * k + ((base >> 16) & 0xff) * (1 - k));
	int g = (int)(((fresh >> 8) & 0xff) * k + ((base >> 8) & 0xff) * (1 - k));
	int b = (int)((fresh & 0xff) * k + (base & 0xff) * (1 - k));
	if (r + g + b < 24)
		return;
	draw_text_centered(y, LOG[line_shown],
		(unsigned int)((r << 16) | (g << 8) | b), scale);
}
