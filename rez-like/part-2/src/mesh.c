#include <math.h>
#include <string.h>

#include "mesh.h"

static struct mesh shapes[SHAPE_COUNT];
static int built;

static void point(struct mesh *mesh, double x, double y, double z)
{
	if (mesh->points < MESH_POINTS_MAX)
		mesh->point[mesh->points++] = vec(x, y, z);
}

static void edge(struct mesh *mesh, int first, int second)
{
	if (mesh->edges < MESH_EDGES_MAX) {
		mesh->edge[mesh->edges][0] = first;
		mesh->edge[mesh->edges][1] = second;
		mesh->edges++;
	}
}

static void face(struct mesh *mesh, int first, int second, int third)
{
	if (mesh->faces < MESH_FACES_MAX) {
		mesh->face[mesh->faces][0] = first;
		mesh->face[mesh->faces][1] = second;
		mesh->face[mesh->faces][2] = third;
		mesh->faces++;
	}
}

// six points, one on each axis, joined into eight triangles. the simplest
// solid that still reads as a body when it turns
static void octahedron(struct mesh *mesh)
{
	point(mesh, 1, 0, 0); point(mesh, -1, 0, 0);
	point(mesh, 0, 1, 0); point(mesh, 0, -1, 0);
	point(mesh, 0, 0, 1); point(mesh, 0, 0, -1);
	for (int first = 0; first < 2; first++)
		for (int second = 2; second < 4; second++) {
			edge(mesh, first, second);
			for (int third = 4; third < 6; third++) {
				edge(mesh, first, third);
				edge(mesh, second, third);
				face(mesh, first, second, third);
			}
		}
}

static void cube(struct mesh *mesh)
{
	for (int i = 0; i < 8; i++)
		point(mesh, (i & 1) ? 1 : -1, (i & 2) ? 1 : -1, (i & 4) ? 1 : -1);
	for (int i = 0; i < 8; i++)
		for (int bit = 1; bit < 8; bit <<= 1)
			if (!(i & bit))
				edge(mesh, i, i | bit);
	// two faces only, the top and the bottom, so the cube reads as a box
	// without turning into a block of light
	face(mesh, 2, 3, 7); face(mesh, 2, 7, 6);
	face(mesh, 0, 1, 5); face(mesh, 0, 5, 4);
}

// a long crystal, sharp at both ends, with a square waist
static void diamond(struct mesh *mesh)
{
	point(mesh, 0, 0, 2.2); point(mesh, 0, 0, -2.2);
	point(mesh, 1, 0, 0); point(mesh, 0, 1, 0); point(mesh, -1, 0, 0); point(mesh, 0, -1, 0);
	for (int i = 2; i < 6; i++) {
		int next = i == 5 ? 2 : i + 1;
		edge(mesh, 0, i);
		edge(mesh, 1, i);
		edge(mesh, i, next);
		face(mesh, 0, i, next);
		face(mesh, 1, i, next);
	}
}

// a wing, wide and thin, with a body line down the middle. it flies sideways
// across the tunnel and it is the enemy that dodges
static void manta(struct mesh *mesh)
{
	point(mesh, 0, 0, 1.4);          // nose
	point(mesh, 0, 0, -0.8);         // tail
	point(mesh, 2.6, 0.1, -0.4);     // right wing tip
	point(mesh, -2.6, 0.1, -0.4);    // left wing tip
	point(mesh, 1.2, -0.3, 0.2);     // right shoulder
	point(mesh, -1.2, -0.3, 0.2);    // left shoulder
	point(mesh, 0, 0.5, 0.0);        // back
	edge(mesh, 0, 4); edge(mesh, 4, 2); edge(mesh, 2, 1); edge(mesh, 1, 3); edge(mesh, 3, 5);
	edge(mesh, 5, 0); edge(mesh, 0, 6); edge(mesh, 6, 1); edge(mesh, 4, 6); edge(mesh, 5, 6);
	edge(mesh, 4, 1); edge(mesh, 5, 1);
	face(mesh, 0, 4, 2); face(mesh, 0, 3, 5); face(mesh, 4, 1, 2); face(mesh, 5, 3, 1);
}

// a ring of twelve points, the gate the tunnel throws at the player
static void ring(struct mesh *mesh)
{
	for (int i = 0; i < 12; i++) {
		double angle = 2 * M_PI * i / 12;
		point(mesh, cos(angle), sin(angle), 0);
	}
	for (int i = 0; i < 12; i++) {
		edge(mesh, i, (i + 1) % 12);
		if (i % 3 == 0)
			edge(mesh, i, (i + 6) % 12);
	}
}

