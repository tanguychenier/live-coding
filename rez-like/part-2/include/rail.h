#ifndef RAIL_H
#define RAIL_H

#include "vec.h"

// the rail is a smooth curve through a list of points, the camera and the
// tunnel both follow it. t counts the points, a point index with a fraction
#define RAIL_POINTS_MAX  400
// how far apart the points are laid, in world units. the bends of the
// curve are as wide as this, which is what makes them gentle at speed
#define RAIL_SPACING     30.0
// the rail is a chain of moves, each so many points long, turning and
// pitching by so many radians per point
#define TURN_SOFT        0.10
#define TURN_HARD        0.20
#define TURN_SCREW       0.32
#define PITCH_SOFT       0.10
#define PITCH_HARD       0.18
#define PACE_CRUISE      0.75
#define PACE_NORMAL      1.0
#define PACE_FAST        1.4
#define PACE_SPRINT      1.8
// the plain has a floor, and the rail never goes under this height above it
#define PLAIN_LOWEST     4.0
// where the tunnel opens or closes, the change takes this many points
#define OPEN_BLEND       1.5
#define RAIL_ZONES_MAX   8

struct move {
	int points;
	double turn, pitch;      // radians per point, left is positive
	double pace;             // times the zone's pace
	int open;                // the tunnel is open on the void here
};

struct rail {
	struct vec point[RAIL_POINTS_MAX];
	double pace[RAIL_POINTS_MAX];    // the pace at each point
	int open[RAIL_POINTS_MAX];       // the tunnel is open there
	int count;
	int zones;
	double zone_start[RAIL_ZONES_MAX + 1];   // where each zone begins, in rail units
};

// catmull rom, the curve between two points bends towards its neighbours
// on both sides, which is what makes it smooth
struct vec rail_at(const struct rail *rail, double t);
// which way the rail goes at t, a unit vector
struct vec rail_forward(const struct rail *rail, double t);
// the frame of the rail at t, right and up square to the forward direction.
// the camera and the scenery share it, so they agree on which way is up
void rail_frame(const struct rail *rail, double t, struct vec *forward,
		struct vec *right, struct vec *up);
// how far t is allowed to go, the last point of the curve
double rail_end(const struct rail *rail);
// lays the whole level from the moves of each zone
void rail_build(struct rail *rail, int zones, const struct move *const moves[],
		const int move_counts[]);
int rail_zone_at(const struct rail *rail, double t);
// the pace at t, and how open the tunnel is there, from zero to one
double rail_pace(const struct rail *rail, double t);
double rail_open(const struct rail *rail, double t);

#endif
