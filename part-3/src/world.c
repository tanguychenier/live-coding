#include "world.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the world comes out of a file: a table compiled in means rebuilding the
// game to move one corridor
char **map;
int map_width, map_height;

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
	return map_height > 0;
}

int is_wall(int x, int y)
{
	if (x < 0 || x >= map_width || y < 0 || y >= map_height
	    || x >= (int)strlen(map[y]))
		return 1;
	return map[y][x] != '.';
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
