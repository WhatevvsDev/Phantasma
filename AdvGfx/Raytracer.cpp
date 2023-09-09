#include "Raytracer.h"
#include "Math.h"
#include "Common.h"
#include "LogUtility.h"
#include "IOUtility.h"
#include "Compute.h"

#include <GLFW/glfw3.h>

#include <utility>

struct Tri 
{ 
    glm::vec3 vertex0, vertex1, vertex2; 
    glm::vec3 centroid;
};

struct Ray 
{ 
    glm::vec3 O, D; float t = 1e30f; 
};

struct AABB
{
    glm::vec3 min;
    glm::vec3 max;
};

struct BVHNode
{
    AABB aabb;
    uint left_first, tri_count;
    bool isLeaf() { return tri_count > 0; }
};

 #define N 2048

// TODO: Swap triangle to bouding box centroid, instead of vertex centroid :)

void UpdateNodeBounds( uint nodeIdx );

void Subdivide( uint nodeIdx );

// application data
Tri tris[N];
uint triIdx[N];
BVHNode bvhNode[N * 2];
uint rootNodeIdx = 0, nodes_used = 1;

// functions

void IntersectTri( Ray& ray, const Tri& tri )
{
	const glm::vec3 edge1 = tri.vertex1 - tri.vertex0;
	const glm::vec3 edge2 = tri.vertex2 - tri.vertex0;
	const glm::vec3 h = cross( ray.D, edge2 );
	const float a = dot( edge1, h );
	if (a > -0.0001f && a < 0.0001f) return; // ray parallel to triangle
	const float f = 1 / a;
	const glm::vec3 s = ray.O - tri.vertex0;
	const float u = f * dot( s, h );
	if (u < 0 || u > 1) return;
	const glm::vec3 q = cross( s, edge1 );
	const float v = f * dot( ray.D, q );
	if (v < 0 || u + v > 1) return;
	const float t = f * dot( edge2, q );
	if (t > 0.0001f) ray.t = fmin( ray.t, t );
}

bool IntersectAABB( const Ray& ray, const glm::vec3 bmin, const glm::vec3 bmax )
{
	float tx1 = (bmin.x - ray.O.x) / ray.D.x, tx2 = (bmax.x - ray.O.x) / ray.D.x;
	float tmin = fmin( tx1, tx2 ), tmax = fmax( tx1, tx2 );
	float ty1 = (bmin.y - ray.O.y) / ray.D.y, ty2 = (bmax.y - ray.O.y) / ray.D.y;
	tmin = fmax( tmin, fmin( ty1, ty2 ) ), tmax = fmin( tmax, fmax( ty1, ty2 ) );
	float tz1 = (bmin.z - ray.O.z) / ray.D.z, tz2 = (bmax.z - ray.O.z) / ray.D.z;
	tmin = fmax( tmin, fmin( tz1, tz2 ) ), tmax = fmin( tmax, fmax( tz1, tz2 ) );
	return tmax >= tmin && tmin < ray.t && tmax > 0;
}

void intersect_bvh( Ray& ray, const uint nodeIdx )
{
	BVHNode& node = bvhNode[nodeIdx];
	if (!IntersectAABB( ray, node.aabb.min, node.aabb.max )) return;
	if (node.isLeaf())
	{
		for (uint i = 0; i < node.tri_count; i++ )
			IntersectTri( ray, tris[triIdx[node.left_first + i]] );
	}
	else
	{
		intersect_bvh( ray, node.left_first );
		intersect_bvh( ray, node.left_first + 1 );
	}
}

void BuildBVH()
{
	// populate triangle index array
	for (int i = 0; i < N; i++) triIdx[i] = i;
	// calculate triangle centroids for partitioning
	for (int i = 0; i < N; i++)
		tris[i].centroid = (tris[i].vertex0 + tris[i].vertex1 + tris[i].vertex2) * 0.3333f;
	// assign all triangles to root node
	BVHNode& root = bvhNode[rootNodeIdx];
	root.left_first = 0, root.tri_count = N;
	UpdateNodeBounds( rootNodeIdx );
	// subdivide recursively
	Subdivide( rootNodeIdx );
}

void UpdateNodeBounds( uint nodeIdx )
{
	BVHNode& node = bvhNode[nodeIdx];
	node.aabb.min = glm::vec3( 1e30f );
	node.aabb.max = glm::vec3( -1e30f );
	for (uint first = node.left_first, i = 0; i < node.tri_count; i++)
	{
		uint leafTriIdx = triIdx[first + i];
		Tri& leafTri = tris[leafTriIdx];
		node.aabb.min = glm::min( node.aabb.min, leafTri.vertex0 ),
		node.aabb.min = glm::min( node.aabb.min, leafTri.vertex1 ),
		node.aabb.min = glm::min( node.aabb.min, leafTri.vertex2 ),
		node.aabb.max = glm::max( node.aabb.max, leafTri.vertex0 ),
		node.aabb.max = glm::max( node.aabb.max, leafTri.vertex1 ),
		node.aabb.max = glm::max( node.aabb.max, leafTri.vertex2 );
	}
}

