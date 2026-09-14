#ifndef TEXT_H
#define TEXT_H

// we speak to the player, and without a library: a five by seven font, one
// byte per column, drawn like everything else

// how long a notice stays on screen, and how high above the bottom
#define NOTICE_SECONDS 5.0
#define NOTICE_UP      34

#define GLYPH_W    5
#define GLYPH_H    7
#define TEXT_SCALE 2           // one font pixel is this many view pixels
#define TEXT_COLOR 0xe8e4da
#define TEXT_SHADOW 0x101318

// one dot of the font: is this letter lit there?
int glyph_bit(char c, int column, int line);
// written from x, y in the view, top left. gives back the width written.
int draw_text(int x, int y, const char *text, unsigned int color, int scale);
// the same, centred on the width of the view
void draw_text_centered(int y, const char *text, unsigned int color, int scale);

#endif