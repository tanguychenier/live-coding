// a shooter on a rail, drawn in lines of light. the eye rides the rail, the
// world breathes on the beat, things come at you, you mark up to eight of
// them and let go, and every shot lands on a sixteenth and plays a note

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "boss.h"
#include "demo.h"
#include "draw.h"
#include "font.h"
#include "hero.h"
#include "level.h"
#include "palette.h"
#include "particle.h"
#include "player.h"
#include "pool.h"
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
// a hit shakes the eye for this long and this far, in world units
#define HURT_SHAKE       0.35
#define SHAKE_SIZE       0.35
#define SHAKE_RATE       40.0
// a hit washes the picture red for this long
#define HURT_WASH        0.25
#define HURT_WASH_GAIN   0.16
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
// on the death screen enter only counts after this, so that a hand still
// mashing fire does not skip it
#define DEATH_WAIT       1.0
// the end fades in from white over this long, and goes back to the start
// on its own after this long without a key
#define WON_FADE         3.0
#define WON_IDLE         30.0
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
#define HUD_BAR_WIDE     240.0
#define HUD_BAR_Y        16.0
#define HUD_LESSON_Y     64.0     // the two lines of the lesson, under the score
#define HUD_GLOW         0.6      // the numbers also go in the light, this much
#define HUD_BLINK        2.0      // blinks per second of the press enter line
#define HUD_BLINK_ON     0.6      // and how much of each blink is on
// where the big lines sit, as fractions of the height, and how far apart
#define TITLE_Y          0.3
#define DEAD_Y           0.36
#define WON_Y            0.26
#define LINE_GAP         34.0
#define BIG_SIZE         6.0
#define MID_SIZE         4.0
#define SMALL_SIZE       3.0
// the end wash, how white it goes before the screen comes
#define WON_WASH         2.0

enum state { STATE_PLAY, STATE_DEAD, STATE_WON };

struct game {
	struct rail rail;
	struct player player;
	struct hero hero;
	struct level level;
	struct boss boss;
	struct camera camera;
	enum state state;
	double state_at;
	int zone;
	double zone_at;
	double t_eye;
	double roll;
	double punch_at;
	double flash_at;
	int fire_let_go;         // on the death screen, fire has been let go since the death
	long final_score;
	int final_chain;
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
	boss_reset(&game->boss);
	level_reset(&game->level, now, score);
	game->state = STATE_PLAY;
	game->state_at = now;
	game->zone = level_zone(now);
	game->zone_at = now;
	game->t_eye = level_t_eye(&game->rail, now);
	game->roll = 0.0;
	game->punch_at = -PUNCH_TIME;
	game->flash_at = -HURT_WASH;
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
static const double MIX_WON[LAYER_COUNT] = { 0.0, 0.0, 0.0, 1.0, 0.3, 0.0 };
static const double MIX_DEAD[LAYER_COUNT] = { 0.0, 0.0, 0.0, 0.5, 0.0, 0.0 };

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
	if (game->boss.dead || game->state == STATE_WON)
		for (int i = 0; i < LAYER_COUNT; i++)
			level[i] = MIX_WON[i];
	if (game->state == STATE_DEAD)
		for (int i = 0; i < LAYER_COUNT; i++)
			level[i] = MIX_DEAD[i];
	for (int i = 0; i < LAYER_COUNT; i++)
		sound_layer((enum layer)i, level[i]);
}

