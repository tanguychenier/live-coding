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

// no file and no sample, only oscillators, envelopes, filters and a clock.
// the music is layers turned on one by one
#define SOUND_RATE      44100
// the output is kept low on purpose, a game in the background must not make
// the room jump, and the chains of eight impacts are what should crack
#define SOUND_MASTER    0.25
#define SOUND_CHUNK     512
// how much sound alsa keeps ahead of the speaker, in microseconds. shorter
// stutters on a busy machine, longer and a hit waits longer for its turn
#define SOUND_LATENCY   60000
// a hit is scheduled at least this far ahead, so that the mixer, which runs
// ahead by the buffer and a chunk, has not passed the moment it is asked for
#define SOUND_LEAD      0.14

enum layer {
	LAYER_KICK,       // the four on the floor
	LAYER_HAT,        // the off beats, then every sixteenth
	LAYER_BASS,       // the riff in the low octave
	LAYER_PAD,        // three detuned saws, the chord of the zone
	LAYER_COUNT
};

int  sound_open(void);
void sound_close(void);
// seconds on the music clock, the one the pictures and the shots follow.
// it is the time of the run, and a run can start again at any bar of the
// level, so the clock is told where the run stands and keeps the beat
double sound_now(void);
void sound_restart(double run_time);
// the first sixteenth far enough after a moment for the mixer to catch it
double sound_next_step(double after);
// where the beat is, zero on a beat and one just before the next
double sound_beat_phase(double t);
// a layer slides to a level between zero and one
void sound_layer(enum layer which, double target);
// the chord and the bass root of the zone
void sound_zone(int zone);
void sound_mute(int on);
int  sound_muted(void);

#endif
