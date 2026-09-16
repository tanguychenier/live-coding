#ifndef STORY_H
#define STORY_H

#include "world.h"

// the opening is not a cutscene, and nothing is taken away from the player.
// the station shakes because it is being hit, the log scrolls because it is
// being written, and the colour changes because the alarm is on. all of it
// runs while he walks.

// how long the sequence in the airlock lasts before the room goes quiet
#define STORY_OPENING   26.0
// how far the picture gets thrown at the worst of a hit, in view pixels
#define SHAKE_MAX       7
// the log is written one character at a time, at this speed
#define LOG_SPEED       34.0
#define LOG_LINES       6
#define LOG_RIGHT       4
#define LOG_TOP         4

// ------------------------------------------------------------- the film
// two black bars, and the player knows at once where he stands. while they
// are on screen, he watches; when they leave, he plays. no panel has to say
// "cutscene": the frame says it, the way films always have.
#define BARS_MAX   0.155      // how much of the view one bar takes
#define BARS_IN    0.9        // how long they take to come down
#define BARS_OUT   1.2        // and to leave, a little slower
#define FILM_LINE  3.2        // how long one line stays on screen

void story_begin(double now);
// how tall a bar is, as a share of the view. it is 0 while we are playing.
double story_bars(void);
// tells whether the player can walk, or whether he is only watching
int  story_control(void);
// how fast the camera drifts on its own during the opening, in radians per
// second
double story_drift(void);
// pressing space during the film skips it
void story_skip(double now);
// at the end the bars come back and the game closes on its own. the closing
// lines also say how many he put down.
void story_score(int amount);
void story_finish(double now);
// when we start over, the opening film does not play again, because it has
// already been seen
void story_restart(double now);
// death is a whole screen, not a line at the bottom. the world freezes, loses
// its colour and turns red, the bars come back, and what to do next is
// written in large letters. without that the player does not even know that
// he lost.
void story_death(double now);
int  story_dead(void);
void story_draw_death(void);
int  story_finished(void);
void story_draw_film(void);
// advances the film and the log. when the player opens a door, the alarm ends
// sooner.
void story_update(double now, int doors_opened);
// the clock is handed over once per frame, so that nothing else has to carry
// it around
void story_tick(double now);
// how far the picture is shifted this frame, in view pixels. axis 0 is across
// and axis 1 is up and down.
int  story_shake(int axis);
// the alarm lights the top of the view and the edges. x and y say which pixel
// we are colouring.
unsigned int story_grade_at(unsigned int color, int y);
// where the hit landed. the dust rises at that spot, not in front of the
// camera.
void story_shake_here(double x, double y);
// something hits the station now, at this spot. we play a rumble half a
// second before and then a shake that fades, because that order is what makes
// it feel like an impact.
void story_hit(double now, double force, double x, double y);
// the blast puts a warm glow over everything, and then it falls back
void story_blast(double now);
void story_draw_blast(void);
void story_draw_dust(const struct player *player, double now);
// the game tells us whether a notice is already on screen, and the log stays
// quiet while it is
void story_notice(int visible);
void story_draw_log(void);
int  story_opening_over(void);

#endif
