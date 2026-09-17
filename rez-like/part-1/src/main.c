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
#include "level.h"
#include "palette.h"
#include "rail.h"
#include "screen.h"
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

struct game {
	struct rail rail;
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
}

static void draw_frame(struct game *game, double now)
{
	const struct palette *pal = palette_of(game->zone);
	draw_clear(pal->sky_top, pal->sky_bottom);
	draw_fog(pal->fog);
	tunnel_draw(&game->camera, &game->rail, game->t_eye, now, pal);
	draw_finish();
}

int main(void)
{
	static struct game the_game;
	struct game *game = &the_game;
	level_build_rail(&game->rail);
	struct screen screen;
	if (!screen_open(&screen, GAME_NAME))
		return 1;
	struct keys keys = { 0 };
	double last = now_in_seconds(), start = last;

	while (!keys.quit) {
		double moment = now_in_seconds();
		double elapsed = moment - last;
		if (elapsed > FRAME_CAP)
			elapsed = FRAME_CAP;
		last = moment;
		double now = moment - start;
		screen_read_keys(&screen, &keys);
		step_play(game, elapsed, now);
		draw_frame(game, now);
		screen_present(&screen);
		double spent = now_in_seconds() - moment;
		if (spent < FRAME_SECONDS)
			usleep((useconds_t)((FRAME_SECONDS - spent) * 1e6));
	}
	screen_close(&screen);
	return 0;
}
