<div align="center">

<img src="../banniere.png" alt="rez-like" width="100%">

[![watch the stream](https://img.shields.io/badge/watch%20the%20stream-YouTube-FF0000?logo=youtube&logoColor=white)](https://youtu.be/j4NwurDLC5c) [![C](https://img.shields.io/badge/C-gnu17-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c) [![X11](https://img.shields.io/badge/X11-Xlib-1d2a3a?logo=x.org&logoColor=white)](https://www.x.org) [![MIT](https://img.shields.io/badge/licence-MIT-green.svg)](../../LICENSE)

</div>

> **Part 1 of 2**, written during one live session. [All the parts](../) of this game.

AXON, a rail shooter drawn in lines of light, in the spirit of Rez, written in C. A wireframe 3D engine, a rail, a tunnel, targets marked up to eight, and a music the program computes, on which every shot plays a note.

Written from an empty file during a single live session, 3529 lines in 7 h 03.
**https://youtu.be/j4NwurDLC5c**

<div align="center">

<img src="captures/jeu-1.png" width="49%" alt="the game running">
<img src="captures/jeu-2.png" width="49%" alt="the game running">
<img src="captures/jeu-3.png" width="49%" alt="the game running">
<img src="captures/jeu-4.png" width="49%" alt="the game running">

</div>

## Running it

```sh
gcc -Wall -Wextra -O2 -Iinclude -o axon src/demo.c src/draw.c src/hero.c src/level.c src/main.c src/mesh.c src/palette.c src/particle.c src/player.c src/rail.c src/screen.c src/sound.c src/thing.c src/tunnel.c -lX11 -lasound -lm && ./axon
```

A C compiler, the X11 headers and the ALSA headers (`libx11-dev` and `libasound2-dev` on Debian and Ubuntu). Nothing else.


