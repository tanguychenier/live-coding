#include <alsa/asoundlib.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "sound.h"

// ---------------------------------------------------------------- the parts
// everything below is made of four pieces: an oscillator, an envelope, a
// filter and a clock. a guitar tone is a rich wave, hard clipping, and a
// narrow band: none of those three is the string.

#define BPM      162.0
#define BEAT     (60.0 / BPM)
#define SIXTEEN  (BEAT / 4.0)
#define ROOT     41.20                // e1, low enough to feel it

struct filter { double low, band; };

static double lowpass(struct filter *f, double in, double cut, double q)
{
	double g = 2.0 * sin(M_PI * (cut < 20000 ? cut : 20000) / SOUND_RATE);
	f->low += g * f->band;
	double high = in - f->low - q * f->band;
	f->band += g * high;
	return f->low;
}

static double bandpass(struct filter *f, double in, double cut, double q)
{
	double g = 2.0 * sin(M_PI * (cut < 20000 ? cut : 20000) / SOUND_RATE);
	f->low += g * f->band;
	double high = in - f->low - q * f->band;
	f->band += g * high;
	return f->band;
}

static double saw(double phase) { return 2.0 * (phase - floor(phase + 0.5)); }
static double square(double phase) { return phase - floor(phase) < 0.5 ? 1.0 : -1.0; }

static unsigned int seed = 1;
static double noise(void)
{
	seed = seed * 1103515245u + 12345u;
	return (double)((seed >> 9) & 0xffff) / 32768.0 - 1.0;
}

static double envelope(double t, double attack, double decay)
{
	if (t < 0.0)
		return 0.0;
	if (t < attack)
		return t / attack;
	double x = (t - attack) / decay;
	return x < 1.0 ? pow(1.0 - x, 1.4) : 0.0;
}

// an amplifier runs out of headroom at some point, and tanh has exactly that
// shape
static double drive(double x, double gain) { return tanh(x * gain) / tanh(gain); }

// ---------------------------------------------------------------- the voices
// a mouth is a buzz pushed through three resonances. move them apart and the
// throat stops being a human one, which is what we want for the thing.
struct throat { struct filter a, b, c; double phase; };

static double voice(struct throat *v, double t, double pitch,
					double f1, double f2, double f3, double growl)
{
	v->phase += pitch * (1.0 + 0.04 * sin(2 * M_PI * 5.0 * t)) / SOUND_RATE;
	double p = v->phase - floor(v->phase);
	double pulse = p < 0.35 ? sin(M_PI * p / 0.35)
	                        : -0.25 * (1.0 - (p - 0.35) / 0.65);
	double source = pulse + growl * noise() * 0.5;
	return (bandpass(&v->a, source, f1, 0.08)
		+ bandpass(&v->b, source, f2, 0.10) * 0.7
		+ bandpass(&v->c, source, f3, 0.14) * 0.4) * 0.5;
}

// ---------------------------------------------------------------- the state
#define VOICES 8

struct shot {
	enum sfx kind;
	double start;      // when it began, in seconds since the mixer started
	double distance;
	int busy;
};

static struct mixer {
	snd_pcm_t *pcm;
	pthread_t thread;
	pthread_mutex_t lock;
	int running;
	int muted;
	double clock;                     // seconds since the mixer started
	double layer[LAYER_COUNT];        // where each layer is now
	double target[LAYER_COUNT];       // where it is going
	struct shot shots[VOICES];
	struct filter cab1, cab2, cabhi, bass, pad, hit, room, sfxf;
	struct filter reach_of, rail, blast;
	struct throat mouth[2];
	double ph_pad, ph_pad2, ph_bass, ph_riff, ph_riff2;
	double echo[SOUND_RATE / 2];
	int echo_at;
} snd;

void sound_layer(enum layer which, double target)
{
	if (!snd.running || which < 0 || which >= LAYER_COUNT)
		return;
	pthread_mutex_lock(&snd.lock);
	snd.target[which] = target < 0.0 ? 0.0 : (target > 1.0 ? 1.0 : target);
	pthread_mutex_unlock(&snd.lock);
}

