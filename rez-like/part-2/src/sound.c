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
#define OCTAVE_ARP       4
#define OCTAVE_LEAD      5
// the mixer holds this many hits at once. eight shots and their eight
// impacts overlap the locks of the next chain
#define VOICES           48
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

static double bandpass(struct filter *filter, double in, double cut, double resonance)
{
	double g = 2.0 * sin(M_PI * (cut < FILTER_TOP ? cut : FILTER_TOP) / SOUND_RATE);
	filter->low += g * filter->band;
	double high = in - filter->low - resonance * filter->band;
	filter->band += g * high;
	return filter->band;
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

// the minor pentatonic, five notes that never clash, whatever order they
// land in. a degree past five goes up an octave
static const int PENTATONIC[5] = { 0, 3, 5, 7, 10 };

static double degree_hz(int octave, int degree)
{
	if (degree < 0)
		degree = 0;
	return hz(12.0 * (octave + degree / 5) + PENTATONIC[degree % 5]);
}

// ---------------------------------------------------------------- the state
struct voice {
	enum hit kind;
	double start;      // on the mixer clock
	int note;
	int busy;
	double ph[3];      // up to three oscillators
	struct filter a, b;
};

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
	enum { MODE_CARD, MODE_PULL, MODE_CLOCK } mode;
	double clock;                     // seconds of sound made so far
	double origin;                    // the mixer clock at run time zero
	double wall;                      // the wall clock when the mixer started
	double layer[LAYER_COUNT];
	double target[LAYER_COUNT];
	struct chord chord;
	struct voice voices[VOICES];
	// the oscillators of the music and their filters
	double ph_bass, ph_bass2, ph_pad[9], ph_arp, ph_lead, ph_lead2, ph_kick;
	struct filter f_bass, f_pad, f_hat, f_hat_open, f_clap, f_lead, f_echo_l, f_echo_r;
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

int sound_offline(void) { return snd.mode == MODE_PULL; }

// the clock the game follows. the card never stops once started, so a
// moment of the mix is heard that many seconds after the start on the wall
// clock. the mixer runs ahead by the buffer, which is what the lead is for
static double mixer_now(void)
{
	double t = snd.clock;
	if (snd.mode != MODE_PULL)
		t = wall_now() - snd.wall;
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
	for (int i = 0; i < VOICES; i++)
		snd.voices[i].busy = 0;
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

void sound_hit(enum hit which, int note, double when)
{
	pthread_mutex_lock(&snd.lock);
	// the oldest voice gives way when they are all busy, so that a chain
	// of eight never falls silent
	int slot = -1;
	double oldest = 1e30;
	for (int i = 0; i < VOICES; i++) {
		if (!snd.voices[i].busy) {
			slot = i;
			break;
		}
		if (snd.voices[i].start < oldest) {
			oldest = snd.voices[i].start;
			slot = i;
		}
	}
	struct voice *voice = &snd.voices[slot];
	memset(voice, 0, sizeof *voice);
	voice->kind = which;
	voice->start = when + snd.origin;
	voice->note = note;
	voice->busy = 1;
	pthread_mutex_unlock(&snd.lock);
}

// ---------------------------------------------------------------- the hits
// each one is a small instrument with its own filters, so that eight at
// once do not fight over one state. t is the age of the hit in seconds
static double play_hit(struct voice *voice, double t, double *echo_send)
{
	switch (voice->kind) {
	case HIT_LOCK: {
		// a ping, a sine at the degree with a tick of noise on the front
		double freq = degree_hz(OCTAVE_HIT + 1, voice->note);
		voice->ph[0] += freq / SOUND_RATE;
		double env = envelope(t, 0.002, 0.11);
		double tick = envelope(t, 0.0005, 0.012);
		double out = sin(2 * M_PI * voice->ph[0]) * env * 0.34;
		*echo_send += out * 0.5;
		return out + noise() * tick * 0.12;
	}
	case HIT_SHOT: {
		// a saw that falls an octave in a tenth of a second, through a band
		// that follows it. the note is where it lands
		double freq = degree_hz(OCTAVE_HIT, voice->note);
		double sweep = 1.0 + 1.0 * envelope(t, 0.0, 0.10);
		voice->ph[0] += freq * sweep / SOUND_RATE;
		double env = envelope(t, 0.003, 0.18);
		double out = bandpass(&voice->a, saw(voice->ph[0]), freq * sweep * 2.0, 0.5) * env;
		*echo_send += out * 0.5;
		return out * 0.5;
	}
	case HIT_KILL: {
		// the impact, a burst of noise and a bell at the degree, with a
		// thump under it. a chain of eight is a rising line of bells
		double freq = degree_hz(OCTAVE_HIT, voice->note);
		voice->ph[0] += freq / SOUND_RATE;
		voice->ph[1] += freq * 2.01 / SOUND_RATE;
		double burst = envelope(t, 0.0005, 0.04);
		double bell = envelope(t, 0.002, 0.45);
		double thump = envelope(t, 0.001, 0.14);
		double out = bandpass(&voice->a, noise(), 3000.0, 0.6) * burst * 0.7
			+ (sin(2 * M_PI * voice->ph[0]) + 0.4 * sin(2 * M_PI * voice->ph[1])) * bell * 0.3
			+ sin(2 * M_PI * (60.0 + 50.0 * thump) * t) * thump * 0.5;
		*echo_send += out * 0.6;
		return out;
	}
	case HIT_HURT: {
		// a thud and a fall, low, the sound of losing something
		double env = envelope(t, 0.002, 0.45);
		double fall = 160.0 - 100.0 * (1.0 - env);
		voice->ph[0] += fall / SOUND_RATE;
		double out = sin(2 * M_PI * voice->ph[0]) * env * 0.7
			+ lowpass(&voice->a, noise(), 600.0, 0.5) * envelope(t, 0.001, 0.12) * 0.8;
		return drive(out, 2.0) * 0.8;
	}
	case HIT_GATE: {
		// the chord itself, swelling, for the gates and the end of a zone
		double env = envelope(t, 0.12, 1.4);
		double out = 0.0;
		for (int i = 0; i < 3; i++) {
			voice->ph[i] += hz(12.0 * OCTAVE_PAD + snd.chord.note[i]) / SOUND_RATE;
			out += saw(voice->ph[i]);
		}
		out = lowpass(&voice->a, out / 3.0, 400.0 + 2400.0 * env, 0.6) * env;
		*echo_send += out * 0.5;
		return out * 0.6;
	}
	case HIT_WARN: {
		// two short square blips, up then down, the sound of a thing taking
		// aim. it warns, so it must cut through everything
		double freq = degree_hz(OCTAVE_LEAD, t < 0.08 ? 4 : 2);
		voice->ph[0] += freq / SOUND_RATE;
		double env = t < 0.08 ? envelope(t, 0.001, 0.07) : envelope(t - 0.08, 0.001, 0.07);
		return square(voice->ph[0]) * env * 0.14;
	}
	case HIT_RISE: {
		// the launch, two bars of noise climbing through a band that opens,
		// and a saw that climbs an octave under it, swelling all the way
		double along = t / (RISE_BARS * BAR);
		if (along > 1.0)
			along = 1.0;
		double swell = along * along;
		double freq = hz(12.0 * OCTAVE_PAD + snd.chord.root) * (1.0 + along);
		voice->ph[0] += freq / SOUND_RATE;
		voice->ph[1] += freq * 1.005 / SOUND_RATE;
		double cut = 300.0 + 6000.0 * swell;
		double out = bandpass(&voice->a, noise(), cut, 0.5) * swell * 0.6
			+ lowpass(&voice->b, (saw(voice->ph[0]) + saw(voice->ph[1])) * 0.5, cut, 0.8) * swell * 0.5;
		*echo_send += out * 0.3;
		return out;
	}
	case HIT_BOSS: {
		// the stinger of a phase change, a fall of noise and a sub hit
		double env = envelope(t, 0.01, 1.6);
		double cut = 6000.0 * env * env + 80.0;
		double out = bandpass(&voice->a, noise(), cut, 0.3) * env * 0.8
			+ sin(2 * M_PI * (45.0 + 40.0 * env) * t) * envelope(t, 0.005, 0.8) * 0.8;
		*echo_send += out * 0.3;
		return drive(out, 1.5) * 0.9;
	}
	default:
		return 0.0;
	}
}

static double hit_length(enum hit kind)
{
	switch (kind) {
	case HIT_LOCK: return 0.15;
	case HIT_SHOT: return 0.22;
	case HIT_KILL: return 0.5;
	case HIT_HURT: return 0.5;
	case HIT_GATE: return 1.6;
	case HIT_WARN: return 0.17;
	case HIT_BOSS: return 1.7;
	case HIT_RISE: return RISE_BARS * BAR;
	default:       return 0.5;
	}
}

// ---------------------------------------------------------------- the music
// the patterns are sixteen steps, a bar. a rest is -1, otherwise semitones
// above the root of the zone
static const int KICK[STEPS_PER_BAR]  = { 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0 };
static const int HAT[STEPS_PER_BAR]   = { 0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,1 };
static const int CLAP[STEPS_PER_BAR]  = { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 };
static const int BASS[2 * STEPS_PER_BAR] = {
	-1, 0,-1, 0, -1, 0, 0,-1,  -1, 0,-1, 0,  3,-1, 0,-1,
	-1, 0,-1, 0, -1, 0, 0,-1,  -1, 0,-1,12,  -1, 0,10,-1,
};
static const int LEAD[2 * STEPS_PER_BAR] = {
	 0,-1, 7,-1,  3,-1,-1, 7,  -1, 0,-1,10,  -1, 7,-1,-1,
	 0,-1, 7,-1,  3,-1,-1,12,  -1,10,-1, 7,  -1, 3,-1, 0,
};
// the arp climbs the chord over two octaves, one note a sixteenth
static const int ARP[8] = { 0, 1, 2, 3, 4, 5, 4, 2 };

static double music(double t, double *echo_send)
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

	if (snd.layer[LAYER_ARP] > 0.001) {
		// the chord climbing, plucked, a note a sixteenth, sent to the echo
		int pick = ARP[step % 8];
		double freq = hz(12.0 * (OCTAVE_ARP + pick / 3) + snd.chord.note[pick % 3]);
		snd.ph_arp += freq / SOUND_RATE;
		double env = envelope(in_step, 0.001, 0.09);
		double out = square(snd.ph_arp) * env * 0.16 * snd.layer[LAYER_ARP];
		*echo_send += out * 0.9;
		out += out;
	}

	if (snd.layer[LAYER_LEAD] > 0.001) {
		double level = snd.layer[LAYER_LEAD];
		if (CLAP[in_bar]) {
			// a clap is three bursts of noise a few ms apart, then a tail
			double env = envelope(in_step, 0.0005, 0.02) + envelope(in_step - 0.01, 0.0005, 0.02)
				+ envelope(in_step - 0.02, 0.0005, 0.14);
			out += bandpass(&snd.f_clap, noise(), 1500.0, 0.5) * env * 0.5 * level;
		}
		int note = LEAD[in_two];
		if (note >= 0) {
			double freq = hz(12.0 * OCTAVE_LEAD + snd.chord.root + note);
			snd.ph_lead += freq / SOUND_RATE;
			snd.ph_lead2 += freq * 1.007 / SOUND_RATE;
			double env = envelope(in_step, 0.003, 0.16);
			double out = (saw(snd.ph_lead) + saw(snd.ph_lead2)) * 0.5;
			out = lowpass(&snd.f_lead, drive(out, 3.0), 900.0 + 2600.0 * env, 0.7) * env * 0.28 * level;
			*echo_send += out * 0.6;
			out += out;
		}
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
		double mix = music(t, &send);
		for (int i = 0; i < VOICES; i++) {
			struct voice *voice = &snd.voices[i];
			if (!voice->busy)
				continue;
			double age = t - voice->start;
			if (age < 0.0)
				continue;
			if (age > hit_length(voice->kind)) {
				voice->busy = 0;
				continue;
			}
			mix += play_hit(voice, age, &send);
		}
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

void sound_render(short *stereo, int frames)
{
	fill(stereo, frames);
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
	// a rendering to a file does without the card, the caller pulls the
	// mix. a test run that must stay silent, or a machine without a card,
	// keeps the wall clock so that the game still runs in time
	if (getenv("TEC_WAV")) {
		snd.mode = MODE_PULL;
		return 1;
	}
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
