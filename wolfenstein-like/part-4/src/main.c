// wolfenstein 3d drew its corridors with one ray per column, in 1992
// let us write that engine in c, from this empty file
// x11 gives us a window and a block of memory, the rest is ours

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "render.h"
#include "light.h"
#include "text.h"
#include "screen.h"
#include "texture.h"
#include "world.h"
#include "sound.h"
#include "story.h"
#include "thing.h"
#include "demo.h"
#include "fight.h"
#include "trigger.h"

// a game says what can be done at the moment it can be done, and holds its
// tongue the rest of the time.
static const char *notice;
static double notice_until;

static void say(const char *line, double moment)
{
	notice = line;
	notice_until = moment + NOTICE_SECONDS;
}

// draws the boss bar at the top, and only during the fight. before the fight
// it would teach nothing, and after it there is nothing left to say.
static void draw_boss_bar(int hud)
{
	double part = boss_health();
	if (part < 0.0)
		return;
	int wide = view_width * 52 / 100, tall = 6 * hud;
	int x0 = (view_width - wide) / 2, y0 = 4 * hud;
	// a bar that looks empty has to mean dead, so as long as a single point
	// of health is left, we keep a visible segment.
	int filled = (int)(wide * part);
	if (part > 0.0 && filled < 6 * hud)
		filled = 6 * hud;
	for (int y = 0; y < tall; y++)
		for (int x = 0; x < wide; x++) {
			int px = x0 + x, py = y0 + y;
			if (px < 0 || py < 0 || px >= view_width || py >= view_height)
				continue;
			int edge = y < hud || y >= tall - hud
				|| x < hud || x >= wide - hud;
			view[py * view_width + px] = edge ? 0x2a2026
				: (x < filled ? 0xa8342c : 0x14090a);
		}
	draw_text_centered(y0 + tall + 2 * hud, "THE THING FROM DECK FOUR",
		0x8a5a52, hud);
}

// ------------------------------------------------------------------ the menu
// three lines, and the list of keys. a game without a pause forces one to
// close the window to stop, and it says nowhere how to hit. the menu is the
// place for that, and it is always one key away.
static const char *MENU[] = { "CONTINUE", "FULL SCREEN", "START OVER", "QUIT" };
#define MENU_COUNT ((int)(sizeof MENU / sizeof *MENU))
// the lines by name, so that the choice never counts on the order above
enum { MENU_CONTINUE, MENU_FULL_SCREEN, MENU_START_OVER, MENU_QUIT };

// two columns, the key and then what it does. they have to be aligned,
// otherwise it is not a list, it is a heap.
static const char *KEYS_LIST[][2] = {
	{ "ARROWS  W S", "WALK AND TURN" },
	{ "CLICK  MOUSE", "LOOK   ESC GIVES IT BACK" },
	{ "A  D",        "SIDESTEP" },
	{ "CTRL",        "ATTACK" },
	{ "1  2",        "PIPE   SIDEARM" },
	{ "SPACE",       "OPEN A DOOR" },
	{ "M",           "MAP" },
	{ "N",           "SOUND" },
	{ "F11",         "FULL SCREEN" },
	{ "ESC",         "THIS MENU" },
};
#define TOUCHES_COUNT ((int)(sizeof KEYS_LIST / sizeof *KEYS_LIST))

// draws a filled rectangle, for the background of the panel and for its edges
static void panel(int x0, int y0, int l, int h, unsigned int color, double part)
{
	for (int y = y0; y < y0 + h; y++)
		for (int x = x0; x < x0 + l; x++) {
			if (x < 0 || y < 0 || x >= view_width || y >= view_height)
				continue;
			unsigned int c = view[y * view_width + x];
			int r = (int)(((c >> 16) & 0xff) * (1 - part)
				+ ((color >> 16) & 0xff) * part);
			int g = (int)(((c >> 8) & 0xff) * (1 - part)
				+ ((color >> 8) & 0xff) * part);
			int b = (int)((c & 0xff) * (1 - part) + (color & 0xff) * part);
			view[y * view_width + x] =
				(unsigned int)((r << 16) | (g << 8) | b);
		}
}