void sound_mute(int on)
{
	if (!snd.running)
		return;
	pthread_mutex_lock(&snd.lock);
	snd.muted = on;
	pthread_mutex_unlock(&snd.lock);
}

int sound_muted(void) { return snd.muted; }

void sound_play(enum sfx which, double distance)
{
	if (!snd.running)
		return;
	pthread_mutex_lock(&snd.lock);
	for (int i = 0; i < VOICES; i++)
		if (!snd.shots[i].busy) {
			snd.shots[i] = (struct shot){ which, snd.clock, distance, 1 };
			break;
		}
	pthread_mutex_unlock(&snd.lock);
}

// ---------------------------------------------------------------- one shots
static double one_shot(struct shot *s, double t)
{
	switch (s->kind) {
	case SFX_SHOT: {
		double e1 = envelope(t, 0.0004, 0.05), e2 = envelope(t, 0.001, 0.14);
		return bandpass(&snd.sfxf, noise(), 2600, 0.5) * e1 * 1.4
			+ lowpass(&snd.hit, noise(), 420 + 900 * e2, 0.6) * e2;
	}
	case SFX_PUNCH: {
		double e = envelope(t, 0.004, 0.10);
		return lowpass(&snd.sfxf, noise(), 300 + 500 * e, 0.9) * e * 1.2;
	}
	case SFX_IMPACT: {
		double e = envelope(t, 0.0005, 0.12);
		return bandpass(&snd.sfxf, noise(), 1500 + 3000 * e, 0.4) * e
			+ sin(2 * M_PI * 320 * t) * envelope(t, 0.001, 0.35) * 0.25;
	}
	case SFX_DOOR: {
		// this is a door that slides, not a filter that whistles. a station
		// door is a rail that rubs, which is a narrow steady band, a mass
		// that moves, which is low steady noise, and a knock at the end. none
		// of the three sweeps, and that is what makes them bearable.
		double walked = t < 0.10 ? t / 0.10
			: (t < 0.95 ? 1.0 : fmax(0.0, (1.15 - t) / 0.20));
		double sfx = bandpass(&snd.rail, noise(), 1100, 0.9) * walked * 0.30
			+ lowpass(&snd.reach_of, noise(), 170, 0.5) * walked * 0.50;
		if (t > 1.05) {
			double e = envelope(t - 1.05, 0.001, 0.22);
			sfx += (sin(2 * M_PI * 88 * (t - 1.05)) * 0.7
				+ noise() * 0.3) * e * 0.8;
		}
		return sfx;
	}
	case SFX_STEP: {
		// a step on sheet metal. without it the character slides as if on
		// ice, because the sound of the step is what gives him weight. it has
		// to stay under everything else, almost inaudible on its own.
		double e = envelope(t, 0.002, 0.085);
		return (lowpass(&snd.reach_of, noise(), 240, 0.4) * 0.8
			+ bandpass(&snd.rail, noise(), 2600, 1.4) * 0.12) * e * 0.55;
	}
	case SFX_BLAST: {
		// this one is close, so we hear the crack, then the blast, then the
		// debris
		double crack = envelope(t, 0.0008, 0.09);
		double blast = envelope(t, 0.006, 1.10);
		double debris = envelope(t, 0.12, 1.60);
		return bandpass(&snd.rail, noise(), 2200, 0.5) * crack * 0.85
			+ lowpass(&snd.blast, noise(), 70 + 150 * blast, 0.35)
			  * blast * 1.05
			+ sin(2 * M_PI * (50 + 34 * blast) * t) * blast * 0.45
			+ bandpass(&snd.reach_of, noise(), 900, 1.3) * debris * 0.16;
	}
	case SFX_PICKUP: {
		double e = envelope(t, 0.002, 0.22);
		return (sin(2 * M_PI * 880 * t) + sin(2 * M_PI * 1320 * t) * 0.5) * e * 0.3;
	}
	case SFX_SHRIEK:
		return voice(&snd.mouth[0], t, 210 + 320 * envelope(t, 0.05, 1.4),
			720, 1600 + 900 * t, 3200, 0.7) * envelope(t, 0.02, 1.5);
	case SFX_GROWL:
		return voice(&snd.mouth[1], t, 78 + 22 * sin(2 * M_PI * 0.5 * t),
			420 + 120 * t, 1100, 2600, 0.35) * envelope(t, 0.25, 2.3) * 0.8;
	case SFX_HURT: {
		double e = envelope(t, 0.003, 0.30);
		return (sin(2 * M_PI * 140 * t) * 0.6 + noise() * 0.4) * e * 0.7;
	}
	case SFX_DIE:
		return voice(&snd.mouth[1], t, 120 - 60 * t, 500, 900, 2200, 0.5)
			* envelope(t, 0.02, 1.8);
	default:
		return 0.0;
	}
}

