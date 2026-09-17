// a shooter on a rail, drawn in lines of light. the eye rides the rail, the
// world breathes on the beat, things come at you, you mark up to eight of
// them and let go, and every shot lands on a sixteenth and plays a note

#include <stdlib.h>
#include <unistd.h>

#include "draw.h"
#include "screen.h"

#define GAME_NAME        "AXON"
#define FRAME_SECONDS    (1.0 / 60.0)
// the top of a byte, for the gradient that proves the pixels are ours
#define BYTE_TOP         255

int main(void)
{
	struct screen screen;
	if (!screen_open(&screen, GAME_NAME))
		return 1;
	struct keys keys = { 0 };

	while (!keys.quit) {
		screen_read_keys(&screen, &keys);
		for (int y = 0; y < view_height; y++)
			for (int x = 0; x < view_width; x++)
				view[y * view_width + x] = rgb(x * BYTE_TOP / view_width,
							       y * BYTE_TOP / view_height, 0);
		screen_present(&screen);
		usleep((useconds_t)(FRAME_SECONDS * 1e6));
	}
	screen_close(&screen);
	return 0;
}
