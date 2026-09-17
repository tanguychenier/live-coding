#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "draw.h"

unsigned int *view;
int view_width, view_height;

// how much bigger than the base size the picture is
static double scale_up = 1.0;

double draw_scale(void)
{
	return scale_up;
}

// the picture, as big as the window, made anew when the window changes
int draw_resize(int width, int height)
{
	if (width == view_width && height == view_height && view)
		return 1;
	free(view);
	view_width = width;
	view_height = height;
	scale_up = (double)height / VIEW_BASE_HEIGHT;
	view = calloc((size_t)width * height, sizeof *view);
	return view != NULL;
}