static double shot_length(enum sfx kind)
{
	switch (kind) {
	case SFX_SHRIEK: return 1.6;
	case SFX_GROWL:  return 2.6;
	case SFX_DIE:    return 1.9;
	case SFX_DOOR:   return 1.4;
	case SFX_STEP:   return 0.16;
	case SFX_BLAST:  return 2.4;
	default:         return 0.6;
	}
}

// ---------------------------------------------------------------- the music
static const int RIFF[32] = {
	0,0,0,1, 0,0,3,0, 0,0,0,1, 0,3,1,0,
	0,0,0,1, 0,0,5,0, 0,0,0,3, 1,0,1,0,
};
// the phrygian scale, because it is the one that sounds like a threat
static const double SCALE[8] = { 0, 1, 3, 5, 7, 8, 10, 12 };
static const int KICK[16]  = { 1,0,1,0, 0,1,0,0, 1,0,1,0, 0,1,0,1 };
static const int SNARE[16] = { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 };
static const int PULSE[8]  = { 0, 0, 3, 0, 4, 0, 2, 1 };

static double music(double t)
{
	int six = (int)(t / SIXTEEN);
	double in_six = t - six * SIXTEEN;
	double out = 0.0;

	// the breath is two detuned saws, heavily filtered, and it sounds like
	// the station idling. we stay above 20 hz, because below that a saw
	// becomes a flutter that wears you down. e2 is low enough to sit under
	// everything and high enough to be a sound.
	if (snd.layer[LAYER_BREATH] > 0.001) {
		double f = ROOT * 2.0;
		snd.ph_pad += f / SOUND_RATE;
		snd.ph_pad2 += f * 1.004 / SOUND_RATE;
		double pad = (saw(snd.ph_pad) + saw(snd.ph_pad2)) * 0.5;
		// the sweep is wide and slow, because a drone that never moves is
		// just noise
		double cut = 210 + 150 * sin(2 * M_PI * t / 17.0);
		out += lowpass(&snd.pad, pad, cut, 1.0)
			* 0.16 * snd.layer[LAYER_BREATH];
	}

	// the pulse plays eighth notes, and its filter opens as it heats up
	if (snd.layer[LAYER_PULSE] > 0.001) {
		int step = (int)(t / (BEAT / 2.0));
		double since = t - step * (BEAT / 2.0);
		double f = ROOT * 2.0 * pow(2.0, SCALE[PULSE[step % 8]] / 12.0);
		snd.ph_bass += f / SOUND_RATE;
		double e = envelope(since, 0.004, 0.22);
		out += lowpass(&snd.bass, square(snd.ph_bass) * e,
			300 + 800 * e, 0.7) * 0.5 * snd.layer[LAYER_PULSE];
	}

	// the metal layer is struck noise and a kick. it is not a drum kit, it is
	// a bulkhead.
	if (snd.layer[LAYER_METAL] > 0.001) {
		double m = 0.0;
		if (KICK[six % 16]) {
			double e = envelope(in_six, 0.001, 0.13);
			m += drive(sin(2 * M_PI * (52 + 55 * e) * in_six) * e, 2.2) * 0.85;
		}
		if (SNARE[six % 16]) {
			double e = envelope(in_six, 0.0008, 0.17);
			m += bandpass(&snd.hit, noise(), 1900, 0.5) * e * 0.45;
		}
		double e = envelope(in_six, 0.0004, 0.035);
		m += noise() * e * ((six % 4 == 0) ? 0.16 : 0.09);
		out += m * snd.layer[LAYER_METAL];
	}

	// the drive layer is the riff, a rich wave that is clipped hard and sent
	// through a cabinet
	if (snd.layer[LAYER_DRIVE] > 0.001) {
		double f = ROOT * pow(2.0, SCALE[RIFF[six % 32]] / 12.0);
		snd.ph_riff += f / SOUND_RATE;
		snd.ph_riff2 += f * 2.0 * 1.003 / SOUND_RATE;
		double gate = envelope(in_six, 0.0015, SIXTEEN * 0.92);
		double pick = envelope(in_six, 0.0004, 0.008) * 0.35;
		double amp = drive((saw(snd.ph_riff) * 0.8
			+ saw(snd.ph_riff2) * 0.45 + pick) * gate, 9.0);
		double cab = lowpass(&snd.cab1, amp, 3800, 0.9);
		cab -= lowpass(&snd.cab2, cab, 95, 0.9);
		cab += bandpass(&snd.cabhi, amp, 2100, 0.6) * 0.25;
		out += cab * 0.30 * snd.layer[LAYER_DRIVE];
	}
	return out;
}

