#include "world.h"
#include "texture.h"

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
	if (map_height > 0) {
		free(doors);
		doors = calloc((size_t)map_width * map_height, sizeof(*doors));
		if (!doors)
			return 0;
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

// we look a few steps ahead, not at arm's length: nobody walks into a door
// to open it, one pushes it from where one stands
void push_door(const struct player *player)
{
	for (double d = 0.6; d <= DOOR_REACH; d += 0.4) {
		int x = (int)(player->x + player->dir_x * d);
		int y = (int)(player->y + player->dir_y * d);
		if (is_door(x, y)) {
			if (doors[y * map_width + x] == 0.0)
				doors[y * map_width + x] = 0.001;
			return;
		}
		if (is_wall(x, y))
			return;              // a wall between us: nothing to push
	}
}

// the two leaves take a second to part, and they never close again: coming
// back this way should be a short cut, not a chore
void move_doors(double elapsed)
{
	if (!doors)
		return;
	for (int i = 0; i < map_width * map_height; i++)
		if (doors[i] > 0.0 && doors[i] < 1.0) {
			doors[i] += elapsed / DOOR_SECONDS;
			if (doors[i] > 1.0)
				doors[i] = 1.0;
		}
}

int is_door(int x, int y)
{
	if (x < 0 || y < 0 || x >= map_width || y >= map_height
	    || x >= (int)strlen(map[y]))
		return 0;
	return map[y][x] == '+';
}

// a strip sits on a wall: the file puts a letter where the wall would be,
// and that letter carries the neon
int is_lamp(int x, int y)
{
	if (x < 0 || y < 0 || x >= map_width || y >= map_height
	    || x >= (int)strlen(map[y]))
		return 0;
	return map[y][x] == 'T';
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
	if (x < 0 || y < 0 || x >= map_width || y >= map_height
	    || x >= (int)strlen(map[y]))
		return 0;
	char c = map[y][x];
	if (c == 'T')
		return LAMP_TEXTURE;
	if (c >= '1' && c < '1' + WALL_KINDS)
		return c - '1';
	return 0;
}

int is_wall(int x, int y)
{
	if (x < 0 || x >= map_width || y < 0 || y >= map_height
	    || x >= (int)strlen(map[y]))
		return 1;
	// the file carries more than walls and floor: where the player
	// starts, and where the way out is. only a wall stops anyone.
	char c = map[y][x];
	if (c == '+')
		return door_at(x, y) < DOOR_WALKABLE;
	return !(c == '.' || c == 'S' || c == 'E');
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
