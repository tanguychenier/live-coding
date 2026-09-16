#include "world.h"
#include "texture.h"
#include "screen.h"
#include "render.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the world comes out of a file: a table compiled in means rebuilding the
// game to move one corridor
char **map;
int map_width, map_height;

// how far each door has opened, one number per square: a door is a place,
// not an object
static double *doors;

// what we have seen. a map that shows the whole level from the first second
// leaves nothing to find; this one fills in as one goes.
static char *seen;

// what has already been picked up: one cell, one byte. without it you
// walk back over it and help yourself forever.
static char *taken;

// whether the door system has given way
static int seals_released;

// when the system gives way, the doors do not offer anything, they open.
// while it holds, a sealed door says nothing and does not answer, and when it
// gives, every sealed door on the deck slides open by itself. the player is
// not granted a permission, he hears the deck open.
void world_release_seals(void)
{
	seals_released = 1;
	if (!doors)
		return;
	for (int y = 0; y < map_height; y++)
		for (int x = 0; x < (int)strlen(map[y]); x++)
			if (map[y][x] == 'L' && doors[y * map_width + x] == 0.0)
				doors[y * map_width + x] = 0.001;
}
int  world_seals_released(void) { return seals_released; }

// what the level file can hold. a wall is anything that is not floor.
int load_level(const char *path)
{
	FILE *f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "no level at %s\n", path);
		return 0;
	}

	char line[512];
	int room = 0;
	map_height = 0;
	map_width = 0;
	while (fgets(line, sizeof line, f)) {
		int length = (int)strlen(line);
		while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
			line[--length] = '\0';
		if (length == 0)
			continue;
		if (map_height == room) {
			room = room ? room * 2 : 32;
			char **bigger = realloc(map, (size_t)room * sizeof(*map));
			if (!bigger) {
				fclose(f);
				return 0;
			}
			map = bigger;
		}
		map[map_height] = strdup(line);
		if (!map[map_height]) {
			fclose(f);
			return 0;
		}
		if (length > map_width)
			map_width = length;
		map_height++;
	}
	fclose(f);

	// a rectangular map has to be one in memory too. the file's lines
	// are not all the same length, and every reader made up for it with
	// a strlen. on the floor that runs once per pixel, which is half
	// the frame. pad the rows on load and every strlen goes away.
	for (int y = 0; y < map_height; y++) {
		int length = (int)strlen(map[y]);
		if (length >= map_width)
			continue;
		char *square = realloc(map[y], (size_t)map_width + 1);
		if (!square)
			return 0;
		memset(square + length, '#', (size_t)map_width - length);
		square[map_width] = '\0';
		map[y] = square;
	}

	if (map_height > 0) {
		free(doors);
		doors = calloc((size_t)map_width * map_height, sizeof(*doors));
		free(seen);
		seen = calloc((size_t)map_width * map_height, 1);
		free(taken);
		taken = calloc((size_t)map_width * map_height, 1);
		if (!doors || !seen || !taken)
			return 0;
		seals_released = 0;
	}
	return map_height > 0;
}

// which way to look when the game opens: down the longest clear line, so the
// first thing anyone sees is a corridor and not a wall two steps away
static void face_the_open(double x, double y, double *dir_x, double *dir_y)
{
	const double way[4][2] = { {1, 0}, {0, 1}, {-1, 0}, {0, -1} };
	double best = -1.0;
	*dir_x = 1.0;
	*dir_y = 0.0;
	for (int i = 0; i < 4; i++) {
		double d = 0.0;
		while (d < 20.0 && !is_wall((int)(x + way[i][0] * (d + 0.5)),
					    (int)(y + way[i][1] * (d + 0.5))))
			d += 0.5;
		if (d > best) {
			best = d;
			*dir_x = way[i][0];
			*dir_y = way[i][1];
		}
	}
}

// 'S' is where we stand at the first frame, 'E' is the way out
void level_marks(double *start_x, double *start_y, int *exit_x, int *exit_y,
		 double *dir_x, double *dir_y)
{
	*start_x = 1.5;
	*start_y = 1.5;
	*exit_x = -1;
	*exit_y = -1;
	for (int y = 0; y < map_height; y++)
		for (int x = 0; map[y][x]; x++) {
			if (map[y][x] == 'S') {
				*start_x = x + 0.5;
				*start_y = y + 0.5;
			} else if (map[y][x] == 'E') {
				*exit_x = x;
				*exit_y = y;
			}
		}
	face_the_open(*start_x, *start_y, dir_x, dir_y);
}