// four points, the smallest solid there is, the drone of the swarm
static void tetrahedron(struct mesh *mesh)
{
	point(mesh, 1, 1, 1); point(mesh, 1, -1, -1); point(mesh, -1, 1, -1); point(mesh, -1, -1, 1);
	for (int first = 0; first < 4; first++)
		for (int second = first + 1; second < 4; second++)
			edge(mesh, first, second);
	face(mesh, 0, 1, 2); face(mesh, 0, 1, 3); face(mesh, 0, 2, 3); face(mesh, 1, 2, 3);
}

// a turret, two tips and a ring of six between them. it hovers, takes aim
// and fires, so it needs a shape the eye can tell from the rest
static void spindle(struct mesh *mesh)
{
	point(mesh, 0, 1.5, 0); point(mesh, 0, -1.5, 0);
	for (int i = 0; i < 6; i++) {
		double angle = 2 * M_PI * i / 6;
		point(mesh, cos(angle), 0, sin(angle));
	}
	for (int i = 0; i < 6; i++) {
		int here = 2 + i, next = 2 + (i + 1) % 6;
		edge(mesh, 0, here);
		edge(mesh, 1, here);
		edge(mesh, here, next);
		face(mesh, 0, here, next);
	}
}

// what the turrets fire, a dart, sharp in front and finned behind
static void bolt(struct mesh *mesh)
{
	point(mesh, 0, 0, 1.6); point(mesh, 0, 0, -1.0);
	point(mesh, 0.35, 0, 0); point(mesh, 0, 0.35, 0);
	point(mesh, -0.35, 0, 0); point(mesh, 0, -0.35, 0);
	for (int i = 2; i < 6; i++) {
		int next = i == 5 ? 2 : i + 1;
		edge(mesh, 0, i);
		edge(mesh, 1, i);
		edge(mesh, i, next);
		face(mesh, 0, i, next);
	}
}

// twelve points on three golden rectangles, thirty edges, the core. an edge
// joins two points at the shortest distance there is on the solid
static void icosahedron(struct mesh *mesh)
{
	const double g = (1.0 + sqrt(5.0)) / 2.0;
	const double shape = 1.0 / sqrt(1.0 + g * g);
	point(mesh, -1, g, 0); point(mesh, 1, g, 0); point(mesh, -1, -g, 0); point(mesh, 1, -g, 0);
	point(mesh, 0, -1, g); point(mesh, 0, 1, g); point(mesh, 0, -1, -g); point(mesh, 0, 1, -g);
	point(mesh, g, 0, -1); point(mesh, g, 0, 1); point(mesh, -g, 0, -1); point(mesh, -g, 0, 1);
	for (int i = 0; i < mesh->points; i++)
		mesh->point[i] = scale(mesh->point[i], shape);
	const double short_edge = 2.0 * shape * 1.01;
	for (int first = 0; first < mesh->points; first++)
		for (int second = first + 1; second < mesh->points; second++)
			if (length(sub(mesh->point[first], mesh->point[second])) < short_edge)
				edge(mesh, first, second);
	for (int first = 0; first < mesh->points; first++)
		for (int second = first + 1; second < mesh->points; second++)
			for (int third = second + 1; third < mesh->points; third++)
				if (length(sub(mesh->point[first], mesh->point[second])) < short_edge
				    && length(sub(mesh->point[second], mesh->point[third])) < short_edge
				    && length(sub(mesh->point[first], mesh->point[third])) < short_edge)
					face(mesh, first, second, third);
}

