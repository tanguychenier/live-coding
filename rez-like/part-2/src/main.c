// a shooter on a rail, drawn in lines of light. the eye rides the rail, the
// world breathes on the beat, things come at you, you mark up to eight of
// them and let go, and every shot lands on a sixteenth and plays a note

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "demo.h"
#include "draw.h"
#include "font.h"
#include "hero.h"
#include "level.h"
#include "palette.h"
#include "particle.h"
#include "player.h"
#include "rail.h"
#include "screen.h"
#include "sound.h"
#include "thing.h"
#include "tunnel.h"
#include "world.h"

#define GAME_NAME        "AXON"
// the eye looks this far ahead of itself along the rail
#define LOOK_AHEAD       0.35
// the roll of the camera follows the sideways bend of the rail, in radians
// per unit of sideways lean of the point looked at, and eases to it at
// this rate
#define ROLL_GAIN        0.072
#define ROLL_EASE        3.0
// a frame is never longer than this, whatever the machine did meanwhile
#define FRAME_CAP        0.05
#define FRAME_SECONDS    (1.0 / 60.0)
// a chain of eight punches the eye in, the focal growing by this much and
// easing back over this long
#define PUNCH_ZOOM       0.2
#define PUNCH_TIME       0.45
// the name of a zone stays big in the middle this long
#define ZONE_TITLE_TIME  3.0
#define ZONE_TITLE_SIZE  8.0
// the first seconds say what the hands do, and the words fade over the last
#define LESSON_TIME      9.0
#define LESSON_FADE      1.0
// the groove of the tunnel builds up, the bass from this bar, the hats
// from this one at half, then whole from this one
#define UPLINK_BASS_BAR  4.0
#define UPLINK_HAT_BAR   8.0
#define UPLINK_HAT_HALF  0.4
#define UPLINK_HAT_FULL_BAR 32.0
// where the numbers sit
#define HUD_MARGIN       14.0
#define HUD_TEXT_MAX     64       // a line of the numbers, at most
#define HUD_SCORE_SIZE   4.0
#define HUD_ZONE_SIZE    2.5
#define HUD_CHAIN_SIZE   4.0
#define HUD_CHAIN_GROW   2.0      // the chain is this much bigger the instant it lands
#define HUD_HEALTH_STEP  22.0
#define HUD_HEALTH_WIDE  16.0
#define HUD_LESSON_Y     64.0     // the two lines of the lesson, under the score
#define HUD_GLOW         0.6      // the numbers also go in the light, this much
#define HUD_BLINK        2.0      // blinks per second of the press enter line
#define HUD_BLINK_ON     0.6      // and how much of each blink is on
// where the big lines sit, as fractions of the height, and how far apart
#define TITLE_Y          0.3
#define LINE_GAP         34.0
#define BIG_SIZE         6.0
#define MID_SIZE         4.0
#define SMALL_SIZE       3.0

struct game {
	struct rail rail;
	struct player player;
	struct hero hero;
	struct level level;
	struct camera camera;
	int zone;
	double zone_at;
	double t_eye;
	double roll;
	double punch_at;
};

