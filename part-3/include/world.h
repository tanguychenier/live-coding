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
void level_marks(double *start_x, double *start_y, int *exit_x, int *exit_y,
		 double *dir_x, double *dir_y);
int is_wall(int x, int y);
int wall_kind(int x, int y);
int is_lamp(int x, int y);
// how far the player remembers what has been seen
#define SEEN_REACH 4

int is_seen(int x, int y);
void remember(const struct player *player);
// and this one no longer holds: it flickers
int lamp_faulty(int x, int y);
// a door takes a second to open, and once it is open this far one walks
// between the two leaves
#define DOOR_SECONDS  1.2
#define DOOR_WALKABLE 0.8
// how far a push reaches: one opens a door from where one stands, not with
// one's nose against the leaf
#define DOOR_REACH    2.6

int is_door(int x, int y);
// from 0 (shut) to 1 (both leaves tucked into the wall)
double door_at(int x, int y);
void push_door(const struct player *player);
void move_doors(double elapsed);
void move_player(struct player *player, double step_x, double step_y);
void turn_player(struct player *player, double angle);
void fit_view_to_window(struct player *player);

#endif
