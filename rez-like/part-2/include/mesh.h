#ifndef MESH_H
#define MESH_H

#include "draw.h"
#include "vec.h"

// a shape is points, edges between them and a few faces, drawn as lines of
// light and panes of glass, moved and turned as a whole
#define MESH_POINTS_MAX  64
#define MESH_EDGES_MAX   128
#define MESH_FACES_MAX   64

struct mesh {
	struct vec point[MESH_POINTS_MAX];
	int edge[MESH_EDGES_MAX][2];
	int face[MESH_FACES_MAX][3];
	int points, edges, faces;
};

// the shapes of the game, all built from numbers, none loaded
enum shape { SHAPE_OCTA, SHAPE_CUBE, SHAPE_DIAMOND, SHAPE_MANTA, SHAPE_RING,
	     SHAPE_TETRA, SHAPE_SPINDLE, SHAPE_BOLT, SHAPE_ICOSA, SHAPE_HERO,
	     SHAPE_COUNT };

// the points of the pilot that move, by name, so that the arms can be
// thrown forward and the legs kicked without counting
enum hero_point { HERO_HAND_R = 17, HERO_HAND_L = 18, HERO_ELBOW_R = 15,
		  HERO_ELBOW_L = 16, HERO_FOOT_R = 21, HERO_FOOT_L = 22,
		  HERO_KNEE_R = 19, HERO_KNEE_L = 20, HERO_HEAD_FRONT = 4 };

const struct mesh *mesh_of(enum shape shape);

// where a point of the shape ends up in the world, given where the shape
// is, how big it is, and how it is turned around each axis
struct vec mesh_place(struct vec local, struct vec at, double size,
		      double yaw, double pitch, double roll);

// draws the whole shape as lines, at a place, a size and a turn. the faces
// are drawn when asked, as faint glass of the same colour
void mesh_draw(const struct camera *cam, const struct mesh *mesh, struct vec at,
	       double size, double yaw, double pitch, double roll,
	       struct light colour, int with_faces);

#endif
