#include "world.h"

#include <math.h>

// the whole world: a wall is anything that is not a dot
static const char *map[MAP_HEIGHT] = {
	"########################",
	"#......................#",
	"#..####..........####..#",
	"#..#..#..........#..#..#",
	"#..#..#..........#..#..#",
	"#..####..........####..#",
	"#......................#",
	"#......................#",
	"####.##########.########",
	"#......................#",
	"#..###....##....###....#",
	"#....#....##....#......#",
	"#....#....##....#......#",
	"#....#..........#......#",
	"#....############......#",
	"#......................#",
	"#......................#",
	"########.####.##########",
	"#......................#",
	"#..##..........##......#",
	"#..##..........##......#",
	"#......................#",
	"#......................#",
	"########################",
};

int is_wall(int x, int y)
{
	if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
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
