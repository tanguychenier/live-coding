#ifndef LIGHT_H
#define LIGHT_H

// the light comes from the strips set in the level: pools of it, darkness
// between them, and enough to find one's way.

// how far the lamp we carry reaches, in squares. beyond that it is black
#define LAMP_REACH   7.0
// what is left of it right at the edge
#define LAMP_FLOOR   0.04
// a neon pulses, very little and very fast
#define LAMP_HUM     0.035
#define LAMP_HZ      9.0
// and a failing tube goes out for good for the length of a beat
#define FAULT_LOW    0.10
#define FAULT_HALF   0.62

// a surface facing us takes the light, one seen edge on does not: without
// that, everything close by shines like a glowing camera
#define LAMP_AMBIENT 0.30     // what a surface takes even facing away

// how many squares a strip on a wall reaches
#define LAMP_ON_WALL 7.0
// the ceiling strips run everywhere: they give every square its base light,
// and the ones on the walls make the pools
#define STRIP_LIGHT  0.30
// the blue gloom behind it all, faint: the neon is what makes the picture
#define GLOOM        0.11
// the glow we carry ourselves, faint: it keeps us out of pure black
#define CARRIED      0.30
// the ceiling takes less than the floor: a strip lights downward
#define CEILING_PART 0.45

// a strip hangs at head height: with no height a column of wall is lit the
// same from floor to ceiling, and comes out flat
#define LAMP_HEIGHT  0.45      // where the strip hangs, 0 at the wall top
#define UNDER_ROOF   1.25      // how much the top of the wall loses
#define AT_THE_FOOT  0.52      // and how much its foot loses

// the light map of the level, worked out once at load time
void light_map(void);
double lit_here(int x, int y);
// the same, continuous: we read between the squares
double lit_at(double x, double y);

// what a point on a wall takes by its height, 0 at the top, 1 at the foot
double at_height(double v);
// what the carried lamp gives at that distance, at this instant
double lamp(double distance);
// the neon is alive: called once a frame, with the clock
void lamp_flicker(double seconds);

#endif
