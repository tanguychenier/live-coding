#ifndef SOUND_H
#define SOUND_H

// no file and no sample, only oscillators, envelopes, filters and a clock,
// the clock the whole game keeps time to. the music is layers turned on
// one by one, and everything the player does lands on a sixteenth
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
#define BPM             128.0
#define BEAT            (60.0 / BPM)
#define STEP            (BEAT / 4.0)
#define STEPS_PER_BAR   16
#define BAR             (BEAT * 4.0)
// the shape of the beat as the eye sees it, exp(-phase * decay), sharp on
// the kick and gone well before the next one
#define BEAT_DECAY      6.0

enum layer {
	LAYER_KICK,       // the four on the floor
	LAYER_HAT,        // the off beats, then every sixteenth
	LAYER_BASS,       // the riff in the low octave
	LAYER_PAD,        // three detuned saws, the chord of the zone
	LAYER_ARP,        // the chord played fast, when the swarm comes
	LAYER_LEAD,       // the clap and the riff on top, for the core
	LAYER_COUNT
};

// what the player does, each with its own voice. the note is a degree of
// the scale, from zero up, and it is the caller who spreads a chain of
// eight shots over eight rising degrees
enum hit { HIT_LOCK, HIT_SHOT, HIT_KILL, HIT_HURT, HIT_GATE, HIT_WARN,
	   HIT_BOSS, HIT_RISE, HIT_COUNT };
// the riser of the launch lasts this many bars and ends on the drop
#define RISE_BARS       2

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
// plays a hit at a moment on the music clock, quantized by the caller
void sound_hit(enum hit which, int note, double when);
void sound_mute(int on);
int  sound_muted(void);
// the offline path. with no card, the caller pulls the mix itself, so many
// frames at a time, and the clock advances with what it pulled
int  sound_offline(void);
void sound_render(short *stereo, int frames);

#endif
