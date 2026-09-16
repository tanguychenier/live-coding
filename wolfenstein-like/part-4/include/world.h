#ifndef WORLD_H

// how long the hatch takes to open, which is how long the corridor has to be
// held
#define HATCH_SECONDS 20.0
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
// where the name is painted on the airlock floor. it takes four floor cells,
// from (7,4) to (7,7), right in front of the player, who wakes up at (6,6).
#define STENCIL_X 9
#define STENCIL_Y 5
int stencil_at(int x, int y);
int is_badge(int x, int y);
int have_badge(void);
// how far the player remembers what has been seen
#define SEEN_REACH 4

int is_seen(int x, int y);
// how many cells have been seen so far. the map stays off screen while it has
// nothing to show.
int seen_count(void);
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

// the way out: once the hatch opens the view goes dark over this many
// seconds, down to this much, and the words stay. not to black: the room
// is still there, behind the words
#define ENDING_FADE   2.5
#define ENDING_DARK   0.82

// three kinds of door, and two of them tell you something. "+" simply opens.
// "L" stays sealed while the door system holds, and it is the system that
// will give. "J" is jammed, so it opens a third of the way and sticks, and
// you see through it without getting through.
#define DOOR_JAMMED_MAX 0.46

enum push { PUSH_NOTHING, PUSH_OPENS, PUSH_JAMMED, PUSH_SEALED, PUSH_LOCKED };

// the door system gives way, and everything that was sealed can open
void world_release_seals(void);
int  world_seals_released(void);
// what lies on the floor to be picked up, and what is left of it
enum loot { LOOT_NONE, LOOT_GUN, LOOT_AMMO, LOOT_MED, LOOT_BADGE };
enum loot loot_at(int x, int y);
enum loot loot_take(const struct player *player);
// where the thing at the bottom stands, in the last room
int is_door(int x, int y);
// how far this door is able to open
double door_limit(int x, int y);
// from 0 (shut) to 1 (both leaves tucked into the wall)
double door_at(int x, int y);
// is there a shut door in front of us, within reach?
int door_ahead(const struct player *player);
enum push push_door(const struct player *player);
void move_doors(double elapsed);
void move_player(struct player *player, double step_x, double step_y);
void turn_player(struct player *player, double angle);
void fit_view_to_window(struct player *player);

#endif
