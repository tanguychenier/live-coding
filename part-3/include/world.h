#ifndef WORLD_H
#define WORLD_H

// the level is read at startup, so its size is not known at compile time
extern char **map;
extern int map_width, map_height;

#define LEVEL_FILE "levels/keep.txt"

#define WALK_SPEED 3.0
#define TURN_SPEED 2.2
struct player {
	double x, y;                // where we stand, in map squares
	double dir_x, dir_y;        // a unit vector: where the eyes point
	double plane_x, plane_y;    // the camera plane, as wide as the view is
};

#define FRAMES_PER_SECOND 60

int load_level(const char *path);
int is_wall(int x, int y);
void move_player(struct player *player, double step_x, double step_y);
void turn_player(struct player *player, double angle);

#endif