// ---------------------------------------------------------------- the mixer
static void fill(short *buffer, int frames)
{
	pthread_mutex_lock(&snd.lock);
	for (int i = 0; i < LAYER_COUNT; i++) {
		// a layer slides in over about a second, because a sudden jump sounds
		// like a fault
		double d = snd.target[i] - snd.layer[i];
		double step = (double)frames / SOUND_RATE / 1.0;
		if (fabs(d) <= step)
			snd.layer[i] = snd.target[i];
		else
			snd.layer[i] += d > 0 ? step : -step;
	}
	for (int n = 0; n < frames; n++) {
		double t = snd.clock + n / (double)SOUND_RATE;
		double mix = music(t);
		for (int i = 0; i < VOICES; i++) {
			struct shot *s = &snd.shots[i];
			if (!s->busy)
				continue;
			double age = t - s->start;
			if (age > shot_length(s->kind)) {
				s->busy = 0;
				continue;
			}
			// distance takes the top off the sound and turns it down, the way
			// a room does
			double far = 1.0 / (1.0 + s->distance * s->distance * 0.05);
			mix += one_shot(s, age) * far;
		}
		double back = snd.echo[snd.echo_at];
		snd.echo[snd.echo_at] = mix + back * 0.24;
		snd.echo_at = (snd.echo_at + 1) % (int)(SIXTEEN * 3 * SOUND_RATE);
		double wet = mix + lowpass(&snd.room, back, 3000, 0.8) * 0.2;
		wet *= SOUND_MASTER;
		if (snd.muted)
			wet = 0.0;
		if (wet > 1.0) wet = 1.0;
		if (wet < -1.0) wet = -1.0;
		short v = (short)(wet * 30000);
		buffer[n * 2] = v;
		buffer[n * 2 + 1] = v;
	}
	snd.clock += frames / (double)SOUND_RATE;
	pthread_mutex_unlock(&snd.lock);
}

// the thread receives the mixer it feeds, because that is the one thing a
// thread start routine is allowed to receive
static void *run(void *state)
{
	struct mixer *m = state;
	short buffer[SOUND_CHUNK * 2];
	while (m->running) {
		fill(buffer, SOUND_CHUNK);
		snd_pcm_sframes_t wrote = snd_pcm_writei(m->pcm, buffer, SOUND_CHUNK);
		if (wrote < 0)
			snd_pcm_recover(m->pcm, (int)wrote, 1);
	}
	return NULL;
}

int sound_open(void)
{
	memset(&snd, 0, sizeof snd);
	// we create the lock first, before any return, because a game started
	// without sound still locks this mutex the first time a sound is asked
	// for.
	pthread_mutex_init(&snd.lock, NULL);
	// a test run must never make noise on the machine, so a game launched for
	// a screenshot stays silent
	if (getenv("TEC_SILENT"))
		return 0;
	if (snd_pcm_open(&snd.pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
		return 0;
	if (snd_pcm_set_params(snd.pcm, SND_PCM_FORMAT_S16_LE,
			SND_PCM_ACCESS_RW_INTERLEAVED, 2, SOUND_RATE, 1, 120000) < 0) {
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
