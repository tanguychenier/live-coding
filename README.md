<div align="center">

<img src="banniere.svg" alt="TEC live coding" width="100%">

**One game, one stream, from an empty file.**

[![LÖVE 11.5](https://img.shields.io/badge/L%C3%96VE-11.5-e64998?logo=love&logoColor=white)](https://love2d.org)
[![Lua 5.1](https://img.shields.io/badge/Lua-5.1-2C2D72?logo=lua&logoColor=white)](https://www.lua.org)
[![TEC on YouTube](https://img.shields.io/badge/TEC-YouTube-FF0000?logo=youtube&logoColor=white)](https://www.youtube.com/@tanguy_tec)
[![MIT](https://img.shields.io/badge/licence-MIT-green.svg)](LICENSE)

</div>

Every folder here is a **complete game, written from scratch during a single
live session** on [TEC](https://www.youtube.com/@tanguy_tec). No pre-written
code, no cuts, no second take. The commit lands when the stream ends.

Each game keeps its own README: what it is, how to play it, and how the
interesting part works.

## Latest

<table>
<tr>
<td width="45%">

<a href="street-fighter-like"><img src="street-fighter-like/captures/jeu-1.png" width="100%" alt="street-fighter-like"></a>

</td>
<td valign="top">

### [street-fighter-like](street-fighter-like)

Four fighters, two stages, three rounds. The opponent runs a state machine, and left alone the game plays both sides itself.

Lua, LÖVE 2D, 665 lines, 2 h 09 of stream.

[Watch it being written](https://youtu.be/gJOMl3DYnhg)

</td>
</tr>
</table>

## Everything else

| game | what it is | lines | stream | |
|---|---|---:|---:|---|
| [binding-of-isaac-like](binding-of-isaac-like) | A Binding of Isaac-like: one floor of twelve rooms generated from a seed, a key hidden in a dead end, a locked boss door, five kinds of monster and a boss that charges and fires a cross. | 747 | 2 h 14 | [stream](https://youtu.be/-rA3q0Ju6_c) |

## Running any of them

```sh
cd <the game folder>
love .
```

[LÖVE 11.5](https://love2d.org) is the only thing to install.

## Licence

MIT, see [LICENSE](LICENSE). The sprites come from a prototype I wrote in 2019,
kept in [game_prototype_lua](https://github.com/tanguychenier/game_prototype_lua).