// the eye follows the rail and looks a little ahead of itself. in a bend,
// the point looked at is off to one side, and the roll leans into it the
// way a rider leans into a curve
static void place_camera(struct game *game, struct sight *sight, double elapsed, double now)
{
	struct vec eye, at, forward, right, up, motion, bend;
	if (game->boss.active) {
		boss_eye(&game->boss, now, &eye, &at, &motion, &bend);
		game->roll += (0.0 - game->roll) * fmin(1.0, ROLL_EASE * elapsed);
	} else {
		rail_frame(&game->rail, game->t_eye, &forward, &right, &up);
		eye = sub(rail_at(&game->rail, game->t_eye), scale(up, EYE_DROP));
		at = rail_at(&game->rail, game->t_eye + LOOK_AHEAD);
		double lean = dot(sub(at, rail_at(&game->rail, game->t_eye)), right);
		game->roll += (-lean / LOOK_AHEAD * ROLL_GAIN - game->roll) * ROLL_EASE * elapsed;
		// on the rail the way is taken as straight, a bolt has a moment
		// to bend if it is not
		motion = scale(forward, level_speed(&game->rail, game->t_eye) * RAIL_SPACING);
		bend = vec(0, 0, 0);
	}
	// a hit knocks the eye sideways for a moment
	if (now - game->player.hurt_at < HURT_SHAKE && game->player.hurt_at > 0) {
		double fade = 1.0 - (now - game->player.hurt_at) / HURT_SHAKE;
		struct vec side = unit(cross(sub(at, eye), vec(0, 1, 0)));
		eye = add(eye, scale(side, SHAKE_SIZE * fade * ((int)(now * SHAKE_RATE) % 2 ? 1 : -1)));
	}
	// the punch of a full chain
	double focal = FOCAL;
	double punch = 1.0 - (now - game->punch_at) / PUNCH_TIME;
	if (punch > 0.0)
		focal *= 1.0 + PUNCH_ZOOM * sin(punch * M_PI);
	if (game->boss.active)
		focal *= 1.0 + BOSS_ZOOM * (0.5 + 0.5 * sin(2 * M_PI * (now - game->boss.began)
							    / BOSS_ZOOM_PERIOD));
	camera_look(&game->camera, eye, at, vec(0, 1, 0), game->roll, focal);
	sight->eye = eye;
	sight->forward = game->camera.forward;
	sight->motion = motion;
	sight->bend = bend;
}

// the run is over, won or given up, back to the start
static void end_run(struct game *game)
{
	begin_run(game, 0.0, 0);
}

// the death screen keeps the world as it was, only the sparks and the
// pilot go on moving
static void step_frozen(struct game *game, double elapsed, double now)
{
	struct sight sight;
	place_camera(game, &sight, elapsed, now);
	hero_update(&game->hero, &game->camera, game->player.cursor_x, game->player.cursor_y, 0, 0,
		    elapsed, now);
	particles_update(elapsed);
	if (game->state == STATE_WON)
		boss_update(&game->boss, &sight, elapsed, now);
}

