<div align="center">

<img src="banniere.png" alt="wolfenstein-like" width="100%">

[![watch the stream](https://img.shields.io/badge/watch%20the%20stream-YouTube-FF0000?logo=youtube&logoColor=white)](https://youtu.be/crEy4UbZ3qs) [![C](https://img.shields.io/badge/C-gnu17-00599C?logo=c&logoColor=white)](https://en.cppreference.com/w/c) [![X11](https://img.shields.io/badge/X11-Xlib-1d2a3a?logo=x.org&logoColor=white)](https://www.x.org) [![MIT](https://img.shields.io/badge/licence-MIT-green.svg)](../LICENSE)

</div>

> **Written across 4 live sessions**, 4 of them done. Each part has its own folder, with the code exactly as it stood when that stream ended.

| | | |
|---|---|---|
| **[Part 1](part-1/)** | the engine itself, one ray per column, and the walls stand up | [watch](https://youtu.be/4QS_gnC-Vz8) · 1 h 09 |
| **[Part 2](part-2/)** | textures on the walls, then on the floor and the ceiling | [watch](https://youtu.be/SeKs2vCeFUk) · 1 h 42 |
| **[Part 3](part-3/)** | the world comes out of a file, several kinds of wall, doors, an exit | [watch](https://youtu.be/YKUUJJBlzc4) · 3 h 43 |
| **[Part 4](part-4/)** | it becomes a game, things that see you and come, a fight, a story, and the level played to the end | [video 1](https://youtu.be/ib2nzm4y6aI) · [video 2](https://youtu.be/crEy4UbZ3qs) · 6 h 11 |

The Keep, a first-person engine written in C, in 2.5D, the way Wolfenstein 3D did it. One ray per column of the screen, walking a grid of characters. No game engine and no framework. X11 hands over a window and a block of memory, and every pixel after that is ours.



**[Play it in the browser](https://tanguychenier.github.io/live-coding/keep/)**

**[Read the course](doc/the-keep-le-cours.pdf)**, 328 pages in French. Every chapter of the four sessions, with what to understand, the maths worked by hand, the code to write and what the screen shows.

<div align="center">

<img src="part-4/captures/jeu-1.png" width="49%" alt="the game running">
<img src="part-4/captures/jeu-2.png" width="49%" alt="the game running">
<img src="part-4/captures/jeu-3.png" width="49%" alt="the game running">
<img src="part-4/captures/jeu-4.png" width="49%" alt="the game running">

</div>

## Running it

```sh
cd part-4
gcc -Wall -Wextra -O2 -Iinclude -o game src/demo.c src/fight.c src/light.c src/main.c src/render.c src/screen.c src/sound.c src/sprite.c src/story.c src/text.c src/texture.c src/thing.c src/trigger.c src/world.c -lX11 -lasound -lm && ./game
```

A C compiler, the X11 headers and the ALSA headers (`libx11-dev` and `libasound2-dev` on Debian and Ubuntu). Nothing else.

| | |
|---|---|
| arrow up / down | walk forward and back |
| arrow left / right | turn |
| `a` `d` | step sideways |
| `escape` | quit |

No engine, no library, no framework. X11 gives a window and a block of memory;
everything else is in `main.c`, and one line of gcc builds it.

## How the walls are drawn

The technique is not new. It is how Wolfenstein 3D drew its corridors in 1992,
and it has been written up many times since. What is here is the
implementation, written from an empty file.

The world is a grid of characters, and a wall is anything that is not a dot.
For each of the 320 columns of the screen, one ray leaves the eye and walks
that grid, always stepping across whichever grid line is nearest, so no
square is ever missed and none is visited twice. Where the ray meets a wall,
the distance it covered decides how tall that wall is drawn. Near is tall, far
is short, and a column of pixels is all it takes.

The distance is measured on the camera plane rather than from the eye. Measured
from the eye, the corners of a straight wall are farther away than its middle,
and the wall bends into a fishbowl.

## The two greys

A wall met on a north-south grid line is drawn a shade darker than one met on
an east-west line. Without that difference every corner of the level
disappears, because two walls at right angles end up the same colour. It costs
one line and it is the whole reason the rooms read as rooms.

## Walking into things

Each axis is tested on its own, so a shoulder against a wall keeps sliding
instead of stopping the player dead. The test looks a fifth of a square ahead
of where the player actually is (the nose), otherwise you walk until your
eyes are inside the texture.
