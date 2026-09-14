// wolfenstein 3d drew its corridors with one ray per column, in 1992
// let us write that engine in c, from this empty file
// x11 gives us a window and a block of memory, the rest is ours

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <time.h>
#include <unistd.h>

#include "render.h"
#include "light.h"
#include "text.h"
#include "screen.h"
#include "texture.h"
#include "world.h"

// a game says what can be done at the moment it can be done, and holds its
// tongue the rest of the time.
static const char *notice;
static double notice_until;

static void say(const char *line, double moment)
{
	notice = line;
	notice_until = moment + NOTICE_SECONDS;
}

static double now_in_seconds(void)
{
	struct timespec moment;
	clock_gettime(CLOCK_MONOTONIC, &moment);
	return moment.tv_sec + moment.tv_nsec / 1e9;
}

int main(void)
{
	if (!load_level(LEVEL_FILE))
		return 1;

	struct screen screen;
	if (!screen_open(&screen))
		return 1;

	// the textures cost nothing to keep and everything to draw: once, here
	make_wall_textures();
	light_map();
	make_floor_texture();
	make_ceiling_texture();

	// facing east, with the camera plane across the line of sight
	struct player player = {
		.x = 0.0, .y = 0.0,
		.dir_x = 1.0, .dir_y = 0.0,
		.plane_x = 0.0, .plane_y = FIELD_OF_VIEW,
	};
	// the file says where we start and where the way out is
	int exit_x, exit_y;
	level_marks(&player.x, &player.y, &exit_x, &exit_y,
		    &player.dir_x, &player.dir_y);
	struct keys keys = { .map = 1 };
	double last = now_in_seconds();
	// the first thing one reads, and the only one nobody triggers
	say("ARROWS TO MOVE    M FOR THE MAP", last);
	// when the hatch opened: we leave time to read it
	double left_at = 0.0;

	while (!keys.quit) {
		double moment = now_in_seconds();
		double elapsed = moment - last;
		last = moment;

		screen_read_keys(&screen, &keys);

		// a door that opens by itself teaches nothing; a door that
		// asks for a key and says so does
		if (door_ahead(&player))
			say("PRESS SPACE TO OPEN", moment);
		if (keys.push) {
			push_door(&player);
			keys.push = 0;
		}
		if (walk_over(&player))
			say("SECURITY BADGE TAKEN", moment);
		move_doors(elapsed);
		lamp_flicker(moment);
		remember(&player);

		// the way out, and it wants the badge
		if ((int)player.x == exit_x && (int)player.y == exit_y) {
			if (!have_badge())
				say("THE HATCH IS LOCKED", moment);
			else if (left_at == 0.0) {
				left_at = moment;
				say("THE HATCH OPENS", moment);
			}
		}
		if (left_at > 0.0 && moment - left_at > NOTICE_SECONDS)
			keys.quit = 1;

		// every move is scaled by the time the last frame took, so the game
		// runs at the same speed whatever the machine is doing
		double forward = (keys.forward - keys.back) * WALK_SPEED * elapsed;
		double sideways = (keys.strafe_right - keys.strafe_left) * WALK_SPEED * elapsed;
		double turn = (keys.right - keys.left) * TURN_SPEED * elapsed;

		double step_x = player.dir_x * forward + player.plane_x * sideways;
		double step_y = player.dir_y * forward + player.plane_y * sideways;

		move_player(&player, step_x, step_y);
		turn_player(&player, turn);
		fit_view_to_window(&player);
		render_floor_and_ceiling(&player);
		render_walls(&player);
		if (keys.map)
			render_map(&player, MAP_CELL, MAP_LEFT, MAP_TOP);
		if (moment < notice_until)
			draw_text_centered(view_height - NOTICE_UP, notice,
				TEXT_COLOR, TEXT_SCALE);
		screen_present(&screen);

		// no need to draw faster than that, and without this the loop
		// eats a whole core to draw the same thing twice
		double spare = 1.0 / FRAMES_PER_SECOND - (now_in_seconds() - moment);
		if (spare > 0)
			usleep((useconds_t)(spare * 1e6));
	}

	// XDestroyImage frees the buffer it was given, so this is the whole
	// clean-up: the picture, then the connection
	XDestroyImage(screen.image);
	XFreeGC(screen.display, screen.gc);
	XCloseDisplay(screen.display);
	return 0;
}