static void step_play(struct game *game, const struct keys *keys, double elapsed, double now)
{
	int zone = level_zone(now);
	if (zone != game->zone) {
		game->zone = zone;
		game->zone_at = now;
		sound_zone(zone);
	}
	if (!game->boss.active) {
		game->t_eye = level_t_eye(&game->rail, now);
		if (game->t_eye >= rail_end(&game->rail)) {
			double end = rail_end(&game->rail);
			boss_begin(&game->boss, world_arena_centre(&game->rail), rail_at(&game->rail, end),
				   rail_forward(&game->rail, end), now);
		}
	}
	struct sight sight;
	place_camera(game, &sight, elapsed, now);
	if (!game->boss.active)
		level_update(&game->level, &game->rail, game->t_eye, now, game->player.score);
	boss_update(&game->boss, &sight, elapsed, now);
	if (boss_phase_changed(&game->boss))
		game->punch_at = now;
	int reached = things_update(&game->rail, game->t_eye, &sight, elapsed, now);
	int hurt = player_hurt(&game->player, reached, now);
	if (hurt)
		game->flash_at = now;
	player_update(&game->player, keys, elapsed);
	player_aim(&game->player, &game->camera, game->hero.hands, now);
	if (game->player.released_full)
		game->punch_at = now;
	hero_update(&game->hero, &game->camera, game->player.cursor_x, game->player.cursor_y,
		    game->player.released, hurt, elapsed, now);
	particles_update(elapsed);
	if (!game->player.alive) {
		// the signal is lost and everything in it bursts, so that the
		// screen that follows is written on the dark
		game->state = STATE_DEAD;
		game->state_at = now;
		game->fire_let_go = 0;
		things_scatter();
	}
	if (game->boss.dead && now - game->boss.dead_at > BOSS_DEATH) {
		game->state = STATE_WON;
		game->state_at = now;
		game->final_score = game->player.score;
		game->final_chain = game->player.best_chain;
		sound_zone(ZONE_UPLINK);
	}
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
	int blink = fmod(now * HUD_BLINK, 1.0) < HUD_BLINK_ON;
	if (game->state != STATE_WON) {
		snprintf(text, sizeof text, "%08ld", player->score);
		font_write(px(HUD_MARGIN), px(HUD_MARGIN - 2), text, px(HUD_SCORE_SIZE),
			   light_scale(LIGHT_HUD, gain));
		font_write(px(HUD_MARGIN), px(HUD_MARGIN + 26), palette_of(game->zone)->name,
			   px(HUD_ZONE_SIZE), light_scale(LIGHT_HUD_DIM, gain));
	}
	// the chain, big, for a moment after a release
	if (player->chain > 1 && now - player->chain_at < CHAIN_SHOW) {
		double fade = 1.0 - (now - player->chain_at) / CHAIN_SHOW;
		snprintf(text, sizeof text, "X%d", player->chain);
		double size = px(HUD_CHAIN_SIZE + HUD_CHAIN_GROW * fade);
		font_write(view_width - px(HUD_MARGIN) - font_width(text, size), px(HUD_MARGIN - 2),
			   text, size, light_scale(light_mix(LIGHT_HUD_DIM, LIGHT_CHAIN, fade), gain));
	}
	// health, eight bars along the bottom, the lost ones left dark
	for (int i = 0; i < HEALTH_MAX && game->state != STATE_WON; i++) {
		double x = px(HUD_MARGIN + i * HUD_HEALTH_STEP);
		double y = view_height - px(HUD_MARGIN);
		struct light lit = light_scale(i < player->health ? LIGHT_HUD : LIGHT_HUD_OFF, gain);
		draw_line_2d(x, y, x + px(HUD_HEALTH_WIDE), y, lit);
		draw_line_2d(x, y + px(2), x + px(HUD_HEALTH_WIDE), y + px(2), lit);
	}
	// the boss bar, only during the fight
	double health = boss_health(&game->boss);
	if (health > 0.0 && game->state != STATE_WON) {
		double wide = px(HUD_BAR_WIDE), y = px(HUD_BAR_Y);
		double x0 = (view_width - wide) / 2.0;
		draw_line_2d(x0, y, x0 + wide, y, light_scale(LIGHT_HUD_OFF, gain));
		draw_line_2d(x0, y - px(1), x0 + wide * health, y - px(1), light_scale(LIGHT_BOSS_OPEN, gain));
		draw_line_2d(x0, y + px(1), x0 + wide * health, y + px(1), light_scale(LIGHT_BOSS_OPEN, gain));
		snprintf(text, sizeof text, "CORE  PHASE %d", game->boss.phase + 1);
		write_centered(y + px(6), text, px(HUD_ZONE_SIZE), light_scale(LIGHT_HUD_DIM, gain));
	}
	// the name of the zone, big, when it starts
	double title = 1.0 - (now - game->zone_at) / ZONE_TITLE_TIME;
	if (title > 0.0 && game->state == STATE_PLAY && !game->boss.active) {
		double fade = sin(title * M_PI);
		write_centered(view_height * TITLE_Y, palette_of(game->zone)->name, px(ZONE_TITLE_SIZE),
			       light_scale(LIGHT_HUD, gain * fade));
	}
	// the first seconds say what the hands do
	if (game->state == STATE_PLAY && now < LESSON_TIME) {
		double fade = fmin(1.0, (LESSON_TIME - now) / LESSON_FADE);
		write_centered(px(HUD_LESSON_Y), "ARROWS OR MOUSE MOVE THE SIGHT", px(HUD_ZONE_SIZE),
			       light_scale(LIGHT_HUD_DIM, gain * fade));
		write_centered(px(HUD_LESSON_Y + 16), "HOLD SPACE OR CLICK TO MARK   LET GO TO FIRE",
			       px(HUD_ZONE_SIZE), light_scale(LIGHT_HUD_DIM, gain * fade));
	}
	if (game->state == STATE_DEAD) {
		double y = view_height * DEAD_Y;
		write_centered(y, "SIGNAL LOST", px(BIG_SIZE), light_scale(LIGHT_HURT, gain));
		snprintf(text, sizeof text, "SCORE %08ld", player->score);
		write_centered(y + px(LINE_GAP + 10), text, px(SMALL_SIZE), light_scale(LIGHT_HUD, gain));
		if (now - game->state_at > DEATH_WAIT && blink)
			write_centered(y + px(2 * LINE_GAP + 8), "PRESS ENTER TO TRY THE ZONE AGAIN",
				       px(HUD_ZONE_SIZE), light_scale(LIGHT_HUD, gain));
		if (now - game->state_at > DEATH_WAIT)
			write_centered(y + px(3 * LINE_GAP), "SPACE TO END THE RUN", px(HUD_ZONE_SIZE),
				       light_scale(LIGHT_HUD_DIM, gain));
	}
	if (game->state == STATE_WON) {
		double y = view_height * WON_Y;
		write_centered(y, "SIGNAL RESTORED", px(BIG_SIZE), light_scale(LIGHT_ENDING, gain));
		snprintf(text, sizeof text, "SCORE %08ld", game->final_score);
		write_centered(y + px(LINE_GAP + 16), text, px(MID_SIZE), light_scale(LIGHT_HUD, gain));
		snprintf(text, sizeof text, "BEST CHAIN X%d", game->final_chain);
		write_centered(y + px(2 * LINE_GAP + 16), text, px(SMALL_SIZE), light_scale(LIGHT_CHAIN, gain));
		if (blink)
			write_centered(y + px(3 * LINE_GAP + 22), "PRESS ENTER", px(HUD_ZONE_SIZE),
				       light_scale(LIGHT_HUD, gain));
	}
}

