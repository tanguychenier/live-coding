#include <math.h>
#include <string.h>

#include "palette.h"
#include "player.h"

void player_reset(struct player *player)
{
	memset(player, 0, sizeof *player);
	player->cursor_x = view_width / 2.0;
	player->cursor_y = view_height / 2.0;
}

// the cursor follows the mouse when it moves, and the arrows otherwise. the
// two never fight, the last one that moved wins
static void move_cursor(struct player *player, const struct keys *keys, double elapsed)
{
	static int last_mouse_x = -1, last_mouse_y = -1;
	if (keys->mouse_x != last_mouse_x || keys->mouse_y != last_mouse_y) {
		if (last_mouse_x >= 0) {
			player->cursor_x = keys->mouse_x;
			player->cursor_y = keys->mouse_y;
		}
		last_mouse_x = keys->mouse_x;
		last_mouse_y = keys->mouse_y;
	}
	double speed = CURSOR_SPEED * draw_scale();
	player->cursor_x += (keys->right - keys->left) * speed * elapsed;
	player->cursor_y += (keys->down - keys->up) * speed * elapsed;
	if (player->cursor_x < 0) player->cursor_x = 0;
	if (player->cursor_y < 0) player->cursor_y = 0;
	if (player->cursor_x > view_width - 1) player->cursor_x = view_width - 1;
	if (player->cursor_y > view_height - 1) player->cursor_y = view_height - 1;
}

void player_update(struct player *player, const struct keys *keys, double elapsed)
{
	move_cursor(player, keys, elapsed);
	player->holding = keys->fire || keys->mouse_down;
}

// the cursor is a ring with four gaps, a sight and not a plate, and it grows
// while it marks
static void draw_cursor(const struct player *player, double now)
{
	double x = player->cursor_x, y = player->cursor_y;
	double px = draw_scale();
	double radius = LOCK_RADIUS * px
		* (player->holding ? 1.0 + CURSOR_PULSE * sin(now * CURSOR_PULSE_RATE) : CURSOR_REST);
	struct light lit = player->holding ? LIGHT_CURSOR_HOT : LIGHT_CURSOR;
	const int segments = 48, gap_every = 12, gap_size = 2;
	for (int i = 0; i < segments; i++) {
		double a0 = 2 * M_PI * i / segments, a1 = 2 * M_PI * (i + 1) / segments;
		if (i % gap_every >= gap_every - gap_size)
			continue;
		draw_line_2d(x + radius * cos(a0), y + radius * sin(a0),
			     x + radius * cos(a1), y + radius * sin(a1), lit);
	}
	// a dot in the middle, so the eye has a point to aim
	draw_line_2d(x - px, y, x + px, y, lit);
}

void player_draw(const struct player *player, double now)
{
	draw_cursor(player, now);
}
