#include <alsa/asoundlib.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sound.h"

// ---------------------------------------------------------------- the parts
// four pieces, an oscillator, an envelope, a filter and a clock. a kick is a
// sine whose pitch falls, a hat is noise above a cut, a bass a saw under one

// the note frequencies, from c0, which is the c four octaves under middle c
#define C0_HZ            16.3516
// the key is c minor. the bass root sits two octaves up from c0, the pad
// three, the hits four and five, so that each part has its own floor
#define OCTAVE_BASS      2
#define OCTAVE_PAD       3
#define OCTAVE_HIT       4
// a filter cannot be asked for more than the top of hearing
#define FILTER_TOP       20000.0
// the delay, three sixteenths on the left and two on the right, which is
// what makes a dry hit swing. the feedback keeps it from turning to mud
#define ECHO_STEPS_LEFT  3
#define ECHO_STEPS_RIGHT 2
#define ECHO_FEEDBACK    0.32
#define ECHO_SEND        0.45
#define ECHO_CUT         3200.0
#define ECHO_Q           0.7
#define ECHO_MAX         (SOUND_RATE * 2)
// a layer slides to its level over this many seconds
#define LAYER_SLIDE      1.5
// the soft clip at the end, an amplifier that runs out of headroom, and the
// loudest sample written, a little under the top of sixteen bits
#define DRIVE_OUT        1.4
#define SAMPLE_PEAK      32000.0

struct filter { double low, band; };

static double lowpass(struct filter *filter, double in, double cut, double resonance)
{
	double g = 2.0 * sin(M_PI * (cut < FILTER_TOP ? cut : FILTER_TOP) / SOUND_RATE);
	filter->low += g * filter->band;
	double high = in - filter->low - resonance * filter->band;
	filter->band += g * high;
	return filter->low;
}

static double highpass(struct filter *filter, double in, double cut, double resonance)
{
	double g = 2.0 * sin(M_PI * (cut < FILTER_TOP ? cut : FILTER_TOP) / SOUND_RATE);
	filter->low += g * filter->band;
	double high = in - filter->low - resonance * filter->band;
	filter->band += g * high;
	return high;
}

static double saw(double phase) { return 2.0 * (phase - floor(phase + 0.5)); }
static double square(double phase) { return phase - floor(phase) < 0.5 ? 1.0 : -1.0; }

// the noise is a linear congruential generator, the one of the c standard,
// and a sample takes sixteen of its high bits, spread from minus one to one
#define NOISE_MULTIPLIER 1103515245u
#define NOISE_INCREMENT  12345u
#define NOISE_SKIP       9
#define NOISE_BITS       16
#define NOISE_MASK       ((1u << NOISE_BITS) - 1)
#define NOISE_HALF       (1u << (NOISE_BITS - 1))

static unsigned int seed = 1;
static double noise(void)
{
	seed = seed * NOISE_MULTIPLIER + NOISE_INCREMENT;
	return (double)((seed >> NOISE_SKIP) & NOISE_MASK) / NOISE_HALF - 1.0;
}

// up in attack seconds, then down to nothing over decay seconds, with the
// curve of a struck thing, fast at first and long at the end
static double envelope(double t, double attack, double decay)
{
	if (t < 0.0)
		return 0.0;
	if (t < attack)
		return t / attack;
	double part = (t - attack) / decay;
	return part < 1.0 ? pow(1.0 - part, 1.6) : 0.0;
}

static double drive(double x, double gain) { return tanh(x * gain) / tanh(gain); }

static double hz(double semitones_from_c0)
{
	return C0_HZ * pow(2.0, semitones_from_c0 / 12.0);
}

// the chords of the zones, three notes as semitones from c, and the bass
// root under them. c minor, a flat major, f minor, then g minor for the
// tension of the core
struct chord { int note[3]; int root; };
static const struct chord CHORDS[4] = {
	{ { 0, 3, 7 }, 0 }, { { 8, 12, 15 }, 8 }, { { 5, 8, 12 }, 5 }, { { 7, 10, 14 }, 7 } };

static struct mixer {
	snd_pcm_t *pcm;
	pthread_t thread;
	pthread_mutex_t lock;
	int running, muted;
	enum { MODE_CARD, MODE_CLOCK } mode;
	double clock;                     // seconds of sound made so far
	double origin;                    // the mixer clock at run time zero
	double wall;                      // the wall clock when the mixer started
	double layer[LAYER_COUNT];
	double target[LAYER_COUNT];
	struct chord chord;
	// the oscillators of the music and their filters
	double ph_bass, ph_bass2, ph_pad[9], ph_kick;
	struct filter f_bass, f_pad, f_hat, f_hat_open, f_echo_l, f_echo_r;
	double echo[ECHO_MAX];
	int echo_at;
} snd;