void Subdivide( uint nodeIdx )
{
	// terminate recursion
	BVHNode& node = bvhNode[nodeIdx];
	if (node.tri_count <= 2) return;
	// determine split axis and position
	glm::vec3 extent = node.aabb.max - node.aabb.min;
	int axis = 0;
	if (extent.y > extent.x) axis = 1;
	if (extent.z > extent[axis]) axis = 2;
	float splitPos = node.aabb.min[axis] + extent[axis] * 0.5f;
	// in-place partition
	int i = node.left_first;
	int j = i + node.tri_count - 1;
	while (i <= j)
	{
		if (tris[triIdx[i]].centroid[axis] < splitPos)
			i++;
		else
			std::swap( triIdx[i], triIdx[j--] );
	}
	// abort split if one of the sides is empty
	int leftCount = i - node.left_first;
	if (leftCount == 0 || leftCount == node.tri_count) return;
	// create child nodes
	int leftChildIdx = nodes_used++;
	int rightChildIdx = nodes_used++;

	bvhNode[leftChildIdx].left_first = node.left_first;
	bvhNode[leftChildIdx].tri_count = leftCount;
	bvhNode[rightChildIdx].left_first = i;
	bvhNode[rightChildIdx].tri_count = node.tri_count - leftCount;

	node.left_first = leftChildIdx;
	node.tri_count = 0;

	UpdateNodeBounds( leftChildIdx );
	UpdateNodeBounds( rightChildIdx );
	// recurse
	Subdivide( leftChildIdx );
	Subdivide( rightChildIdx );
}

struct SceneData
{
	uint resolution[2];
	uint tri_count;
	uint dummy;
	glm::vec3 cam_pos{ 0.0f };
} sceneData;

// Temporary scuffed input
static bool move_w = false;
static bool move_a = false;
static bool move_s = false;
static bool move_d = false;
static bool move_space = false;
static bool move_lctrl = false;

void Raytracer::key_input(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if(action != GLFW_PRESS && action != GLFW_RELEASE)
		return;

	bool is_pressed = (action == GLFW_PRESS);

	switch(key)
	{
		case GLFW_KEY_W:
		move_w = is_pressed;
		break;
		case GLFW_KEY_A:
		move_a = is_pressed;
		break;
		case GLFW_KEY_S:
		move_s = is_pressed;
		break;
		case GLFW_KEY_D:
		move_d = is_pressed;
		break;
		case GLFW_KEY_SPACE:
		move_space = is_pressed;
		break;
		case GLFW_KEY_LEFT_CONTROL:
		move_lctrl = is_pressed;
		break;
	}
}

void Raytracer::mouse_button_input(GLFWwindow* window, int button, int action, int mods)
{

}
void Raytracer::cursor_input(GLFWwindow* window, double xpos, double ypos)
{

}

namespace Raytracer
{
    glm::vec3 camPos( 0, 0, -18 );
    glm::vec3 p0( -1, 1, -15 ), p1( 1, 1, -15 ), p2( -1, -1, -15 );
    Ray ray;

	ComputeOperation* perform_raytracing;

	void init()
	{
		for (int i = 0; i < N; i++)
        {
            glm::vec3 r0( RandomFloat(), RandomFloat(), RandomFloat() );
            glm::vec3 r1( RandomFloat(), RandomFloat(), RandomFloat() );
            glm::vec3 r2( RandomFloat(), RandomFloat(), RandomFloat() );
            tris[i].vertex0 = r0 * 9.0f - glm::vec3( 5 );
            tris[i].vertex1 = tris[i].vertex0 + r1;
            tris[i].vertex2 = tris[i].vertex0 + r2;
        }	

        BuildBVH();
		Compute::create_kernel(get_current_directory_path() + "/raytrace_tri.cl", "raytrace");
	}

	void raytrace(int width, int height, uint32_t* buffer)
	{
		int moveHor = (move_d) - (move_a);
		int moveVer = (move_space) - (move_lctrl);
		int moveWard = (move_w) - (move_s);

		glm::vec3 dir = {moveHor, moveVer, moveWard};
		glm::normalize(dir);

		sceneData.cam_pos += dir;

		sceneData.resolution[0] = width;
		sceneData.resolution[1] = height;
		sceneData.tri_count = N;

		ComputeOperation("raytrace_tri.cl")
			//.write({bvhNode, 4096 *  sizeof(BVHNode)})
			//.write({triIdx, 2048 * sizeof(uint)})
			.read({buffer, (size_t)(width * height) * sizeof(uint32_t)})
			.write({&sceneData, sizeof(SceneData)})
			.write({tris, 2048 * sizeof(Tri)})
			.global_dispatch({width, height, 1})
			.execute();
    }
}