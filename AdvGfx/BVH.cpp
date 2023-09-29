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

float aabb_area(const glm::vec3& extent)
{
	return extent.x * extent.y + extent.y * extent.z + extent.x * extent.z;
}

float evaluate_sah( BVHNode& node, int axis, float pos, BVH& bvh, const std::vector<Tri>& tris )
{
	// determine triangle counts and bounds for this split candidate
	glm::vec3 left_min  = glm::vec3(1e30);
	glm::vec3 left_max  = glm::vec3(1e-30);
	glm::vec3 right_min = glm::vec3(1e30);
	glm::vec3 right_max = glm::vec3(1e-30);

	int leftCount = 0, rightCount = 0;
	for( uint i = 0; i < node.tri_count; i++ )
	{
		const Tri& triangle = tris[bvh.triIdx[node.left_first + i]];
		if (bvh.centroids[bvh.triIdx[node.left_first + i]][axis] < pos)
		{
			leftCount++;
			left_min = glm::min(left_min, triangle.vertex0);
			left_min = glm::min(left_min, triangle.vertex1);
			left_min = glm::min(left_min, triangle.vertex2);
			left_max = glm::max(left_max, triangle.vertex0);
			left_max = glm::max(left_max, triangle.vertex1);
			left_max = glm::max(left_max, triangle.vertex2);
		}
		else
		{
			rightCount++;
			right_min = glm::min(right_min, triangle.vertex0);
			right_min = glm::min(right_min, triangle.vertex1);
			right_min = glm::min(right_min, triangle.vertex2);
			right_max = glm::max(right_max, triangle.vertex0);
			right_max = glm::max(right_max, triangle.vertex1);
			right_max = glm::max(right_max, triangle.vertex2);
		}
	}
	glm::vec3 left_extent = left_max - left_min;
	glm::vec3 right_extent = right_max - right_min;
	float left_area = aabb_area(left_extent);
	float right_area = aabb_area(right_extent);

	float cost = leftCount * left_area + rightCount * right_area;
	return cost > 0 ? cost : 1e30f;
}

void subdivide( uint nodeIdx, BVH& bvh, const std::vector<Tri>& tris)
{
	BVHNode& node = bvh.bvhNodes[nodeIdx];
	// determine split axis using SAH
	int bestAxis = -1;
	float bestPos = 0, bestCost = 1e30f;
	for (int axis = 0; axis < 3; axis++) for (uint i = 0; i < node.tri_count; i++)
	{
		const Tri& triangle = tris[bvh.triIdx[node.left_first + i]];
		float candidatePos = bvh.centroids[bvh.triIdx[i]][axis];
		float cost = evaluate_sah( node, axis, candidatePos , bvh, tris);
		if (cost < bestCost)
			bestPos = candidatePos, bestAxis = axis, bestCost = cost;
	}
	int axis = bestAxis;
	float splitPos = bestPos;
	glm::vec3 e = node.max - node.min; // extent of parent
	float parentArea = aabb_area(e);
	float parentCost = node.tri_count * parentArea;
	LOGDEFAULT(std::format("Best {}, Parent {}", bestCost, parentCost));
	if (bestCost >= parentCost) return;
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
	if (leftCount == 0 || leftCount == node.tri_count) return;
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
	root.left_first = 0, root.tri_count = (unsigned int)primitive_count;

	update_node_bounds(0, *this, tris);
	// subdivide recursively
	subdivide(0, *this, tris);
}
