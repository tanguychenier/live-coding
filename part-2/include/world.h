#ifndef WORLD_H
#define WORLD_H

#define MAP_WIDTH  24
#define MAP_HEIGHT 24

// where the player stands when the game opens: a free square in the corridor
#define START_X 2.5
#define START_Y 6.5

#define WALK_SPEED 3.0
#define TURN_SPEED 2.2
struct player {
	double x, y;                // where we stand, in map squares
	double dir_x, dir_y;        // a unit vector: where the eyes point
	double plane_x, plane_y;    // the camera plane, as wide as the view is
};

#define FRAMES_PER_SECOND 60

int is_wall(int x, int y);
void move_player(struct player *player, double step_x, double step_y);
void turn_player(struct player *player, double angle);

#endif
