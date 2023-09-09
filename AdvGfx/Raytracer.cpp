#include "Raytracer.h"
#include "Math.h"
#include "Common.h"

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
    uint leftFirst, triCount;
    bool isLeaf() { return triCount > 0; }
};

 #define N 512

Tri tris[N];
uint triIdx[N];
BVHNode bvhNode[N * 2];
uint rootNodeIdx = 0, nodesUsed = 1;

// TODO: Swap triangle to bouding box centroid, instead of vertex centroid :)

void UpdateNodeBounds( uint nodeIdx );
void Subdivide( uint nodeIdx );

void build_bvh()
{
    // populate triangle index array
	for (int i = 0; i < N; i++) triIdx[i] = i;
	// calculate triangle centroids for partitioning
	for (int i = 0; i < N; i++)
		tris[i].centroid = (tris[i].vertex0 + tris[i].vertex1 + tris[i].vertex2) * 0.3333f;
	// assign all triangles to root node
	BVHNode& root = bvhNode[rootNodeIdx];
	root.leftFirst = 0, root.triCount = N;
	UpdateNodeBounds( rootNodeIdx );
	// subdivide recursively
	Subdivide( rootNodeIdx );
}

void UpdateNodeBounds( uint nodeIdx )
{
    BVHNode& node = bvhNode[nodeIdx];
    node.aabb.min = glm::vec3( 1e30f );
    node.aabb.max = glm::vec3( -1e30f );
    for (uint first = node.leftFirst, i = 0; i < node.triCount; i++)
    {
        uint leafTriIdx = triIdx[first + i];
        Tri& leafTri = tris[leafTriIdx];

        node.aabb.min = glm::min( node.aabb.min, leafTri.vertex0 );
        node.aabb.min = glm::min( node.aabb.min, leafTri.vertex1 );
        node.aabb.min = glm::min( node.aabb.min, leafTri.vertex2 );
        node.aabb.max = glm::max( node.aabb.max, leafTri.vertex0 );
        node.aabb.max = glm::max( node.aabb.max, leafTri.vertex1 );
        node.aabb.max = glm::max( node.aabb.max, leafTri.vertex2 );
    }
}

void Subdivide( uint nodeIdx )
{
	// terminate recursion
	BVHNode& node = bvhNode[nodeIdx];
	if (node.triCount <= 2) return;
	// determine split axis and position
	glm::vec3 extent = node.aabb.max - node.aabb.min;
	int axis = 0;
	if (extent.y > extent.x) axis = 1;
	if (extent.z > extent[axis]) axis = 2;
	float splitPos = node.aabb.min[axis] + extent[axis] * 0.5f;
	// in-place partition
	int i = node.leftFirst;
	int j = i + node.triCount - 1;
	while (i <= j)
	{
		if (tris[triIdx[i]].centroid[axis] < splitPos)
			i++;
		else
			std::swap( triIdx[i], triIdx[j--] );
	}
	// abort split if one of the sides is empty
	int leftCount = i - node.leftFirst;
	if (leftCount == 0 || leftCount == node.triCount) return;
	// create child nodes
	int leftChildIdx = nodesUsed++;
	int rightChildIdx = nodesUsed++;
	bvhNode[leftChildIdx].leftFirst = node.leftFirst;
	bvhNode[leftChildIdx].triCount = leftCount;
	bvhNode[rightChildIdx].leftFirst = i;
	bvhNode[rightChildIdx].triCount = node.triCount - leftCount;
	node.leftFirst = leftChildIdx;
	node.triCount = 0;
	UpdateNodeBounds( leftChildIdx );
	UpdateNodeBounds( rightChildIdx );
	// recurse
	Subdivide( leftChildIdx );
	Subdivide( rightChildIdx );
}


void intersect_tri( Ray& ray, const Tri& tri )
{
    const glm::vec3 edge1 = tri.vertex1 - tri.vertex0;
    const glm::vec3 edge2 = tri.vertex2 - tri.vertex0;
    const glm::vec3 h = cross( ray.D, edge2 );
    const float a = dot( edge1, h );
    if (fabs(a) < 0.0001f) return; // ray parallel to triangle
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

bool intersect_aabb( const Ray& ray, const glm::vec3 bmin, const glm::vec3 bmax )
{
    float tx1 = (bmin.x - ray.O.x) * ray.D.x, tx2 = (bmax.x - ray.O.x) * ray.D.x;
	float tmin = fminf( tx1, tx2 ), tmax = fmaxf( tx1, tx2 );
	float ty1 = (bmin.y - ray.O.y) * ray.D.y, ty2 = (bmax.y - ray.O.y) * ray.D.y;
	tmin = fmaxf( tmin, fminf( ty1, ty2 ) ), tmax = fminf( tmax, fmaxf( ty1, ty2 ) );
	float tz1 = (bmin.z - ray.O.z) * ray.D.z, tz2 = (bmax.z - ray.O.z) * ray.D.z;
	tmin = fmaxf( tmin, fminf( tz1, tz2 ) ), tmax = fminf( tmax, fmaxf( tz1, tz2 ) );
	if (tmax >= tmin && tmin < ray.t && tmax > 0) return tmin; else return 1e30f;
}

void intersect_bvh( Ray& ray, const uint nodeIdx )
{
    BVHNode& node = bvhNode[nodeIdx];
    if (!intersect_aabb( ray, node.aabb.min, node.aabb.max )) return;

    if (node.isLeaf())
    {
        for (uint i = 0; i < node.triCount; i++ )
            intersect_tri( ray, tris[triIdx[node.leftFirst + i]] );
    }
    else
    {
        intersect_bvh( ray, node.leftFirst );
        intersect_bvh( ray, node.leftFirst + 1 );
    }
}

namespace Raytracer
{
    glm::vec3 camPos( 0, 0, -18 );
    glm::vec3 p0( -1, 1, -15 ), p1( 1, 1, -15 ), p2( -1, -1, -15 );
    Ray ray;

	void init()
	{
        LOGMSG(Log::MessageType::Debug, std::string("Size is: ") + std::to_string(sizeof(glm::vec3)));

		for (int i = 0; i < N; i++)
        {
            glm::vec3 r0( RandomFloat(), RandomFloat(), RandomFloat() );
            glm::vec3 r1( RandomFloat(), RandomFloat(), RandomFloat() );
            glm::vec3 r2( RandomFloat(), RandomFloat(), RandomFloat() );
            tris[i].vertex0 = r0 * 9.0f - glm::vec3( 5 );
            tris[i].vertex1 = tris[i].vertex0 + r1;
            tris[i].vertex2 = tris[i].vertex0 + r2;
        }

        build_bvh();
	}

	void raytrace(int width, int height, uint32_t* buffer)
	{
        for (int y = 0; y < height; y++) 
        {
            for (int x = 0; x < width; x++)
            {
                glm::vec3 pixelPos = p0 + (p1 - p0) * (x / (float)width) + (p2 - p0) * (y / (float)height);
                ray.O = camPos;
                ray.D = normalize( pixelPos - ray.O );
                ray.t = 1e30f;

                //for( int i = 0; i < N; i++ ) intersect_tri( ray, tris[i] );
                intersect_bvh(ray, rootNodeIdx);

                bool hit_anything = ray.t != 1e30f;

                buffer[x + y * width] = hit_anything ? 0xffffffff : 0x00000000;
	
            }
        }
    }
}