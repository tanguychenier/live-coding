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

static void build(void)
{
	memset(shapes, 0, sizeof shapes);
	octahedron(&shapes[SHAPE_OCTA]);
	cube(&shapes[SHAPE_CUBE]);
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
