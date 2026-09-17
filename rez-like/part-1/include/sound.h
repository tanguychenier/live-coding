#ifndef SOUND_H
#define SOUND_H

// the clock the whole game keeps time to is the music's. the level is
// measured in its bars, and everything the player does lands on a sixteenth
#define BPM             128.0
#define BEAT            (60.0 / BPM)
#define STEP            (BEAT / 4.0)
#define STEPS_PER_BAR   16
#define BAR             (BEAT * 4.0)
// the shape of the beat as the eye sees it, exp(-phase * decay), sharp on
// the kick and gone well before the next one
#define BEAT_DECAY      6.0

#endif
