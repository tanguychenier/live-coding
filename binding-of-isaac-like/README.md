<div align="center">

<img src="banniere.png" alt="binding-of-isaac-like" width="100%">

[![watch the stream](https://img.shields.io/badge/watch%20the%20stream-YouTube-FF0000?logo=youtube&logoColor=white)](https://youtu.be/-rA3q0Ju6_c) [![LÖVE 11.5](https://img.shields.io/badge/L%C3%96VE-11.5-e64998?logo=love&logoColor=white)](https://love2d.org) [![Lua 5.1](https://img.shields.io/badge/Lua-5.1-2C2D72?logo=lua&logoColor=white)](https://www.lua.org) [![MIT](https://img.shields.io/badge/licence-MIT-green.svg)](../LICENSE)

</div>

A Binding of Isaac-like: one floor of twelve rooms generated from a seed, a key hidden in a dead end, a locked boss door, five kinds of monster and a boss that charges and fires a cross.

Written from an empty file during a single live session, 747 lines in 2 h 14:
**https://youtu.be/-rA3q0Ju6_c**

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
| `zqsd` / `wasd` | walk |
| arrow keys | shoot |
| `r` | replay the same floor |
| `n` | a new floor |
| `escape` | quit |

The seed is printed in the corner: the same seed always builds the same floor,
which is what makes a run reproducible.

## How the floor is built

Rooms grow outward from the centre, and a candidate cell is **rejected when it
already touches two rooms**. That single rule is what produces branches, and
therefore the dead ends where the boss and the key can be hidden. A
breadth-first pass then measures how far each room sits from the start, so the
boss lands on the farthest dead end and the key on the second farthest.

## Credits

The sprites come from a prototype I wrote in 2019, kept in
[game_prototype_lua](https://github.com/tanguychenier/game_prototype_lua).

The sound effects are synthesised rather than sampled: nothing to license, and
each one is tuned to the game.

Stream music: Scott Buckley, [scottbuckley.com.au](https://www.scottbuckley.com.au)
(CC BY 4.0). Outro: AIRGLOW, "Memory Bank" (CC BY 3.0).