static void draw_frame(struct game *game, double now)
{
	// the end is drawn in the calm colours of the start
	int calm = game->state == STATE_WON;
	const struct palette *pal = palette_of(calm ? ZONE_UPLINK : game->zone);
	draw_clear(pal->sky_top, pal->sky_bottom);
	draw_fog(pal->fog);
	draw_pulse(exp(-sound_beat_phase(now) * BEAT_DECAY));
	if (game->boss.active)
		world_arena(&game->camera, game->boss.centre, now, pal,
			    (double)game->boss.phase / (BOSS_PHASES - 1));
	else
		world_draw(&game->camera, &game->rail, game->t_eye, now, game->zone, pal);
	boss_draw(&game->boss, &game->camera, now, pal);
	things_draw(&game->camera, now);
	particles_draw(&game->camera);
	hero_draw(&game->hero, &game->camera, now);
	if (game->state == STATE_PLAY)
		player_draw(&game->player, &game->camera, now);
	draw_hud(game, now, HUD_GLOW);
	// the wash, red for a hit
	struct light wash = LIGHT_BLACK;
	double hurt = 1.0 - (now - game->flash_at) / HURT_WASH;
	if (hurt > 0.0)
		wash = light_scale(LIGHT_HURT, hurt * HURT_WASH_GAIN);
	if (game->boss.dead && game->state == STATE_PLAY) {
		double fade = (now - game->boss.dead_at) / BOSS_DEATH;
		wash = light_scale(LIGHT_ENDING, fade * fade * WON_WASH);
	}
	if (game->state == STATE_WON) {
		double fade = 1.0 - (now - game->state_at) / WON_FADE;
		if (fade > 0.0)
			wash = light_scale(LIGHT_ENDING, fade * WON_WASH);
	}
	draw_finish(wash);
	draw_hud(game, now, 1.0);
}

int main(void)
{
	static struct game the_game;
	struct game *game = &the_game;
	level_build_rail(&game->rail);
	pool_open();
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
		int enter = keys.enter;
		keys.enter = 0;
		switch (game->state) {
		case STATE_PLAY:
			step_play(game, &keys, elapsed, now);
			break;
		case STATE_DEAD:
			step_frozen(game, elapsed, now);
			// a fire held since before the death does not end the run, it
			// has to be let go and pressed again
			if (!keys.fire)
				game->fire_let_go = 1;
			if (enter && now - game->state_at > DEATH_WAIT)
				begin_run(game, level_zone_time(game->zone), game->level.zone_score);
			else if (keys.fire && game->fire_let_go && now - game->state_at > DEATH_WAIT) {
				keys.fire = 0;
				end_run(game);
			}
			break;
		case STATE_WON:
			step_frozen(game, elapsed, now);
			if (enter || now - game->state_at > WON_IDLE)
				end_run(game);
			break;
		}
		set_layers(game, now);
		draw_frame(game, now);
		screen_present(&screen);
		if (trace && moment - traced_at > 0.5) {
			traced_at = moment;
			fprintf(stderr, "t %.1f zone %d eye %.2f things %d score %ld health %d state %d boss %d/%d\n",
				now, game->zone, game->t_eye, things_count(), game->player.score, game->player.health,
				game->state, game->boss.active, game->boss.phase);
		}
		double spent = now_in_seconds() - moment;
		if (spent < FRAME_SECONDS)
			usleep((useconds_t)((FRAME_SECONDS - spent) * 1e6));
	}
	sound_close();
	screen_close(&screen);
	pool_close();
	return 0;
}