static double now_in_seconds(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

// a run starts at a moment of the level, with the score it had there
static void begin_run(struct game *game, double at, long score)
{
	sound_restart(at);
	double now = sound_now();
	player_reset(&game->player);
	game->player.score = score;
	hero_reset(&game->hero);
	things_clear();
	particles_clear();
	level_reset(&game->level, now, score);
	game->zone = level_zone(now);
	game->zone_at = now;
	game->t_eye = level_t_eye(&game->rail, now);
	game->roll = 0.0;
	game->punch_at = -PUNCH_TIME;
	sound_zone(game->zone);
	demo_reset();
}

// the levels of the layers, kick, hat, bass, pad. the tunnel builds the
// groove one part at a time, the bass after four bars and the hats after
// eight, half open, then whole after thirty two
static const double MIX[ZONES][LAYER_COUNT] = {
	{ 1.0, 1.0, 1.0, 0.0, 0.0, 0.0 },
	{ 1.0, 1.0, 1.0, 0.8, 0.0, 0.0 },
	{ 1.0, 1.0, 1.0, 0.5, 0.9, 0.0 },
	{ 1.0, 1.0, 1.0, 0.6, 0.6, 0.9 },
};

static void set_layers(const struct game *game, double now)
{
	double bars = (now - level_zone_time(game->zone)) / BAR;
	double level[LAYER_COUNT];
	for (int i = 0; i < LAYER_COUNT; i++)
		level[i] = MIX[game->zone][i];
	if (game->zone == ZONE_UPLINK) {
		level[LAYER_BASS] = bars >= UPLINK_BASS_BAR ? 1.0 : 0.0;
		level[LAYER_HAT] = bars < UPLINK_HAT_BAR ? 0.0
			: bars < UPLINK_HAT_FULL_BAR ? UPLINK_HAT_HALF : 1.0;
	}
	for (int i = 0; i < LAYER_COUNT; i++)
		sound_layer((enum layer)i, level[i]);
}

// the eye follows the rail and looks a little ahead of itself. in a bend,
// the point looked at is off to one side, and the roll leans into it the
// way a rider leans into a curve
static void place_camera(struct game *game, struct sight *sight, double elapsed, double now)
{
	struct vec eye, at, forward, right, up, motion, bend;
	rail_frame(&game->rail, game->t_eye, &forward, &right, &up);
	eye = sub(rail_at(&game->rail, game->t_eye), scale(up, EYE_DROP));
	at = rail_at(&game->rail, game->t_eye + LOOK_AHEAD);
	double lean = dot(sub(at, rail_at(&game->rail, game->t_eye)), right);
	game->roll += (-lean / LOOK_AHEAD * ROLL_GAIN - game->roll) * ROLL_EASE * elapsed;
	// on the rail the way is taken as straight, a bolt has a moment to
	// bend if it is not
	motion = scale(forward, level_speed(&game->rail, game->t_eye) * RAIL_SPACING);
	bend = vec(0, 0, 0);
	// the punch of a full chain
	double focal = FOCAL;
	double punch = 1.0 - (now - game->punch_at) / PUNCH_TIME;
	if (punch > 0.0)
		focal *= 1.0 + PUNCH_ZOOM * sin(punch * M_PI);
	camera_look(&game->camera, eye, at, vec(0, 1, 0), game->roll, focal);
	sight->eye = eye;
	sight->forward = game->camera.forward;
	sight->motion = motion;
	sight->bend = bend;
}

static void step_play(struct game *game, const struct keys *keys, double elapsed, double now)
{
	int zone = level_zone(now);
	if (zone != game->zone) {
		game->zone = zone;
		game->zone_at = now;
		sound_zone(zone);
	}
	game->t_eye = level_t_eye(&game->rail, now);
	struct sight sight;
	place_camera(game, &sight, elapsed, now);
	level_update(&game->level, &game->rail, game->t_eye, now, game->player.score);
	things_update(&game->rail, game->t_eye, &sight, elapsed, now);
	// nothing hurts yet, the hits come with the life
	int hurt = 0;
	player_update(&game->player, keys, elapsed);
	player_aim(&game->player, &game->camera, game->hero.hands, now);
	if (game->player.released_full)
		game->punch_at = now;
	hero_update(&game->hero, &game->camera, game->player.cursor_x, game->player.cursor_y,
		    game->player.released, hurt, elapsed, now);
	particles_update(elapsed);
	if (!game->player.alive)
		things_scatter();
}

// the numbers are laid out in base pixels and scaled to the picture
static double px(double base)
{
	return base * draw_scale();
}

static void write_centered(double y, const char *text, double size, struct light colour)
{
	font_write((view_width - font_width(text, size)) / 2.0, y, text, size, colour);
}

// the numbers along the top, the health along the bottom. drawn twice, once
// in the light so that they glow, once sharp so that they read
static void draw_hud(const struct game *game, double now, double gain)
{
	const struct player *player = &game->player;
	char text[HUD_TEXT_MAX];
	snprintf(text, sizeof text, "%08ld", player->score);
	font_write(px(HUD_MARGIN), px(HUD_MARGIN - 2), text, px(HUD_SCORE_SIZE),
		   light_scale(LIGHT_HUD, gain));
	font_write(px(HUD_MARGIN), px(HUD_MARGIN + 26), palette_of(game->zone)->name,
		   px(HUD_ZONE_SIZE), light_scale(LIGHT_HUD_DIM, gain));
	// the chain, big, for a moment after a release
	if (player->chain > 1 && now - player->chain_at < CHAIN_SHOW) {
		double fade = 1.0 - (now - player->chain_at) / CHAIN_SHOW;
		snprintf(text, sizeof text, "X%d", player->chain);
		double size = px(HUD_CHAIN_SIZE + HUD_CHAIN_GROW * fade);
		font_write(view_width - px(HUD_MARGIN) - font_width(text, size), px(HUD_MARGIN - 2),
			   text, size, light_scale(light_mix(LIGHT_HUD_DIM, LIGHT_CHAIN, fade), gain));
	}
	// health, eight bars along the bottom, the lost ones left dark
	for (int i = 0; i < HEALTH_MAX; i++) {
		double x = px(HUD_MARGIN + i * HUD_HEALTH_STEP);
		double y = view_height - px(HUD_MARGIN);
		struct light lit = light_scale(i < player->health ? LIGHT_HUD : LIGHT_HUD_OFF, gain);
		draw_line_2d(x, y, x + px(HUD_HEALTH_WIDE), y, lit);
		draw_line_2d(x, y + px(2), x + px(HUD_HEALTH_WIDE), y + px(2), lit);
	}
	// the name of the zone, big, when it starts
	double title = 1.0 - (now - game->zone_at) / ZONE_TITLE_TIME;
	if (title > 0.0) {
		double fade = sin(title * M_PI);
		write_centered(view_height * TITLE_Y, palette_of(game->zone)->name, px(ZONE_TITLE_SIZE),
			       light_scale(LIGHT_HUD, gain * fade));
	}
	// the first seconds say what the hands do
	if (now < LESSON_TIME) {
		double fade = fmin(1.0, (LESSON_TIME - now) / LESSON_FADE);
		write_centered(px(HUD_LESSON_Y), "ARROWS OR MOUSE MOVE THE SIGHT", px(HUD_ZONE_SIZE),
			       light_scale(LIGHT_HUD_DIM, gain * fade));
		write_centered(px(HUD_LESSON_Y + 16), "HOLD SPACE OR CLICK TO MARK   LET GO TO FIRE",
			       px(HUD_ZONE_SIZE), light_scale(LIGHT_HUD_DIM, gain * fade));
	}
}

static void draw_frame(struct game *game, double now)
{
	const struct palette *pal = palette_of(game->zone);
	draw_clear(pal->sky_top, pal->sky_bottom);
	draw_fog(pal->fog);
	draw_pulse(exp(-sound_beat_phase(now) * BEAT_DECAY));
	world_draw(&game->camera, &game->rail, game->t_eye, now, game->zone, pal);
	things_draw(&game->camera, now);
	particles_draw(&game->camera);
	hero_draw(&game->hero, &game->camera, now);
	player_draw(&game->player, &game->camera, now);
	draw_hud(game, now, HUD_GLOW);
	draw_finish();
	draw_hud(game, now, 1.0);
}

int main(void)
{
	static struct game the_game;
	struct game *game = &the_game;
	level_build_rail(&game->rail);
	sound_open();
	// a run can start anywhere in the level, for a look at a zone
	const char *start = getenv("TEC_START");
	double start_at = start ? floor(atof(start) / BAR) * BAR : 0.0;
	begin_run(game, start_at, 0);
	struct screen screen;
	if (!screen_open(&screen, GAME_NAME))
		return 1;
	struct keys keys = { 0 };
	double last = now_in_seconds();
	const char *trace = getenv("TEC_TRACE");
	double traced_at = 0.0;
	// on the bench the game plays itself, as a hand would
	int pilot = getenv("TEC_PILOT") != NULL;

	while (!keys.quit) {
		double moment = now_in_seconds();
		double elapsed = moment - last;
		if (elapsed > FRAME_CAP)
			elapsed = FRAME_CAP;
		last = moment;
		double now = sound_now();
		screen_read_keys(&screen, &keys);
		if (keys.mute) {
			keys.mute = 0;
			sound_mute(!sound_muted());
		}
		if (pilot)
			demo_drive(&game->player, &game->camera, &keys, now);
		step_play(game, &keys, elapsed, now);
		set_layers(game, now);
		draw_frame(game, now);
		screen_present(&screen);
		if (trace && moment - traced_at > 0.5) {
			traced_at = moment;
			fprintf(stderr, "t %.1f zone %d eye %.2f things %d score %ld health %d\n",
				now, game->zone, game->t_eye, things_count(), game->player.score, game->player.health);
		}
		double spent = now_in_seconds() - moment;
		if (spent < FRAME_SECONDS)
			usleep((useconds_t)((FRAME_SECONDS - spent) * 1e6));
	}
	sound_close();
	screen_close(&screen);
	return 0;
}
