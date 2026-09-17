#ifndef DRAW_H
#define DRAW_H

#include "screen.h"

// the picture takes a new size. everything drawn after is that big
int draw_resize(int width, int height);
// how much bigger than the base size the picture is, for the sight, which
// is laid out in base pixels
double draw_scale(void);

#endif
