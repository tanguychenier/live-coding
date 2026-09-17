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
#include "scores.h"
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
// the title screen. the name of the game stands in the world ahead of the
// eye, this far and this high, this big, and draws itself in this long.
// the eye rolls a little, slowly, so that the tunnel is seen to live
#define TITLE_TEXT       "AXON"
#define TITLE_DEPTH      7.0
#define TITLE_UP         1.1
#define TITLE_SIZE       0.3
#define TITLE_DRAW       2.2
#define TITLE_PULSE      0.4
#define TITLE_ROLL       0.05
#define TITLE_ROLL_RATE  0.25
#define TITLE_PRESS_Y    0.47     // the press enter line, in the dark of the tunnel
// the author's name, at the bottom right of the title, writes itself in
// this long, starting this long after the title appears, then stays still
#define SIGNATURE        "TANGUY CH\xc3\x89NIER"
#define SIGNATURE_SIZE   2.0
#define SIGNATURE_AFTER  1.0
#define SIGNATURE_DRAW   1.2
#define TITLE_PRESS_UP   92.0     // the press any key line, this far above the bottom
// the table of the best runs on the title, at the left, where it starts,
// its lines, and where the pilot floats meanwhile, to the right of it
#define TABLE_X          22.0
#define TABLE_Y          196.0
#define TABLE_STEP       13.0
#define TABLE_HEAD_GAP   2.0
#define TABLE_SIZE       1.8
#define TITLE_PILOT_X    0.72
// the launch. from the press to the drop there are between one and two
// bars, so that the drop lands on a bar. the eye starts ahead of the pilot
// and to his right, this far and this high, and sweeps round behind him,
// wide at first. the signal runs down the tunnel this fast, this far
#define LAUNCH_BARS      2
#define LAUNCH_ANGLE     1.2
#define LAUNCH_DISTANCE  4.6
#define LAUNCH_HEIGHT    1.3
#define LAUNCH_WIDE      0.7
#define LAUNCH_SIGNAL_SPEED 2.5
#define LAUNCH_SIGNAL_REACH 8.0
// the debug rendering writes this many seconds of the mix
#define WAV_SECONDS      20.0
#define WAV_HEADER       44
// where the numbers sit
#define HUD_MARGIN       14.0
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

enum state { STATE_TITLE, STATE_LAUNCH, STATE_PLAY, STATE_DEAD, STATE_WON, STATE_NAME };

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
	int cabinet;             // the game is playing itself
	int fire_let_go;         // on the death screen, fire has been let go since the death
	double cabinet_at;
	long final_score;
	int final_chain;
	double launch_until;     // when the drop comes, on the run clock
	double signal_at;        // when the signal left, for the wave in the tunnel
	int titled;              // the title has been seen, the lesson can follow
	char name[NAME_MAX + 1]; // being typed for the table
	int final_zone;
};

static double ease(double part)
{
	part = part < 0.0 ? 0.0 : part > 1.0 ? 1.0 : part;
	return part * part * (3.0 - 2.0 * part);
}

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
	game->signal_at = -LAUNCH_SIGNAL_REACH;
}

// the title, the eye at the start of the rail and the pilot afloat in front
// of it, the name of the game drawing itself ahead
static void begin_title(struct game *game)
{
	begin_run(game, 0.0, 0);
	game->state = STATE_TITLE;
	game->state_at = sound_now();
	game->cabinet = 0;
}

// where the title stands in the world, its top left corner and its axes
static void title_plane(const struct game *game, struct vec *origin, struct vec *right,
			struct vec *up)
{
	const struct camera *cam = &game->camera;
	double width = font_width(TITLE_TEXT, TITLE_SIZE);
	*right = cam->right;
	*up = cam->up;
	*origin = add(cam->eye, add(scale(cam->forward, TITLE_DEPTH),
		sub(scale(cam->up, TITLE_UP + GLYPH_H * TITLE_SIZE / 2.0),
		    scale(cam->right, width / 2.0))));
}

