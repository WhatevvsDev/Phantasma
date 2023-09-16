#include "BVH.h"
#include "Mesh.h"

#include <utility>

void update_node_bounds( uint nodeIdx, BVH& bvh, const std::vector<Tri>& tris)
{
	BVHNode& node = bvh.bvhNodes[nodeIdx];
	node.min = glm::vec3( 1e30f );
	node.max = glm::vec3( -1e30f );
	for (uint first = node.left_first, i = 0; i < node.tri_count; i++)
	{
		uint leafTriIdx = bvh.triIdx[first + i];
		const Tri& leafTri = tris[leafTriIdx];
		node.min = glm::min( node.min, leafTri.vertex0 ),
		node.min = glm::min( node.min, leafTri.vertex1 ),
		node.min = glm::min( node.min, leafTri.vertex2 ),
		node.max = glm::max( node.max, leafTri.vertex0 ),
		node.max = glm::max( node.max, leafTri.vertex1 ),
		node.max = glm::max( node.max, leafTri.vertex2 );
	}
}

void subdivide( uint nodeIdx, BVH& bvh, const std::vector<Tri>& tris)
{
	// terminate recursion

	BVHNode& node = bvh.bvhNodes[nodeIdx];
	if (node.tri_count <= 2) return;
	// determine split axis and position
	glm::vec3 extent = node.max - node.min;
	int axis = 0;
	if (extent.y > extent.x) axis = 1;
	if (extent.z > extent[axis]) axis = 2;
	float splitPos = node.min[axis] + extent[axis] * 0.5f;
	// in-place partition
	int i = node.left_first;
	int j = i + node.tri_count - 1;
	while (i <= j)
	{
		if (bvh.centroids[bvh.triIdx[i]][axis] < splitPos)
			i++;
		else
			std::swap( bvh.triIdx[i], bvh.triIdx[j--] );
	}
	// abort split if one of the sides is empty
	int leftCount = i - node.left_first;
	if (leftCount == 0 || leftCount == (int)node.tri_count) return;
	// create child nodes
	int leftChildIdx = bvh.nodes_used++;
	int rightChildIdx = bvh.nodes_used++;

	bvh.bvhNodes[leftChildIdx].left_first = node.left_first;
	bvh.bvhNodes[leftChildIdx].tri_count = leftCount;
	bvh.bvhNodes[rightChildIdx].left_first = i;
	bvh.bvhNodes[rightChildIdx].tri_count = node.tri_count - leftCount;

	node.left_first = leftChildIdx;
	node.tri_count = 0;

	update_node_bounds( leftChildIdx, bvh, tris);
	update_node_bounds( rightChildIdx, bvh, tris );
	// recurse
	subdivide( leftChildIdx, bvh, tris );
	subdivide( rightChildIdx, bvh, tris );
}

BVH::BVH(const std::vector<Tri>& tris)
{
	size_t primitive_count = tris.size();

	triIdx.resize(primitive_count);
	bvhNodes.resize(primitive_count * 2);
	centroids.resize(tris.size());

	// populate triangle index array
	for (int i = 0; i < primitive_count; i++) triIdx[i] = i;
	// calculate triangle centroids for partitioning
	for (int i = 0; i < primitive_count; i++)
		centroids[i] = (tris[i].vertex0 + tris[i].vertex1 + tris[i].vertex2) * 0.3333f;

	// assign all triangles to root node
	BVHNode& root = bvhNodes[0];
	root.left_first = 0, root.tri_count = primitive_count;

	update_node_bounds(0, *this, tris);
	// subdivide recursively
	subdivide(0, *this, tris);
}
