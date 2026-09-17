// a shooter on a rail, drawn in lines of light. the eye rides the rail, the
// world breathes on the beat, things come at you, you mark up to eight of
// them and let go, and every shot lands on a sixteenth and plays a note

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "draw.h"
#include "mesh.h"
#include "palette.h"
#include "screen.h"

#define GAME_NAME        "AXON"
// a frame is never longer than this, whatever the machine did meanwhile
#define FRAME_CAP        0.05
#define FRAME_SECONDS    (1.0 / 60.0)
// the solid that turns in front of the eye, how far, how big, how fast
#define TEST_DEPTH       6.0
#define TEST_SIZE        1.5
#define TEST_TURN        0.7
// the sky of the test, a little blue at the bottom, and its fog
#define TEST_SKY_BLUE    0.04
#define TEST_FOG         38.0

static double now_in_seconds(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(void)
{
	struct screen screen;
	if (!screen_open(&screen, GAME_NAME))
		return 1;
	struct keys keys = { 0 };
	struct camera camera;
	double start = now_in_seconds();

	while (!keys.quit) {
		double moment = now_in_seconds();
		double now = moment - start;
		screen_read_keys(&screen, &keys);
		camera_look(&camera, vec(0, 0, 0), vec(0, 0, 1), vec(0, 1, 0), 0.0, FOCAL);
		draw_clear(light(0, 0, 0), light(0, 0, TEST_SKY_BLUE));
		draw_fog(TEST_FOG);
		mesh_draw(&camera, mesh_of(SHAPE_CUBE), vec(0, 0, TEST_DEPTH), TEST_SIZE,
			  now * TEST_TURN, now * TEST_TURN * 0.5, 0.0, LIGHT_WHITE, 1);
		draw_finish();
		screen_present(&screen);
		double spent = now_in_seconds() - moment;
		if (spent < FRAME_SECONDS)
			usleep((useconds_t)((FRAME_SECONDS - spent) * 1e6));
	}
	screen_close(&screen);
	return 0;
}