// the door in front of us, if there is a shut one within reach. a few steps
// ahead, not at arm's length: one pushes a door from where one stands.
static int door_in_front(const struct player *player, int *door_x, int *door_y)
{
	for (double d = 0.6; d <= DOOR_REACH; d += 0.4) {
		int x = (int)(player->x + player->dir_x * d);
		int y = (int)(player->y + player->dir_y * d);
		if (is_door(x, y)) {
			*door_x = x;
			*door_y = y;
			return door_at(x, y) == 0.0;
		}
		if (is_wall(x, y))
			return 0;             // a wall between us: nothing to push
	}
	return 0;
}

// a game never asks for a key without saying so: the line shown on screen
// comes out of this function.
int door_ahead(const struct player *player)
{
	int x, y;
	if (!door_in_front(player, &x, &y))
		return 0;
	return !(map[y][x] == 'L' && !seals_released);
}

// we nudge the door once, and after that the two leaves finish their travel
// on their own. the door also answers, because what it does is what the
// player learns.
enum push push_door(const struct player *player)
{
	int x, y;
	if (!door_in_front(player, &x, &y))
		return PUSH_NOTHING;
	char c = map[y][x];
	if (c == 'L' && !seals_released)
		return PUSH_SEALED;
	if (c == 'D' && !have_badge())
		return PUSH_LOCKED;
	if (doors[y * map_width + x] > 0.0)
		return PUSH_NOTHING;
	doors[y * map_width + x] = 0.001;
	return c == 'J' ? PUSH_JAMMED : PUSH_OPENS;
}

double door_limit(int x, int y)
{
	if (x < 0 || y < 0 || x >= map_width || y >= map_height)
		return 1.0;
	return map[y][x] == 'J' ? DOOR_JAMMED_MAX : 1.0;
}

// the two leaves take a second to part, and they never close again: coming
// back this way should be a short cut, not a chore
void move_doors(double elapsed)
{
	if (!doors)
		return;
	for (int i = 0; i < map_width * map_height; i++) {
		double max = door_limit(i % map_width, i / map_width);
		if (doors[i] > 0.0 && doors[i] < max) {
			doors[i] += elapsed / DOOR_SECONDS;
			if (doors[i] > max)
				doors[i] = max;
		}
	}
}

int seen_count(void)
{
	int n = 0;
	for (int y = 0; y < map_height; y++)
		for (int x = 0; x < map_width; x++)
			if (is_seen(x, y))
				n++;
	return n;
}

int is_seen(int x, int y)
{
	if (!seen || x < 0 || y < 0 || x >= map_width || y >= map_height)
		return 0;
	return seen[y * map_width + x];
}

// everything a few steps away, and nothing behind a wall: we mark what the
// light really reaches
void remember(const struct player *player)
{
	if (!seen)
		return;
	int cx = (int)player->x, cy = (int)player->y;
	for (int y = cy - SEEN_REACH; y <= cy + SEEN_REACH; y++)
		for (int x = cx - SEEN_REACH; x <= cx + SEEN_REACH; x++) {
			if (x < 0 || y < 0 || x >= map_width || y >= map_height)
				continue;
			if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > SEEN_REACH * SEEN_REACH)
				continue;
			seen[y * map_width + x] = 1;
		}
}

// the studio name, painted on the airlock floor. gives back which of the
// four squares we are looking at, or -1 if it is none of them.
// the badge. a way out and nothing else to look for is a corridor, not a
// level: the hatch is locked, and what opens it is down in the hold.
static int badge_taken;

int have_badge(void)
{
	return badge_taken;
}

int stencil_at(int x, int y)
{
	// the squares follow one another across the walk: that is how the letters
	// line up left to right for anyone heading east
	if (x != STENCIL_X || y < STENCIL_Y || y >= STENCIL_Y + STENCIL_CELLS)
		return -1;
	return y - STENCIL_Y;
}

int is_door(int x, int y)
{
	if (x < 0 || y < 0 || x >= map_width || y >= map_height)
		return 0;
	char c = map[y][x];
	return c == '+' || c == 'J' || c == 'L' || c == 'D';
}

// a strip sits on a wall: the file puts a letter where the wall would be,
// and that letter carries the neon
int is_lamp(int x, int y)
{
	if (x < 0 || y < 0 || x >= map_width || y >= map_height)
		return 0;
	char c = map[y][x];
	return c == 'T' || c == 'R' || c == 'B' || c == 'F';
}

// you pick it up by walking over it. a chest to open is a panel that eats
// the screen and stops the walk; a plate on the floor is seen from far,
// taken without stopping, and the eyes stay on the room.
enum loot loot_at(int x, int y)
{
	if (x < 0 || y < 0 || x >= map_width || y >= map_height || (taken && taken[y * map_width + x]))
		return LOOT_NONE;
	switch (map[y][x]) {
	case 'K': return LOOT_BADGE;
	case 'G': return LOOT_GUN;
	case 'A': return LOOT_AMMO;
	case 'M': return LOOT_MED;
	default:  return LOOT_NONE;
	}
}