// start is pressed. the title bursts, the pilot is held where he floats,
// the eye lets go of him and the riser climbs to the drop
static void begin_launch(struct game *game, double now)
{
	struct vec origin, right, up;
	title_plane(game, &origin, &right, &up);
	font_burst_3d(origin, right, up, TITLE_TEXT, TITLE_SIZE, LIGHT_HUD);
	struct vec forward, rail_right, rail_up;
	rail_frame(&game->rail, 0.0, &forward, &rail_right, &rail_up);
	hero_hold(&game->hero, game->hero.at, forward, rail_right, rail_up);
	game->state = STATE_LAUNCH;
	game->state_at = now;
	game->launch_until = (floor(now / BAR) + LAUNCH_BARS) * BAR;
	game->signal_at = now;
	sound_hit(HIT_KILL, LOCKS_MAX / 2, sound_next_step(now));
	sound_hit(HIT_RISE, 0, game->launch_until - RISE_BARS * BAR);
	game->titled = 1;
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
static const double MIX_TITLE[LAYER_COUNT] = { 0.0, 0.0, 0.0, 0.8, 0.25, 0.0 };

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
	if (game->state == STATE_DEAD || (game->state == STATE_NAME && !game->boss.dead))
		for (int i = 0; i < LAYER_COUNT; i++)
			level[i] = MIX_DEAD[i];
	// the title waits on the pad alone, the launch holds its breath
	if (game->state == STATE_TITLE || game->state == STATE_LAUNCH)
		for (int i = 0; i < LAYER_COUNT; i++)
			level[i] = game->state == STATE_TITLE ? MIX_TITLE[i] : 0.0;
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

// the run is over, won or given up. if it ranks, the name is asked for,
// and the table keeps it. otherwise straight back to the title
static void end_run(struct game *game, double now)
{
	game->final_score = game->player.score;
	game->final_chain = game->player.best_chain;
	game->final_zone = game->boss.dead ? ZONES : game->zone;
	if (scores_rank(game->final_score) < 0) {
		begin_title(game);
		return;
	}
	game->state = STATE_NAME;
	game->state_at = now;
	game->name[0] = 0;
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

// a letter typed or erased on the name screen, and enter to keep it
static void type_name(struct game *game, struct keys *keys)
{
	size_t length = strlen(game->name);
	if (keys->typed && length < NAME_MAX) {
		game->name[length] = keys->typed;
		game->name[length + 1] = 0;
	}
	if (keys->erase && length > 0)
		game->name[length - 1] = 0;
	keys->typed = 0;
	keys->erase = 0;
}

// the title. the eye stands at the start of the rail and rolls a little
static void step_title(struct game *game, double elapsed, double now)
{
	struct vec forward, right, up;
	rail_frame(&game->rail, 0.0, &forward, &right, &up);
	struct vec eye = sub(rail_at(&game->rail, 0.0), scale(up, EYE_DROP));
	struct vec at = rail_at(&game->rail, LOOK_AHEAD);
	double roll = TITLE_ROLL * sin(now * TITLE_ROLL_RATE);
	camera_look(&game->camera, eye, at, vec(0, 1, 0), roll, FOCAL);
	// the pilot floats to the right, the table of the best runs is on the left
	hero_update(&game->hero, &game->camera, view_width * TITLE_PILOT_X, view_height / 2.0, 0, 0,
		    elapsed, now);
	particles_update(elapsed);
}

// the launch. the pilot floats where the title left him, the eye sweeps
// from ahead of him round to behind him, wide at first, and settles into
// the game's own view exactly as the drop comes
static void step_launch(struct game *game, double elapsed, double now)
{
	double length = game->launch_until - game->state_at;
	double part = ease((now - game->state_at) / length);
	struct vec forward, right, up;
	rail_frame(&game->rail, 0.0, &forward, &right, &up);
	struct vec eye_run = sub(rail_at(&game->rail, 0.0), scale(up, EYE_DROP));
	struct vec at_run = rail_at(&game->rail, LOOK_AHEAD);
	struct vec pilot = game->hero.at;
	// round the pilot, from his front right to straight behind
	double angle = LAUNCH_ANGLE + (-M_PI / 2.0 - LAUNCH_ANGLE) * part;
	double distance = LAUNCH_DISTANCE + (HERO_AHEAD - LAUNCH_DISTANCE) * part;
	double height = LAUNCH_HEIGHT + (HERO_BELOW - LAUNCH_HEIGHT) * part;
	struct vec around = add(scale(right, distance * cos(angle)), scale(forward, distance * sin(angle)));
	struct vec eye = add(pilot, add(around, scale(up, height)));
	// the last of the sweep lands exactly on the game's eye
	eye = mix(eye, eye_run, part * part);
	struct vec at = mix(pilot, at_run, part);
	double focal = FOCAL * (LAUNCH_WIDE + (1.0 - LAUNCH_WIDE) * part);
	camera_look(&game->camera, eye, at, vec(0, 1, 0), 0.0, focal);
	hero_update(&game->hero, &game->camera, view_width / 2.0, view_height / 2.0, 0, 0,
		    elapsed, now);
	particles_update(elapsed);
	if (now >= game->launch_until) {
		double since_signal = now - game->signal_at;
		begin_run(game, 0.0, 0);
		game->signal_at = sound_now() - since_signal;
	}
}

// twenty seconds of the mix, the game playing itself, written as a wav file
// with no card and no window. the clock is pulled frame by frame
static int render_wav(struct game *game, const char *path, double start_at)
{
	FILE *f = fopen(path, "wb");
	if (!f) {
		fprintf(stderr, "cannot write %s\n", path);
		return 1;
	}
	int frames_per_step = (int)(SOUND_RATE * FRAME_SECONDS);
	int total = (int)(WAV_SECONDS * SOUND_RATE);
	short *mix = calloc((size_t)total * 2, sizeof *mix);
	if (!mix)
		return 1;
	struct keys keys = { 0 };
	game->cabinet = 1;
	int done = 0;
	while (done < total) {
		double now = sound_now();
		demo_drive(&game->player, &game->camera, &keys, now, game->zone, &game->boss);
		if (game->state == STATE_PLAY)
			step_play(game, &keys, FRAME_SECONDS, now);
		else
			begin_run(game, start_at, 0);
		set_layers(game, now);
		int frames = frames_per_step;
		if (done + frames > total)
			frames = total - done;
		sound_render(mix + (size_t)done * 2, frames);
		done += frames;
	}
	// the header of a wav, forty four bytes of sizes and formats
	unsigned int data_bytes = (unsigned int)total * 4;
	unsigned int rate = SOUND_RATE, byte_rate = SOUND_RATE * 4;
	unsigned short channels = 2, align = 4, bits = 16, pcm = 1;
	unsigned int riff_size = WAV_HEADER - 8 + data_bytes, fmt_size = 16;
	fwrite("RIFF", 1, 4, f); fwrite(&riff_size, 4, 1, f); fwrite("WAVE", 1, 4, f);
	fwrite("fmt ", 1, 4, f); fwrite(&fmt_size, 4, 1, f); fwrite(&pcm, 2, 1, f);
	fwrite(&channels, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&byte_rate, 4, 1, f);
	fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
	fwrite("data", 1, 4, f); fwrite(&data_bytes, 4, 1, f);
	fwrite(mix, 4, (size_t)total, f);
	fclose(f);
	free(mix);
	return 0;
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

// the table of the best runs, on the title, one line each, the place, the
// name, the points, the zone reached and the best chain
static void draw_table(double gain)
{
	if (scores_count() == 0)
		return;
	double x = px(TABLE_X), y = px(TABLE_Y);
	font_write(x, y, "BEST SIGNALS", px(HUD_ZONE_SIZE), light_scale(LIGHT_HUD_DIM, gain));
	y += px(TABLE_STEP) + px(TABLE_HEAD_GAP);
	for (int i = 0; i < scores_count(); i++) {
		const struct score *score = scores_at(i);
		char line[SCORES_LINE];
		snprintf(line, sizeof line, "%2d %-8s %08ld %-9s X%d", i + 1, score->name,
			 score->points, score->zone >= ZONES ? "CLEAR" : palette_of(score->zone)->name,
			 score->chain);
		font_write(x, y, line, px(TABLE_SIZE), light_scale(i == 0 ? LIGHT_CHAIN : LIGHT_HUD, gain));
		y += px(TABLE_STEP);
	}
}

// the numbers along the top, the health along the bottom. drawn twice, once
// in the light so that they glow, once sharp so that they read
static void draw_hud(const struct game *game, double now, double gain)
{
	const struct player *player = &game->player;
	char text[SCORES_LINE];
	int blink = fmod(now * HUD_BLINK, 1.0) < HUD_BLINK_ON;
	if (game->state == STATE_TITLE) {
		if (blink && !game->cabinet)
			write_centered(view_height * TITLE_PRESS_Y, "PRESS ENTER TO START",
				       px(HUD_ZONE_SIZE), light_scale(LIGHT_HUD, gain));
		draw_table(gain);
		double drawn = (now - game->state_at - SIGNATURE_AFTER) / SIGNATURE_DRAW;
		if (drawn > 0.0)
			font_write_drawn(view_width - px(HUD_MARGIN) - font_width(SIGNATURE, px(SIGNATURE_SIZE)),
					 view_height - px(HUD_MARGIN + SIGNATURE_SIZE * GLYPH_H), SIGNATURE,
					 px(SIGNATURE_SIZE), light_scale(LIGHT_HUD_DIM, gain), drawn);
		return;
	}
	if (game->state == STATE_LAUNCH)
		return;
	if (game->state == STATE_NAME) {
		double y = view_height * WON_Y;
		write_centered(y, game->boss.dead ? "SIGNAL RESTORED" : "SIGNAL LOST", px(BIG_SIZE),
			       light_scale(game->boss.dead ? LIGHT_ENDING : LIGHT_HURT, gain));
		snprintf(text, sizeof text, "SCORE %08ld", game->final_score);
		write_centered(y + px(LINE_GAP + 16), text, px(MID_SIZE), light_scale(LIGHT_HUD, gain));
		write_centered(y + px(2 * LINE_GAP + 16), "YOUR NAME", px(HUD_ZONE_SIZE),
			       light_scale(LIGHT_HUD_DIM, gain));
		// the name so far, and a mark where the next letter goes
		snprintf(text, sizeof text, "%s%s", game->name, blink ? "-" : " ");
		write_centered(y + px(3 * LINE_GAP + 10), text, px(MID_SIZE), light_scale(LIGHT_CHAIN, gain));
		write_centered(y + px(4 * LINE_GAP + 16), "TYPE IT AND PRESS ENTER", px(HUD_ZONE_SIZE),
			       light_scale(LIGHT_HUD_DIM, gain));
		return;
	}
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
	if (game->state == STATE_PLAY && now < LESSON_TIME && !game->cabinet && game->titled) {
		double fade = fmin(1.0, (LESSON_TIME - now) / LESSON_FADE);
		write_centered(px(HUD_LESSON_Y), "ARROWS OR MOUSE MOVE THE SIGHT", px(HUD_ZONE_SIZE),
			       light_scale(LIGHT_HUD_DIM, gain * fade));
		write_centered(px(HUD_LESSON_Y + 16), "HOLD SPACE OR CLICK TO MARK   LET GO TO FIRE",
			       px(HUD_ZONE_SIZE), light_scale(LIGHT_HUD_DIM, gain * fade));
	}
	if (game->cabinet && blink)
		write_centered(view_height - px(TITLE_PRESS_UP), "PRESS ANY KEY TO PLAY", px(HUD_ZONE_SIZE),
			       light_scale(LIGHT_HUD, gain));
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
	int calm = game->state == STATE_WON || (game->state == STATE_NAME && game->boss.dead);
	const struct palette *pal = palette_of(calm ? ZONE_UPLINK : game->zone);
	draw_clear(pal->sky_top, pal->sky_bottom);
	draw_fog(pal->fog);
	draw_pulse(exp(-sound_beat_phase(now) * BEAT_DECAY));
	// the signal, a wave of white down the tunnel from the launch
	double front = (now - game->signal_at) * LAUNCH_SIGNAL_SPEED;
	tunnel_signal(front < LAUNCH_SIGNAL_REACH ? front : -1.0);
	if (game->boss.active)
		world_arena(&game->camera, game->boss.centre, now, pal,
			    (double)game->boss.phase / (BOSS_PHASES - 1));
	else
		world_draw(&game->camera, &game->rail, game->t_eye, now, game->zone, pal);
	if (game->state == STATE_TITLE) {
		// the name draws itself, and breathes on the beat once drawn
		struct vec origin, right, up;
		title_plane(game, &origin, &right, &up);
		double drawn = (now - game->state_at) / TITLE_DRAW;
		double pulse = drawn >= 1.0 ? TITLE_PULSE * exp(-sound_beat_phase(now) * BEAT_DECAY) : 0.0;
		font_write_3d(&game->camera, origin, right, up, TITLE_TEXT, TITLE_SIZE,
			      light_scale(LIGHT_HUD, 1.0 + pulse), drawn);
	}
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
	scores_load();
	// the picture exists before the window, the pilot of the bench and the
	// wav rendering aim through it
	draw_resize(VIEW_BASE_WIDTH, VIEW_BASE_HEIGHT);
	sound_open();
	// a run can start anywhere in the level, for a look at a zone
	const char *start = getenv("TEC_START");
	double start_at = start ? floor(atof(start) / BAR) * BAR : 0.0;
	begin_run(game, start_at, 0);
	const char *wav = getenv("TEC_WAV");
	if (wav) {
		int rc = render_wav(game, wav, start_at);
		sound_close();
		return rc;
	}
	struct screen screen;
	if (!screen_open(&screen, GAME_NAME))
		return 1;
	// the title comes first, unless the bench asked for a place in the level
	if (!start)
		begin_title(game);
	struct keys keys = { 0 };
	double last = now_in_seconds();
	double last_key = sound_now();
	int hand_seen = 0;
	const char *trace = getenv("TEC_TRACE");
	double traced_at = 0.0;
	// on the test bench the pilot can play as if it were a hand, or nobody
	// plays and the cabinet never comes, to look at a screen
	int pilot = getenv("TEC_PILOT") ? atoi(getenv("TEC_PILOT")) : 0;
	if (pilot)
		hand_seen = 1;
	if (pilot == 1)
		demo_hand();

	while (!keys.quit) {
		double moment = now_in_seconds();
		double elapsed = moment - last;
		if (elapsed > FRAME_CAP)
			elapsed = FRAME_CAP;
		last = moment;
		double now = sound_now();
		screen_read_keys(&screen, &keys);
		if (keys.any) {
			keys.any = 0;
			last_key = now;
			hand_seen = 1;
			// the hand is back, the cabinet lets go and the game launches
			// for the player
			if (game->cabinet) {
				game->cabinet = 0;
				keys.fire = keys.left = keys.right = keys.up = keys.down = 0;
				begin_title(game);
				now = sound_now();
				begin_launch(game, now);
			}
		}
		if (keys.mute) {
			keys.mute = 0;
			sound_mute(!sound_muted());
		}
		// if nobody touches the keys at the title, the level plays itself
		// for a minute, launch and all, then comes back to the title
		int idle = !hand_seen && !game->cabinet && now - last_key > DEMO_AFTER;
		if (idle && game->state == STATE_TITLE) {
			game->cabinet = 1;
			game->cabinet_at = now;
			begin_launch(game, now);
		}
		if (game->cabinet) {
			if (now - game->cabinet_at > DEMO_LENGTH || game->state == STATE_DEAD
			    || game->state == STATE_WON || game->state == STATE_NAME) {
				begin_title(game);
				now = sound_now();
				last_key = now;
			} else if (game->state == STATE_PLAY) {
				demo_drive(&game->player, &game->camera, &keys, now, game->zone, &game->boss);
			}
		} else if (pilot == 1) {
			// the hand plays the whole run, and the screens between
			switch (game->state) {
			case STATE_TITLE: demo_title(&keys, now - game->state_at); break;
			case STATE_PLAY:
				demo_drive(&game->player, &game->camera, &keys, now, game->zone, &game->boss);
				break;
			case STATE_DEAD: demo_dead(&keys, now - game->state_at, game->boss.active); break;
			case STATE_WON: demo_won(&keys, now - game->state_at); break;
			case STATE_NAME: demo_name(&keys, now - game->state_at); break;
			default: break;
			}
		}
		int enter = keys.enter;
		keys.enter = 0;
		switch (game->state) {
		case STATE_TITLE:
			step_title(game, elapsed, now);
			if (enter || keys.fire || keys.mouse_down) {
				keys.fire = 0;
				begin_launch(game, now);
			}
			break;
		case STATE_LAUNCH:
			step_launch(game, elapsed, now);
			break;
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
				end_run(game, now);
			}
			break;
		case STATE_WON:
			step_frozen(game, elapsed, now);
			if (enter || now - game->state_at > WON_IDLE)
				end_run(game, now);
			break;
		case STATE_NAME:
			step_frozen(game, elapsed, now);
			type_name(game, &keys);
			if (enter) {
				scores_add(game->name, game->final_score, game->final_zone, game->final_chain);
				scores_save();
				begin_title(game);
				// the cabinet may come back, unless the pilot has the keys
				hand_seen = pilot != 0;
				last_key = sound_now();
			}
			break;
		}
		keys.typed = 0;
		keys.erase = 0;
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
