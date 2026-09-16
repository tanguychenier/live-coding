#ifndef TRIGGER_H
#define TRIGGER_H

#include "world.h"

// the story is set off by places, not by the clock. the player crosses a line
// and the scene starts, so he thinks it happened because he went there, and
// that is true. nothing takes the keys away from him, everything plays in
// front of him while he walks.

void triggers_reset(void);
// called every frame. it gives the line to show, or NULL when there is none.
const char *triggers_step(const struct player *player, double now);

#endif