// the pilot, flying prone, head forward, arms out like a glider, legs
// trailing. seen from behind and a little above, which is where the eye is.
// the head is a small solid of its own, the rest is the silhouette
static void hero(struct mesh *mesh)
{
	point(mesh, 0.00, 0.56, 1.05);    // 0 head top
	point(mesh, 0.18, 0.36, 1.05);    // 1 head right
	point(mesh, 0.00, 0.18, 1.05);    // 2 head bottom
	point(mesh, -0.18, 0.36, 1.05);   // 3 head left
	point(mesh, 0.00, 0.36, 1.26);    // 4 head front
	point(mesh, 0.00, 0.36, 0.88);    // 5 head back
	point(mesh, 0.00, 0.26, 0.82);    // 6 neck
	point(mesh, 0.36, 0.30, 0.72);    // 7 shoulder right
	point(mesh, -0.36, 0.30, 0.72);   // 8 shoulder left
	point(mesh, 0.30, 0.02, 0.70);    // 9 chest right
	point(mesh, -0.30, 0.02, 0.70);   // 10 chest left
	point(mesh, 0.20, 0.18, 0.05);    // 11 waist right
	point(mesh, -0.20, 0.18, 0.05);   // 12 waist left
	point(mesh, 0.22, 0.00, -0.05);   // 13 hip right
	point(mesh, -0.22, 0.00, -0.05);  // 14 hip left
	point(mesh, 0.90, 0.28, 0.60);    // 15 elbow right
	point(mesh, -0.90, 0.28, 0.60);   // 16 elbow left
	point(mesh, 1.40, 0.34, 0.85);    // 17 hand right
	point(mesh, -1.40, 0.34, 0.85);   // 18 hand left
	point(mesh, 0.16, 0.06, -0.55);   // 19 knee right
	point(mesh, -0.16, 0.06, -0.55);  // 20 knee left
	point(mesh, 0.12, 0.12, -1.00);   // 21 foot right
	point(mesh, -0.12, 0.12, -1.00);  // 22 foot left
	// the head
	edge(mesh, 0, 1); edge(mesh, 1, 2); edge(mesh, 2, 3); edge(mesh, 3, 0);
	edge(mesh, 4, 0); edge(mesh, 4, 1); edge(mesh, 4, 2); edge(mesh, 4, 3);
	edge(mesh, 5, 0); edge(mesh, 5, 1); edge(mesh, 5, 2); edge(mesh, 5, 3);
	// the body
	edge(mesh, 5, 6); edge(mesh, 6, 7); edge(mesh, 6, 8);
	edge(mesh, 7, 9); edge(mesh, 8, 10); edge(mesh, 9, 10);
	edge(mesh, 7, 11); edge(mesh, 8, 12); edge(mesh, 9, 11); edge(mesh, 10, 12); edge(mesh, 11, 12);
	edge(mesh, 11, 13); edge(mesh, 12, 14); edge(mesh, 13, 14);
	face(mesh, 7, 8, 12); face(mesh, 7, 12, 11); face(mesh, 11, 12, 14); face(mesh, 11, 14, 13);
	face(mesh, 7, 9, 11); face(mesh, 8, 12, 10);
	// the arms and the legs
	edge(mesh, 7, 15); edge(mesh, 15, 17); edge(mesh, 8, 16); edge(mesh, 16, 18);
	edge(mesh, 13, 19); edge(mesh, 19, 21); edge(mesh, 14, 20); edge(mesh, 20, 22); edge(mesh, 21, 22);
	face(mesh, 13, 14, 20); face(mesh, 13, 20, 19);
}

static void build(void)
{
	memset(shapes, 0, sizeof shapes);
	octahedron(&shapes[SHAPE_OCTA]);
	cube(&shapes[SHAPE_CUBE]);
	diamond(&shapes[SHAPE_DIAMOND]);
	manta(&shapes[SHAPE_MANTA]);
	ring(&shapes[SHAPE_RING]);
	tetrahedron(&shapes[SHAPE_TETRA]);
	spindle(&shapes[SHAPE_SPINDLE]);
	bolt(&shapes[SHAPE_BOLT]);
	icosahedron(&shapes[SHAPE_ICOSA]);
	hero(&shapes[SHAPE_HERO]);
	built = 1;
}

const struct mesh *mesh_of(enum shape shape)
{
	if (!built)
		build();
	return &shapes[shape];
}

// yaw turns around the vertical, pitch around the sideways axis, roll around
// the line of flight. applied in that order, the way a craft would move
struct vec mesh_place(struct vec local, struct vec at, double size,
		      double yaw, double pitch, double roll)
{
	double cy = cos(yaw), sy = sin(yaw);
	double cp = cos(pitch), sp = sin(pitch);
	double cr = cos(roll), sr = sin(roll);
	double x = local.x * cr - local.y * sr, y = local.x * sr + local.y * cr, z = local.z;
	double y2 = y * cp - z * sp, z2 = y * sp + z * cp;
	double x3 = x * cy + z2 * sy, z3 = -x * sy + z2 * cy;
	return add(at, scale(vec(x3, y2, z3), size));
}

void mesh_draw(const struct camera *cam, const struct mesh *mesh, struct vec at,
	       double size, double yaw, double pitch, double roll,
	       struct light colour, int with_faces)
{
	struct vec placed[MESH_POINTS_MAX];
	for (int i = 0; i < mesh->points; i++)
		placed[i] = mesh_place(mesh->point[i], at, size, yaw, pitch, roll);
	if (with_faces)
		for (int i = 0; i < mesh->faces; i++)
			draw_triangle(cam, placed[mesh->face[i][0]], placed[mesh->face[i][1]],
				      placed[mesh->face[i][2]], colour);
	for (int i = 0; i < mesh->edges; i++)
		draw_line(cam, placed[mesh->edge[i][0]], placed[mesh->edge[i][1]], colour);
}
