// wolfenstein 3d drew its corridors with one ray per column, in 1992
// let us write that engine in c, from this empty file
// x11 gives us a window and a block of memory, the rest is ours

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "demo.h"
#include "fight.h"
#include "light.h"
#include "render.h"
#include "screen.h"
#include "sound.h"
#include "text.h"
#include "texture.h"
#include "thing.h"
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
	// the level can be swapped without a rebuild: that is already the
	// rule for the sprite sheets, and it holds for the map
	const char *level_file = getenv("TEC_LEVEL");
	if (!level_file)
		level_file = LEVEL_FILE;
	if (!load_level(level_file))
		return 1;

	struct screen screen;
	if (!screen_open(&screen))
		return 1;

	// the textures cost nothing to keep and everything to draw: once, here
	make_wall_textures();
	make_light_table();
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
	sound_open();
	things_clear();
	things_from_level();
	fight_reset();
	demo_load(ROUTE_FILE);
	double died_at = 0.0;
	double won_at = 0.0;
	// when the keys were last touched, and whether the cabinet is playing
	double last_key = last;
	int cabinet = 0;
	// the first two things to learn, and they are learned in the quiet.
	// the swing is taught in the airlock, where there is nothing to hit:
	// the tool comes before the danger, always.
	say("ARROWS TO MOVE    M FOR THE MAP", last);
	double lesson_at = last;
	int lesson = 0;

	while (!keys.quit) {
		double moment = now_in_seconds();
		double elapsed = moment - last;
		last = moment;

		screen_read_keys(&screen, &keys);
		if (keys.any) {
			keys.any = 0;
			last_key = moment;
			// the hand is back: the cabinet lets go of what it held
			if (cabinet)
				keys.forward = keys.back = keys.left = keys.right = 0;
			cabinet = 0;
		}
		// nobody at the keys for a while: the level plays itself
		if (moment - last_key > DEMO_AFTER && fight.health > 0.0 && !won_at) {
			demo_drive(&player, &keys, moment);
			cabinet = 1;
		}
		if (!lesson && moment - lesson_at > 7.0) {
			say("CTRL SWINGS WHAT YOU ARE HOLDING", moment);
			lesson = 1;
		}

		// the key is read once and shared. it opens a door and it
		// restarts after a death: if each side clears it on its own,
		// the last one never sees it. death is checked first,
		// because a dead man pushes nothing.
		int push_key = keys.push;
		keys.push = 0;
		int death_push = fight.health <= 0.0 && push_key;
		if (fight.health <= 0.0)
			push_key = 0;

		// a door that opens by itself teaches nothing; a door that
		// asks for a key and says so does
		if (door_ahead(&player) && fight.health > 0.0)
			say("PRESS SPACE TO OPEN", moment);
		if (push_key) {
			push_door(&player);
			sound_play(SFX_DOOR, 0.0);
		}
		// picked up by walking over it, no stop and no panel
		switch (loot_take(&player)) {
		case LOOT_GUN:
			fight.has_gun = 1;
			fight.ammo += AMMO_PACK;
			say("SIDEARM  IT STILL FEEDS", moment);
			break;
		case LOOT_AMMO:
			fight.ammo += AMMO_PACK;
			say("ROUNDS", moment);
			break;
		case LOOT_MED:
			fight_heal(MED_PACK);
			say("PATCHED UP", moment);
			break;
		case LOOT_BADGE:
			say("SECURITY BADGE TAKEN", moment);
			break;
		default:
			break;
		}
		// 1 and 2 pick what is held. asking for the gun with no
		// rounds does nothing, and the hud already says why.
		if (keys.weapon) {
			if (keys.weapon == 1) {
				fight.chosen = 1;
				say("PIPE", moment);
			} else if (fight.has_gun) {
				fight.chosen = 2;
				say(fight.ammo ? "SIDEARM" : "SIDEARM  EMPTY", moment);
			}
			keys.weapon = 0;
		}
		if (keys.hit) {
			if (fight.health > 0.0)
				fight_strike(&player, moment);
			keys.hit = 0;
		}
		double damage = things_update(&player, elapsed, moment);
		if (!won_at)
			fight_take(damage, moment);
		fight_step(elapsed);
		move_doors(elapsed);
		lamp_flicker(moment);
		remember(&player);

		// dying must not mean starting the program again. a player
		// sent back to a terminal to retry does not retry.
		if (fight.health <= 0.0) {
			if (!died_at)
				died_at = moment;
			say(moment - died_at > 1.5 ? "SPACE TO TRY AGAIN" : "YOU DIED",
				moment);
			if (moment - died_at > 1.5 && death_push) {
				died_at = 0.0;
				load_level(level_file);
				level_marks(&player.x, &player.y, &exit_x, &exit_y,
					    &player.dir_x, &player.dir_y);
				things_clear();
				things_from_level();
				fight_reset();
				demo_reset();
				say("AGAIN", moment);
			}
		}

		// the way out, and it wants the badge
		if ((int)player.x == exit_x && (int)player.y == exit_y) {
			if (!have_badge())
				say("THE HATCH IS LOCKED", moment);
			else if (!won_at)
				won_at = moment;
		}

		// every move is scaled by the time the last frame took, so the game
		// runs at the same speed whatever the machine is doing
		int alive = fight.health > 0.0;
		// the hatch is open: the man is out, and he stops here
		if (won_at)
			alive = 0;
		double forward = alive
			? (keys.forward - keys.back) * WALK_SPEED * elapsed : 0.0;
		double sideways = alive
			? (keys.strafe_right - keys.strafe_left) * WALK_SPEED * elapsed : 0.0;
		double turn = (alive ? (keys.right - keys.left) * TURN_SPEED : 0.0) * elapsed;

		double step_x = player.dir_x * forward + player.plane_x * sideways;
		double step_y = player.dir_y * forward + player.plane_y * sideways;

		move_player(&player, step_x, step_y);
		turn_player(&player, turn);
		fit_view_to_window(&player);
		render_floor_and_ceiling(&player);
		render_walls(&player);
		// the overlay is a share of the view, not a pixel count. at
		// fixed sizes the map ate a third of a small window and was
		// a stamp on a wide one: measure it against the width.
		int hud = view_width / HUD_DIVISOR;
		if (hud < 1)
			hud = 1;
		// after the walls, never before: the depth buffer has to be
		// filled for them to know what hides them
		things_draw(&player, moment);
		fight_draw(&player, moment, keys.forward || keys.back
			|| keys.strafe_left || keys.strafe_right);
		fight_hud(moment);
		if (keys.map)
			render_map(&player, hud * MAP_CELL, hud * MAP_LEFT,
				hud * MAP_TOP);
		if (moment < notice_until)
			draw_text_centered(view_height - hud * NOTICE_UP, notice,
				TEXT_COLOR, hud * TEXT_SCALE);
		// the end: the view goes dark, and the count says it was
		// our game and not a game
		if (won_at) {
			double gone = (moment - won_at) / ENDING_FADE * ENDING_DARK;
			fight_dim(gone > ENDING_DARK ? ENDING_DARK : gone);
			draw_text_centered(view_height / 2 - hud * ENDING_GAP,
				"YOU MADE IT OUT", TEXT_COLOR, hud * TEXT_SCALE);
			char count[32];
			snprintf(count, sizeof count, "%d PUT DOWN", fight.put_down);
			draw_text_centered(view_height / 2 + hud * ENDING_GAP,
				count, TEXT_COLOR, hud * TEXT_SCALE);
		}
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