// we draw a real panel, not lines thrown in the middle of the screen. heights
// are counted in text height rather than in interface units, so a line takes
// the height of its font times its scale, plus some air, and nothing else.
static void draw_menu(int choice, int u)
{
	for (int i = 0; i < view_width * view_height; i++) {
		unsigned int c = view[i];
		view[i] = ((((c >> 16) & 0xff) * 2 / 7) << 16)
			| ((((c >> 8) & 0xff) * 2 / 7) << 8) | ((c & 0xff) * 2 / 7);
	}
	int st = 4 * u, se = 3 * u, sk = 2 * u;
	int ht = GLYPH_H * st, he = GLYPH_H * se, hk = GLYPH_H * sk;
	int air = 5 * u, margin = 9 * u;
	int total = ht + air * 3 + u + air * 2 + MENU_COUNT * (he + air)
		+ air * 2 + u + air * 2 + hk + air
		+ TOUCHES_COUNT * (hk + air / 2 + u);
	int ph = total + margin * 2;
	int pl = view_width * 88 / 100;
	int px = (view_width - pl) / 2, py = (view_height - ph) / 2;
	if (py < 0)
		py = 0;
	panel(px, py, pl, ph, 0x0a0e14, 0.90);
	panel(px, py, pl, u, 0x3f4c5a, 1.0);
	panel(px, py + ph - u, pl, u, 0x3f4c5a, 1.0);
	panel(px, py, u, ph, 0x3f4c5a, 1.0);
	panel(px + pl - u, py, u, ph, 0x3f4c5a, 1.0);

	int y = py + margin;
	draw_text_centered(y, "THE KEEP", 0x9fe8d8, st);
	y += ht + air * 2;
	panel(px + margin, y, pl - margin * 2, u, 0x27313c, 1.0);
	y += air * 2;

	int left_x = px + margin + 4 * u;
	for (int i = 0; i < MENU_COUNT; i++) {
		// the cursor is a block rather than a character, because the font
		// only has letters and digits and there is no chevron in it.
		if (i == choice)
			panel(left_x, y + he / 4, se * 2, he / 2, 0xf0c060, 1.0);
		draw_text(left_x + (GLYPH_W + 1) * se * 2, y, MENU[i],
			i == choice ? 0xf0e4c8 : 0x5a6674, se);
		y += he + air;
	}
	y += air;
	panel(px + margin, y, pl - margin * 2, u, 0x27313c, 1.0);
	y += air * 2;
	draw_text(left_x, y, "CONTROLS", 0x4e5c6a, sk);
	y += hk + air;
	// the right column starts right after the longest key name
	int column_x = left_x + (GLYPH_W + 1) * sk * 13;
	for (int i = 0; i < TOUCHES_COUNT; i++) {
		draw_text(left_x, y, KEYS_LIST[i][0], 0x8fa2b4, sk);
		draw_text(column_x, y, KEYS_LIST[i][1], 0x5f6d7c, sk);
		y += hk + air / 2 + u;
	}
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
	// the station is falling apart while he wakes up, and none of it stops
	// him from walking. it all happens around him.
	story_begin(last);
	things_clear();
	fight_reset();
	things_from_level();
	triggers_reset();
	demo_load(ROUTE_FILE);
	int doors_opened = 0;
	double died_at = 0.0, over_at = 0.0;
	// when the keys were last touched, whether anyone has ever touched them,
	// and whether the cabinet is playing right now
	double last_key = last;
	int hand_seen = 0, cabinet = 0;
	// the scene behind the jammed door, where it is in its beats and since
	// when
	double breach_at = 0.0;
	int breach_step = 0;
	int menu_on = 0, menu_choice = 0, up_before = 0, down_before = 0;
	int restart = 0;
	// these are the first three things the player learns, and he learns them
	// in the quiet. the swing is taught in the airlock, where there is
	// nothing to hit, because the tool always comes before the danger.
	say("ARROWS TO MOVE    CLICK TO USE THE MOUSE", last);
	double lesson_at = last;
	int lesson = 0;
	int said_window = 0;

	// a trace that we only use to tune the test runs, never on air
	const char *trace = getenv("TEC_TRACE");
	double said_at = 0.0;

	while (!keys.quit) {
		double moment = now_in_seconds();
		double elapsed = moment - last;
		last = moment;

		screen_read_keys(&screen, &keys);
		if (keys.any) {
			keys.any = 0;
			last_key = moment;
			hand_seen = 1;
			// the hand is back: the cabinet lets go of what it held
			if (cabinet)
				keys.forward = keys.back = keys.left = keys.right = 0;
			cabinet = 0;
		}
		// if nobody has touched the keys since the start, the level plays
		// itself. once someone has played, it never takes the hand again,
		// because a cabinet walks off on its own from the start screen, not
		// in the middle of somebody's game.
		if (!hand_seen && moment - last_key > DEMO_AFTER && story_control()
		    && fight.health > 0.0 && !menu_on) {
			demo_drive(&player, &keys, moment);
			cabinet = 1;
		}
		if (keys.menu) {
			keys.menu = 0;
			menu_on = !menu_on;
			menu_choice = 0;
			// the menu gives the mouse back, otherwise you cannot even click
			// somewhere else to leave the game
			if (menu_on)
				screen_release_mouse(&screen);
		}
		// the menu freezes everything. the room is still drawn behind it,
		// darkened, and the game waits, so nothing moves and nothing hits.
		if (menu_on) {
			int go_up = keys.forward && !up_before;
			int go_down = keys.back && !down_before;
			up_before = keys.forward;
			down_before = keys.back;
			if (go_up)
				menu_choice = (menu_choice + MENU_COUNT - 1) % MENU_COUNT;
			if (go_down)
				menu_choice = (menu_choice + 1) % MENU_COUNT;
			if (keys.push || keys.validate) {
				keys.push = keys.validate = 0;
				if (menu_choice == MENU_CONTINUE) {
					menu_on = 0;
				} else if (menu_choice == MENU_FULL_SCREEN) {
					screen_toggle_fullscreen(&screen);
					menu_on = 0;
				} else if (menu_choice == MENU_START_OVER) {
					menu_on = 0;
					fight.health = 0.0;
					died_at = moment - 2.0;
					restart = 1;
				} else {
					keys.quit = 1;
				}
			}
			keys.hit = 0;
			fit_view_to_window(&player);
			render_floor_and_ceiling(&player);
			render_walls(&player);
			things_draw(&player, moment);
			int hudm = view_width / HUD_DIVISOR;
			draw_menu(menu_choice, hudm < 1 ? 1 : hudm);
			screen_present(&screen);
			usleep(16000);
			continue;
		}
		up_before = keys.forward;
		down_before = keys.back;

		// a door that opens by itself teaches nothing; a door that
		// asks for a key and says so does
		if (door_ahead(&player) && story_control())
			say("PRESS SPACE TO OPEN", moment);
		// the space key is read once and then shared, because it skips the
		// film, pushes a door and restarts after a death. if each of them
		// cleared it on its own, the last one would never see it. we also
		// handle death before the door, because a dead man pushes nothing.
		int push_key = keys.push;
		keys.push = 0;
		int death_push = fight.health <= 0.0 && push_key;
		if (fight.health <= 0.0)
			push_key = 0;
		if (push_key) {
			// during the film, space only skips the film and does nothing
			// else
			enum push which = story_control()
				? push_door(&player) : PUSH_NOTHING;
			if (!story_control())
				story_skip(moment);
			if (trace)
				fprintf(stderr, "PUSH %d seals %d\n",
					(int)which, world_seals_released());
			switch (which) {
			case PUSH_OPENS:
				doors_opened++;
				sound_play(SFX_DOOR, 0.0);
				break;
			// the jammed door is the pivot of the whole level. it opens a
			// third of the way, it sticks, and through the gap you see what
			// you will not reach. that same jam is what releases the seals,
			// so its failure is what opens the game.
			case PUSH_JAMMED:
				sound_play(SFX_DOOR, 0.0);
				say("JAMMED  IT IS NOT GOING TO OPEN", moment);
				breach_at = moment;
				breach_step = 0;
				break;
			case PUSH_SEALED:
				say("NO POWER IN THIS ONE", moment);
				break;
			case PUSH_LOCKED:
				sound_play(SFX_IMPACT, 4.0);
				say("VAULT  THE BADGE OPENS THIS", moment);
				break;
			default:
				break;
			}
		}
		// picked up by walking over it, no stop and no panel
		switch (loot_take(&player)) {
		case LOOT_GUN:
			fight.has_gun = 1;
			fight.ammo += AMMO_PACK;
			say("SIDEARM  IT STILL FEEDS", moment);
			sound_play(SFX_PICKUP, 0.0);
			break;
		case LOOT_AMMO:
			fight.ammo += AMMO_PACK;
			say("ROUNDS", moment);
			sound_play(SFX_PICKUP, 0.0);
			break;
		case LOOT_MED:
			fight_heal(MED_PACK);
			say("PATCHED UP", moment);
			sound_play(SFX_PICKUP, 0.0);
			break;
		case LOOT_BADGE:
			say("BADGE  THE VAULT WILL OPEN NOW", moment);
			sound_play(SFX_PICKUP, 0.0);
			break;
		default:
			break;
		}
		if (lesson < 2 && story_control() && moment - lesson_at > 7.0) {
			say(lesson ? "ESC FOR THE MENU AND THE KEYS"
				  : "CTRL SWINGS WHAT YOU ARE HOLDING", moment);
			lesson_at = moment;
			lesson++;
		}

		// the pattern is taught once and only once. the boss charges, misses
		// and rests, and it is while it rests that it really gets hurt. a
		// player who does not find that out loses without knowing why, so we
		// say it in one line at the first rest and never again.
		if (!said_window && boss_resting()) {
			said_window = 1;
			say("IT IS WIDE OPEN  HIT IT NOW", moment);
		}

		// the scene behind the jammed door plays in eight beats. the door is
		// forced, it sticks, and through the gap you see a fight at the far
		// end. there are shots, then a cry, then it stops, and what blows up
		// down there cuts the door controls of the whole deck.
		if (breach_at > 0.0) {
			double t = moment - breach_at;
			// the battle, beat by beat. two men hold the far end of the deck
			// and fire, one beast falls, then the men fall one after the
			// other, and the beasts leave by the far end. none of it comes
			// towards us, because the gap is only a window.
			static const struct { double when; int which; } scene[] = {
				{ 0.4, 0 }, { 0.8, 1 }, { 1.2, 0 }, { 1.6, 1 },
				{ 2.0, 2 }, { 2.4, 3 }, { 2.8, 0 }, { 3.2, 1 },
				{ 3.6, 0 }, { 4.2, 4 }, { 4.7, 1 }, { 5.2, 1 },
				{ 5.9, 5 }, { 6.6, 6 }, { 8.2, 7 }, { 11.0, 8 },
			};
			int amount = (int)(sizeof scene / sizeof *scene);
			while (breach_step < amount && t > scene[breach_step].when) {
				switch (scene[breach_step].which) {
				case 0:
					crew_fire(0, moment);
					things_roam_graze(moment);
					break;
				case 1:
					crew_fire(1, moment);
					things_roam_graze(moment);
					break;
				case 2:
					// this shot lands, and a beast falls
					things_roam_kill();
					sound_play(SFX_DIE, 9.0);
					break;
				case 3:
					// the beasts shriek and close in on the men
					sound_play(SFX_SHRIEK, 9.0);
					things_roam_advance();
					break;
				case 4:
					crew_fall(0);
					sound_play(SFX_HURT, 9.0);
					break;
				case 5:
					crew_fall(1);
					sound_play(SFX_HURT, 9.0);
					break;
				case 6:
					// and the beasts leave by the far end
					things_roam_away();
					sound_play(SFX_GROWL, 9.0);
					break;
				case 7:
					sound_play(SFX_BLAST, 1.5);
					story_hit(moment, 2.4,
						player.x + player.dir_x * 6.0,
						player.y + player.dir_y * 6.0);
					story_blast(moment);
					say("DOOR CONTROL IS GONE", moment);
					world_release_seals();
					// we hear the deck open, three doors at different
					// distances, and the deck becomes another place
					sound_play(SFX_DOOR, 6.0);
					sound_play(SFX_DOOR, 12.0);
					break;
				default:
					say("EVERY SEAL ON THE DECK JUST OPENED", moment);
					breach_at = 0.0;
					break;
				}
				breach_step++;
			}
		}
		const char *scene = triggers_step(&player, moment);
		if (scene)
			say(scene, moment);
		// this is what the survivors have to say. they lie on the floor, you
		// lean over them, they let out a line, then another one, and the
		// third never comes out whole. they are the only crew we will ever
		// see, and that is what makes it real.
		const char *word = crew_speak(&player, moment);
		if (word)
			say(word, moment);
		story_shake_here(player.x, player.y);
		story_tick(moment);
		story_update(moment, doors_opened);
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
		if (keys.mute) {
			sound_mute(!sound_muted());
			say(sound_muted() ? "SOUND OFF" : "SOUND ON", moment);
			keys.mute = 0;
		}
		if (keys.hit) {
			if (story_control())
				fight_strike(&player, moment);
			keys.hit = 0;
		}
		// the magazine has just run dry. we say it once, the bar comes back
		// on its own, and the player knows why.
		if (fight.emptied) {
			fight.emptied = 0;
			say("EMPTY  BACK TO THE PIPE", moment);
		}
		// on the test bench we take every hit without falling, so that the
		// whole level can be walked and photographed
		double damage_taken = things_update(&player, elapsed, moment);
		fight_take(getenv("TEC_ESSAI") ? 0.0 : damage_taken);
		fight_step(elapsed);
		// the music is an event, not a background. nothing plays while
		// nothing hunts us, because silence is the normal state, and that
		// silence is what gives weight to the moment the riff comes in.
		int hunted = 0, awake = 0;
		for (int i = 0; i < THINGS_MAX; i++) {
			if (!things[i].used || things[i].roam)
				continue;
			if (things[i].state == THING_HUNT
			    || things[i].state == THING_STRIKE)
				hunted = 1;
			else if (things[i].state == THING_ALERT)
				awake = 1;
		}
		// three states, and you hear them before you see them. the breath of
		// the station once the deck gives way, a bass when something has got
		// up, and the riff when it comes at us. the player knows what is
		// coming a second before he sees it.
		sound_layer(LAYER_BREATH, world_seals_released() ? 0.30 : 0.0);
		sound_layer(LAYER_PULSE, hunted || awake ? 0.75 : 0.0);
		sound_layer(LAYER_DRIVE, hunted ? 0.85 : 0.0);
		sound_layer(LAYER_METAL, hunted ? 0.70 : 0.0);
		move_doors(elapsed);
		lamp_flicker(moment);
		remember(&player);

		// the game ends when the thing at the bottom falls, not on a cell or
		// on a countdown, because that is what we came here for. the closing
		// film then starts on its own.
		if (boss_down() && over_at == 0.0) {
			over_at = moment;
			story_score(fight.put_down);
			say("IT IS DOWN", moment);
			sound_layer(LAYER_DRIVE, 0.0);
			sound_layer(LAYER_METAL, 0.0);
		}
		if (over_at > 0.0 && moment - over_at > 2.2)
			story_finish(moment);
		if (story_finished())
			keys.quit = 1;
		// dying must not mean starting the program again, because a player
		// sent back to a terminal does not retry. death also has to be seen,
		// so the world freezes, the colour drains, the bars come back, and
		// what to do next is written large in the middle.
		if (fight.health <= 0.0) {
			if (!died_at) {
				died_at = moment;
				story_death(moment);
				sound_layer(LAYER_DRIVE, 0.0);
				sound_layer(LAYER_METAL, 0.0);
				sound_layer(LAYER_PULSE, 0.0);
			}
			if (moment - died_at > 1.3 && death_push)
				restart = 1;
		}
		// the level is rebuilt in place, after a death or from the menu, and
		// both go through the same code. with two ways to start over and two
		// resets, one of them would always forget something.
		if (restart) {
			restart = 0;
			died_at = 0.0;
			load_level(level_file);
			level_marks(&player.x, &player.y, &exit_x, &exit_y,
				    &player.dir_x, &player.dir_y);
			// the eyes turn back to the start, so the camera plane has to
			// follow them. left where it was, it is no longer square to
			// the eyes, and the whole picture shears as soon as we turn
			player.plane_x = -player.dir_y * FIELD_OF_VIEW;
			player.plane_y = player.dir_x * FIELD_OF_VIEW;
			things_clear();
			things_from_level();
			triggers_reset();
			fight_reset();
			story_restart(moment);
			demo_reset();
			doors_opened = 0;
			over_at = breach_at = 0.0;
			breach_step = 0;
			say("AGAIN", moment);
		}

		// every move is scaled by the time the last frame took, so the game
		// runs at the same speed whatever the machine is doing
		int alive = story_control() && fight.health > 0.0;
		double forward = alive
			? (keys.forward - keys.back) * WALK_SPEED * elapsed : 0.0;
		double sideways = alive
			? (keys.strafe_right - keys.strafe_left) * WALK_SPEED * elapsed : 0.0;
		double turn = (alive ? (keys.right - keys.left) * TURN_SPEED : 0.0)
			* elapsed + story_drift() * elapsed;
		if (alive)
			turn += keys.look;

		double step_x = player.dir_x * forward + player.plane_x * sideways;
		double step_y = player.dir_y * forward + player.plane_y * sideways;

		// you hear the steps. without them the character slides, and a
		// character that slides has no weight. there is one step every metre
		// sixty, and the sound stays under everything else.
		static double walked;
		walked += hypot(step_x, step_y);
		if (walked > 1.6) {
			walked = 0.0;
			if (alive)
				sound_play(SFX_STEP, 0.0);
		}
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
		// during the film there is only the film. the weapon and the gauges
		// inside a cinema frame are the one thing that could break the effect
		// once the bars are in place.
		int film = story_bars() > 0.005;
		if (!film)
			fight_draw(&player, moment, keys.forward || keys.back
				|| keys.strafe_left || keys.strafe_right);
		if (keys.map && story_control() && seen_count() > 12)
			render_map(&player, hud * MAP_CELL, hud * MAP_LEFT,
				hud * MAP_TOP);
		// the blast is drawn over the world and over the weapon, because it
		// is light, not an interface layer
		story_draw_blast();
		// the death screen is drawn over the world and under the bars
		story_draw_death();
		story_notice(moment < notice_until || story_dead());
		story_draw_dust(&player, moment);
		if (!film) {
			fight_hud(moment);
			draw_boss_bar(hud);
		}
		story_draw_log();
		if (moment < notice_until)
			draw_text_centered(view_height - hud * NOTICE_UP, notice,
				TEXT_COLOR, hud * TEXT_SCALE);
		// the bars are drawn over everything else, because they are a frame,
		// not a layer of the game
		story_draw_film();
		screen_present(&screen);
		if (trace && moment - said_at > 0.05) {
			said_at = moment;
			// we also print where the nearest awake thing stands, otherwise
			// the test bench shoots into nothing and no screenshot ever shows
			// a fight
			double cx = 0.0, cy = 0.0, nearest = VERY_FAR;
			for (int i = 0; i < THINGS_MAX; i++) {
				struct thing *t = &things[i];
				if (!t->used || t->roam || t->state == THING_DEAD
				    || t->state == THING_IDLE)
					continue;
				double d = hypot(t->x - player.x, t->y - player.y);
				if (d < nearest) { nearest = d; cx = t->x; cy = t->y; }
			}
			fprintf(stderr, "%.2f %.2f  cap %.2f  health %.0f  rounds %d"
				"  target %.2f %.2f  boss %.2f\n",
				player.x, player.y, atan2(player.dir_y, player.dir_x),
				fight.health, fight.ammo, cx, cy, boss_health());
		}

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
