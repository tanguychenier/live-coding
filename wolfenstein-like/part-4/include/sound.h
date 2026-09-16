#ifndef SOUND_H
#define SOUND_H

// the sound is calculated, like the textures. there is no file and no sample,
// only oscillators, an envelope, a resonant filter and a sequencer clock.
// alsa takes the frames we hand it and does nothing else.
#define SOUND_RATE      44100
// the output volume is low on purpose, because a game left running in the
// background must not make the room jump. 0.35 leaves room for the gunshots,
// which are the only sounds that should crack.
#define SOUND_MASTER    0.35
#define SOUND_CHUNK     512       // frames per write: short enough to stay in time

// the music is made of four layers in the same key and the same tempo, so
// they stack without any arranging. the game only turns them up and down, it
// never starts a new piece.
enum layer {
	LAYER_BREATH,     // the station idling. on from the third door
	LAYER_PULSE,      // a bass in eighths, when something is awake
	LAYER_METAL,      // struck noise, when it is a fight
	LAYER_DRIVE,      // the riff. this one is the fight itself
	LAYER_COUNT
};

enum sfx {
	SFX_SHOT, SFX_PUNCH, SFX_IMPACT, SFX_DOOR, SFX_PICKUP,
	SFX_SHRIEK, SFX_GROWL, SFX_HURT, SFX_DIE,
	// what blows up makes a sound, but the shake itself makes none. a shake
	// is the consequence of something, and that something is what makes the
	// noise.
	SFX_BLAST, SFX_STEP, SFX_COUNT
};

int  sound_open(void);
void sound_close(void);
// where the layer should sit, from 0 to 1. it slides there, it does not jump.
void sound_layer(enum layer which, double target);
// plays a sound once. distance makes it duller and quieter, the way a room
// does.
void sound_play(enum sfx which, double distance);
// turns the sound off or on, with the N key during the game
void sound_mute(int on);
int  sound_muted(void);

#endif
