<div align="center">

<img src="banniere.png" alt="street-fighter-like" width="100%">

[![watch the stream](https://img.shields.io/badge/watch%20the%20stream-YouTube-FF0000?logo=youtube&logoColor=white)](https://youtu.be/gJOMl3DYnhg) [![LÖVE 11.5](https://img.shields.io/badge/L%C3%96VE-11.5-e64998?logo=love&logoColor=white)](https://love2d.org) [![Lua 5.1](https://img.shields.io/badge/Lua-5.1-2C2D72?logo=lua&logoColor=white)](https://www.lua.org) [![MIT](https://img.shields.io/badge/licence-MIT-green.svg)](../LICENSE)

</div>

Four fighters, two stages, three rounds. The opponent runs a state machine, and left alone the game plays both sides itself.

Written from an empty file during a single live session, 665 lines in 2 h 09:
**https://youtu.be/gJOMl3DYnhg**

**[Play it in the browser](https://tanguychenier.github.io/live-coding/street-fighter/)**

<div align="center">

<img src="captures/jeu-1.png" width="49%" alt="the game running">
<img src="captures/jeu-2.png" width="49%" alt="the game running">
<img src="captures/jeu-3.png" width="49%" alt="the game running">
<img src="captures/jeu-4.png" width="49%" alt="the game running">

</div>

## Running it

```sh
love .
```

[LÖVE 11.5](https://love2d.org) is the only thing to install.

| | |
|---|---|
| `q` `d` | walk |
| `z` | jump |
| back away from the opponent | block |
| `j` | light attack |
| `k` | heavy attack |
| `enter` | menus |
| `d` on the title | arcade demo |

Best of three rounds, sixty seconds each. Block by holding away from the
opponent: you still take a quarter of the damage, which is what keeps blocking
from being a free answer to everything.

## How the opponent thinks

The opponent runs a small state machine, re-evaluated every tenth of a second
or so: **close** when out of reach, **hit** when in reach, **block** or **back**
when the other one starts a swing. Every branch carries a bit of randomness and
a reaction delay, because an opponent that answers in one frame reads as a
machine and stops being fun.

The same state machine drives *both* fighters in demo mode, which is what an
arcade cabinet does when nobody is playing.

## The blow that lands

An attack animation is three frames for the ninja and eight for the samurai, so
the active window is expressed as a fraction of the animation rather than a
frame number: the hit lands between 40 % and 85 % of the swing, whatever the
fighter. On impact the whole game freezes for four to seven hundredths of a
second. That freeze is most of what makes a hit feel heavy.

## Credits

The four fighters are Calciumtrice's animated sprite sets (Ninja, Samurai,
Ranger, Shieldmaiden), [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/),
from [OpenGameArt](https://opengameart.org/users/calciumtrice). The parallax
backgrounds are by Luis Zuno ([@ansimuz](https://ansimuz.com)), CC0.

The sound effects are synthesised rather than sampled: nothing to license, and
each one is tuned to the game.

Stream music: Scott Buckley, [scottbuckley.com.au](https://www.scottbuckley.com.au)
(CC BY 4.0). Outro: AIRGLOW, "Memory Bank" (CC BY 3.0).
