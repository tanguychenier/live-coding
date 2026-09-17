// a shooter on a rail, drawn in lines of light. the eye rides the rail, the
// world breathes on the beat, things come at you, you mark up to eight of
// them and let go, and every shot lands on a sixteenth and plays a note

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "draw.h"
#include "hero.h"
#include "level.h"
#include "palette.h"
#include "particle.h"
#include "rail.h"
#include "screen.h"
#include "sound.h"
#include "tunnel.h"

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
// the groove of the tunnel builds up, the bass from this bar, the hats
// from this one at half, then whole from this one
#define UPLINK_BASS_BAR  4.0
#define UPLINK_HAT_BAR   8.0
#define UPLINK_HAT_HALF  0.4
#define UPLINK_HAT_FULL_BAR 32.0

struct game {
	struct rail rail;
	struct hero hero;
	struct camera camera;
	int zone;
	double t_eye;
	double roll;
};

static double now_in_seconds(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

// a run starts at a moment of the level
static void begin_run(struct game *game, double at)
{
	sound_restart(at);
	double now = sound_now();
	hero_reset(&game->hero);
	particles_clear();
	game->zone = level_zone(now);
	game->t_eye = level_t_eye(&game->rail, now);
	game->roll = 0.0;
	sound_zone(game->zone);
}

// the levels of the layers, kick, hat, bass, pad. the tunnel builds the
// groove one part at a time, the bass after four bars and the hats after
// eight, half open, then whole after thirty two
static const double MIX[ZONES][LAYER_COUNT] = {
	{ 1.0, 1.0, 1.0, 0.0 },
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
static void place_camera(struct game *game, double elapsed)
{
	struct vec forward, right, up;
	rail_frame(&game->rail, game->t_eye, &forward, &right, &up);
	struct vec eye = sub(rail_at(&game->rail, game->t_eye), scale(up, EYE_DROP));
	struct vec at = rail_at(&game->rail, game->t_eye + LOOK_AHEAD);
	double lean = dot(sub(at, rail_at(&game->rail, game->t_eye)), right);
	game->roll += (-lean / LOOK_AHEAD * ROLL_GAIN - game->roll) * ROLL_EASE * elapsed;
	camera_look(&game->camera, eye, at, vec(0, 1, 0), game->roll, FOCAL);
}

static void step_play(struct game *game, double elapsed, double now)
{
	game->t_eye = level_t_eye(&game->rail, now);
	place_camera(game, elapsed);
	// the pilot flies straight ahead for now, the sight comes next
	hero_update(&game->hero, &game->camera, view_width / 2.0, view_height / 2.0, 0,
		    elapsed, now);
	particles_update(elapsed);
}

static void draw_frame(struct game *game, double now)
{
	const struct palette *pal = palette_of(game->zone);
	draw_clear(pal->sky_top, pal->sky_bottom);
	draw_fog(pal->fog);
	draw_pulse(exp(-sound_beat_phase(now) * BEAT_DECAY));
	tunnel_draw(&game->camera, &game->rail, game->t_eye, now, pal);
	particles_draw(&game->camera);
	hero_draw(&game->hero, &game->camera, now);
	draw_finish();
}

int main(void)
{
	static struct game the_game;
	struct game *game = &the_game;
	level_build_rail(&game->rail);
	sound_open();
	struct screen screen;
	if (!screen_open(&screen, GAME_NAME))
		return 1;
	// a run can start anywhere in the level, for a look at a moment of it
	const char *start = getenv("TEC_START");
	begin_run(game, start ? floor(atof(start) / BAR) * BAR : 0.0);
	struct keys keys = { 0 };
	double last = now_in_seconds();

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
		step_play(game, elapsed, now);
		set_layers(game, now);
		draw_frame(game, now);
		screen_present(&screen);
		double spent = now_in_seconds() - moment;
		if (spent < FRAME_SECONDS)
			usleep((useconds_t)((FRAME_SECONDS - spent) * 1e6));
	}
	sound_close();
	screen_close(&screen);
	return 0;
}