enum loot loot_take(const struct player *player)
{
	int x = (int)player->x, y = (int)player->y;
	enum loot l = loot_at(x, y);
	if (l == LOOT_NONE)
		return LOOT_NONE;
	taken[y * map_width + x] = 1;
	if (l == LOOT_BADGE)
		badge_taken = 1;
	return l;
}

int is_badge(int x, int y)
{
	return loot_at(x, y) == LOOT_BADGE;
}

// a tube at the end of its life. "F" for failing: it holds, it drops, it
// comes back. this is not a ripple, it is a wobble that no longer passes.
int lamp_faulty(int x, int y)
{
	if (x < 0 || y < 0 || x >= map_width || y >= map_height)
		return 0;
	return map[y][x] == 'F';
}

double door_at(int x, int y)
{
	if (x < 0 || y < 0 || x >= map_width || y >= map_height
	    || !doors)
		return 0.0;
	return doors[y * map_width + x];
}

// the level file says which wall it is: '1' is the first, '2' the second and
// so on. anything else is the first, so an old level still reads.
int wall_kind(int x, int y)
{
	if (x < 0 || y < 0 || x >= map_width || y >= map_height)
		return 0;
	char c = map[y][x];
	if (c == 'T' || c == 'F')
		return LAMP_TEXTURE;
	if (c == 'R')
		return ALARM_TEXTURE;
	if (c == 'B')
		return COLD_TEXTURE;
	if (c >= '1' && c < '1' + WALL_KINDS)
		return c - '1';
	return 0;
}

int is_wall(int x, int y)
{
	if (x < 0 || x >= map_width || y < 0 || y >= map_height)
		return 1;
	// the file carries more than walls and floor: where the player
	// starts, and where the way out is. only a wall stops anyone.
	char c = map[y][x];
	// every door can be walked through, not only the ordinary one, because a
	// sealed door that opens and stays a wall would end the level without a
	// word. the jammed one never opens far enough to pass, and it is its
	// limit that forbids it, not an exception written here.
	if (is_door(x, y))
		return door_at(x, y) < DOOR_WALKABLE;
	return !(c == '.' || c == 'S' || c == 'E' || c == 'K' || c == 'G'
	         || c == 'A' || c == 'M' || c == 'x' || c == 'y' || c == 'w'
	         || c == 'Z' || c == 'X' || c == 'n' || c == 'h');
}

// the camera plane follows the shape of the window. its length is the field
// of view: keeping it fixed as the window widens stretches the picture.
void fit_view_to_window(struct player *player)
{
	double aspect = (double)view_width / view_height;
	double length = FIELD_OF_VIEW * aspect / (16.0 / 10.0);
	double n = hypot(player->plane_x, player->plane_y);
	if (n <= 0.0)
		return;
	player->plane_x = player->plane_x / n * length;
	player->plane_y = player->plane_y / n * length;
}

// each axis is tested on its own, so a shoulder against a wall keeps sliding
// instead of stopping the player dead. the margin is the nose: without it we
// walk until the eyes are inside the texture
void move_player(struct player *player, double step_x, double step_y)
{
	const double margin = 0.2;
	double nose_x = step_x > 0 ? margin : -margin;
	double nose_y = step_y > 0 ? margin : -margin;

	if (!is_wall((int)(player->x + step_x + nose_x), (int)player->y))
		player->x += step_x;
	if (!is_wall((int)player->x, (int)(player->y + step_y + nose_y)))
		player->y += step_y;

	// doors are taken without snagging. a one metre gap with a twenty
	// centimetre nose leaves sixty usable, so you scrape the frame every
	// other time and blame the controls. once inside a door cell the
	// walk is pulled gently back to its middle.
	int cx = (int)player->x, cy = (int)player->y;
	if (!is_door(cx, cy))
		return;
	double magnet = 0.12;
	if (is_wall(cx - 1, cy) && is_wall(cx + 1, cy))
		player->x += (cx + 0.5 - player->x) * magnet;
	else if (is_wall(cx, cy - 1) && is_wall(cx, cy + 1))
		player->y += (cy + 0.5 - player->y) * magnet;
}

// turning is one rotation matrix, applied to both vectors at once
void turn_player(struct player *player, double angle)
{
	double cosine = cos(angle);
	double sine = sin(angle);

	double dir_x = player->dir_x;
	player->dir_x = dir_x * cosine - player->dir_y * sine;
	player->dir_y = dir_x * sine + player->dir_y * cosine;

	double plane_x = player->plane_x;
	player->plane_x = plane_x * cosine - player->plane_y * sine;
	player->plane_y = plane_x * sine + player->plane_y * cosine;
}