static double wall_now(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

void sound_layer(enum layer which, double target)
{
	if (which < 0 || which >= LAYER_COUNT)
		return;
	pthread_mutex_lock(&snd.lock);
	snd.target[which] = target < 0.0 ? 0.0 : (target > 1.0 ? 1.0 : target);
	pthread_mutex_unlock(&snd.lock);
}

void sound_zone(int zone)
{
	pthread_mutex_lock(&snd.lock);
	snd.chord = CHORDS[zone < 0 ? 0 : zone > 3 ? 3 : zone];
	pthread_mutex_unlock(&snd.lock);
}

void sound_mute(int on)
{
	pthread_mutex_lock(&snd.lock);
	snd.muted = on;
	pthread_mutex_unlock(&snd.lock);
}

int sound_muted(void) { return snd.muted; }

// the clock the game follows. the card never stops once started, so a
// moment of the mix is heard that many seconds after the start on the wall
// clock. the mixer runs ahead by the buffer, which is what the lead is for
static double mixer_now(void)
{
	double t = wall_now() - snd.wall;
	return t < 0.0 ? 0.0 : t;
}

double sound_now(void)
{
	pthread_mutex_lock(&snd.lock);
	double t = mixer_now() - snd.origin;
	pthread_mutex_unlock(&snd.lock);
	return t;
}

// the run stands at run_time, a whole number of bars into the level. the
// origin is set on the bar the mixer is in, so that the sixteenths of the
// run and the sixteenths of the mixer are the same grid
void sound_restart(double run_time)
{
	pthread_mutex_lock(&snd.lock);
	double bar = floor(mixer_now() / BAR) * BAR;
	snd.origin = bar - run_time;
	pthread_mutex_unlock(&snd.lock);
}

double sound_next_step(double after)
{
	return ceil((after + SOUND_LEAD) / STEP) * STEP;
}

double sound_beat_phase(double t)
{
	double beats = t / BEAT;
	return beats - floor(beats);
}

// ---------------------------------------------------------------- the music
// the patterns are sixteen steps, a bar. a rest is -1, otherwise semitones
// above the root of the zone
static const int KICK[STEPS_PER_BAR]  = { 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0 };
static const int HAT[STEPS_PER_BAR]   = { 0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,1 };
static const int BASS[2 * STEPS_PER_BAR] = {
	-1, 0,-1, 0, -1, 0, 0,-1,  -1, 0,-1, 0,  3,-1, 0,-1,
	-1, 0,-1, 0, -1, 0, 0,-1,  -1, 0,-1,12,  -1, 0,10,-1,
};

static double music(double t)
{
	int step = (int)floor(t / STEP);
	double in_step = t - step * STEP;
	int in_bar = step % STEPS_PER_BAR;
	int in_two = step % (2 * STEPS_PER_BAR);
	double out = 0.0;

	if (snd.layer[LAYER_KICK] > 0.001 && KICK[in_bar]) {
		// the pitch falls from a knock to a thump in the first instants
		double env = envelope(in_step, 0.0005, 0.26);
		double pitch = 48.0 + 130.0 * exp(-in_step * 38.0);
		snd.ph_kick += pitch / SOUND_RATE;
		double click = envelope(in_step, 0.0002, 0.006);
		out += (drive(sin(2 * M_PI * snd.ph_kick) * env, 1.8) * 0.95 + noise() * click * 0.12)
			* snd.layer[LAYER_KICK];
	} else {
		snd.ph_kick = 0.0;
	}

	if (snd.layer[LAYER_HAT] > 0.001) {
		// the off beats first. past half the level, every sixteenth joins,
		// quieter, and the last one of the bar is left open
		double level = snd.layer[LAYER_HAT];
		double hat = 0.0;
		if (HAT[in_bar]) {
			double env = envelope(in_step, 0.0003, in_bar == 15 ? 0.16 : 0.045);
			hat += highpass(&snd.f_hat, noise(), 7000.0, 0.8) * env * 0.5;
		} else if (level > 0.5) {
			double env = envelope(in_step, 0.0003, 0.03);
			hat += highpass(&snd.f_hat_open, noise(), 9000.0, 0.8) * env * 0.28 * (level - 0.5) * 2.0;
		}
		out += hat * (level < 0.5 ? level * 2.0 : 1.0);
	}

	if (snd.layer[LAYER_BASS] > 0.001) {
		int note = BASS[in_two];
		if (note >= 0) {
			double freq = hz(12.0 * OCTAVE_BASS + snd.chord.root + note);
			snd.ph_bass += freq / SOUND_RATE;
			snd.ph_bass2 += freq * 0.5 / SOUND_RATE;
			double env = envelope(in_step, 0.002, 0.2);
			// the cut opens with every note, the acid trick
			double cut = 120.0 + 1400.0 * env * env;
			double raw = saw(snd.ph_bass) * 0.7 + square(snd.ph_bass2) * 0.4;
			out += drive(lowpass(&snd.f_bass, raw, cut, 1.3) * env, 1.6) * 0.55
				* snd.layer[LAYER_BASS];
		}
	}

	if (snd.layer[LAYER_PAD] > 0.001) {
		// three notes, three saws each, spread a few cents apart, under a
		// cut that breathes over eleven seconds
		double pad = 0.0;
		for (int n = 0; n < 3; n++) {
			double freq = hz(12.0 * OCTAVE_PAD + snd.chord.note[n]);
			static const double detune[3] = { 0.996, 1.0, 1.005 };
			for (int d = 0; d < 3; d++) {
				snd.ph_pad[n * 3 + d] += freq * detune[d] / SOUND_RATE;
				pad += saw(snd.ph_pad[n * 3 + d]);
			}
		}
		double cut = 500.0 + 350.0 * sin(2 * M_PI * t / 11.0);
		out += lowpass(&snd.f_pad, pad / 9.0, cut, 0.4) * 0.42 * snd.layer[LAYER_PAD];
	}

	return out;
}

// ---------------------------------------------------------------- the mixer
static void fill(short *buffer, int frames)
{
	pthread_mutex_lock(&snd.lock);
	for (int i = 0; i < LAYER_COUNT; i++) {
		// a layer slides in over a second and a half, a jump sounds like a
		// fault
		double delta = snd.target[i] - snd.layer[i];
		double step = (double)frames / SOUND_RATE / LAYER_SLIDE;
		if (fabs(delta) <= step)
			snd.layer[i] = snd.target[i];
		else
			snd.layer[i] += delta > 0 ? step : -step;
	}
	int tap_left = (int)(ECHO_STEPS_LEFT * STEP * SOUND_RATE);
	int tap_right = (int)(ECHO_STEPS_RIGHT * STEP * SOUND_RATE);
	for (int n = 0; n < frames; n++) {
		double t = snd.clock + n / (double)SOUND_RATE;
		double send = 0.0;
		double mix = music(t);
		// one delay line, read at two taps, one per ear
		int at = snd.echo_at;
		double back_l = snd.echo[(at - tap_left + ECHO_MAX) % ECHO_MAX];
		double back_r = snd.echo[(at - tap_right + ECHO_MAX) % ECHO_MAX];
		snd.echo[at] = send * ECHO_SEND + back_l * ECHO_FEEDBACK;
		snd.echo_at = (at + 1) % ECHO_MAX;
		double left = mix + lowpass(&snd.f_echo_l, back_l, ECHO_CUT, ECHO_Q);
		double right = mix + lowpass(&snd.f_echo_r, back_r, ECHO_CUT, ECHO_Q);
		left = drive(left * SOUND_MASTER, DRIVE_OUT);
		right = drive(right * SOUND_MASTER, DRIVE_OUT);
		if (snd.muted)
			left = right = 0.0;
		buffer[n * 2] = (short)(left * SAMPLE_PEAK);
		buffer[n * 2 + 1] = (short)(right * SAMPLE_PEAK);
	}
	snd.clock += frames / (double)SOUND_RATE;
	pthread_mutex_unlock(&snd.lock);
}

// the thread receives the mixer it feeds, because that is the one thing a
// thread start routine is allowed to receive
static void *run(void *state)
{
	struct mixer *mixer = state;
	short buffer[SOUND_CHUNK * 2];
	while (mixer->running) {
		fill(buffer, SOUND_CHUNK);
		snd_pcm_sframes_t wrote = snd_pcm_writei(mixer->pcm, buffer, SOUND_CHUNK);
		if (wrote < 0) {
			// the card ran dry, the machine was busy. it starts again, and
			// the mixer jumps to where the wall clock is, so that the beat
			// stays with the picture instead of falling behind it by the gap
			snd_pcm_recover(mixer->pcm, (int)wrote, 1);
			pthread_mutex_lock(&mixer->lock);
			mixer->clock = wall_now() - mixer->wall;
			pthread_mutex_unlock(&mixer->lock);
		}
	}
	return NULL;
}

int sound_open(void)
{
	memset(&snd, 0, sizeof snd);
	pthread_mutex_init(&snd.lock, NULL);
	snd.chord = CHORDS[0];
	snd.wall = wall_now();
	// a test run that must stay silent, or a machine without a card, keeps
	// the wall clock so that the game still runs in time
	snd.mode = MODE_CLOCK;
	if (getenv("TEC_SILENT"))
		return 1;
	if (snd_pcm_open(&snd.pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
		return 0;
	if (snd_pcm_set_params(snd.pcm, SND_PCM_FORMAT_S16_LE,
			SND_PCM_ACCESS_RW_INTERLEAVED, 2, SOUND_RATE, 1, SOUND_LATENCY) < 0) {
		snd_pcm_close(snd.pcm);
		snd.pcm = NULL;
		return 0;
	}
	snd.running = 1;
	if (pthread_create(&snd.thread, NULL, run, &snd) != 0) {
		snd.running = 0;
		snd_pcm_close(snd.pcm);
		snd.pcm = NULL;
		return 0;
	}
	snd.mode = MODE_CARD;
	return 1;
}

void sound_close(void)
{
	if (!snd.running)
		return;
	snd.running = 0;
	pthread_join(snd.thread, NULL);
	if (snd.pcm) {
		snd_pcm_drop(snd.pcm);
		snd_pcm_close(snd.pcm);
	}
}
