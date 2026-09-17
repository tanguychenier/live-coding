#ifndef HERO_H
#define HERO_H

#include "draw.h"

// the pilot, a figure in lines of light flying ahead of the eye, seen from
// behind, arms out. it leans towards the cursor, banks with the rail,
// throws its arms forward when a chain leaves, and tumbles when hit
#define HERO_AHEAD       4.5     // units in front of the eye
#define HERO_BELOW       1.05    // units under the line of sight
#define HERO_SIZE        0.42    // one mesh unit in world units
// how far it moves sideways and up when the cursor is at the edge
#define HERO_LEAN        1.5
#define HERO_LEAN_UP     0.6
// it follows the cursor with a lag, in parts per second, which is what makes
// it look flown and not glued
#define HERO_FOLLOW      6.0
// the bank at full lean, in radians, and the nose down pitch that shows the
// back to the eye
#define HERO_BANK        0.75
#define HERO_PITCH       -0.2
// seconds the arms stay thrown forward after a release
#define HERO_THRUST      0.4
// thrown forward, the hands close to this much of their spread, rise and
// go ahead by this much, the elbows a little less
#define HERO_THRUST_IN   0.6
#define HERO_THRUST_UP   0.1
#define HERO_THRUST_OUT  0.7
#define HERO_ELBOW_IN    0.75
#define HERO_ELBOW_UP    0.05
#define HERO_ELBOW_OUT   0.4
// the light in each hand, and how much more when the hands are thrown
#define HERO_HAND_GLOW   0.05
#define HERO_HAND_FLARE  0.08
// the breath on the beat, in mesh units
#define HERO_BOB         0.05
// the trail of sparks from the feet, per second, their size and their life,
// and the wake behind each hand, in world units
#define HERO_TRAIL_RATE  60.0
#define HERO_TRAIL_SIZE  0.012
#define HERO_TRAIL_LIFE  0.25
#define HERO_TRAIL_SPEED 1.5
#define HERO_WAKE        1.6
#define HERO_WAKE_LIGHT  0.3

struct hero {
	struct vec at;               // where the chest is, in the world
	struct vec right, up, forward;   // the frame it is drawn in
	struct vec hands;            // between the hands, where shots leave
	double lean_x, lean_y;       // towards the cursor, minus one to one
	double bank;                 // smoothed roll
	double thrust_at;
	double trail_due;
};

void hero_reset(struct hero *hero);
// follows the cursor and the camera. released is the event of this frame
void hero_update(struct hero *hero, const struct camera *cam, double cursor_x,
		 double cursor_y, int released, double elapsed, double now);
void hero_draw(const struct hero *hero, const struct camera *cam, double now);

#endif
