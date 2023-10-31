#include "BVH.h"


float aabb_area(const glm::vec3& extent)
{
	return extent.x * extent.y + extent.y * extent.z + extent.x * extent.z;
}

float get_node_cost(BVHNode& node)
{
	glm::vec3 extent = node.max - node.min;
	float area = aabb_area(extent);
	float cost = node.tri_count * area;
	return cost > 0 ? cost : 1e30f;
}

struct BVHBuilder
{
	BVH& bvh;
	const std::vector<Tri>& tris;

	BVHBuilder(BVH& bvh, const std::vector<Tri>& tris);

	void update_node_bounds( uint nodeIdx);
	float evaluate_sah( BVHNode& node, int axis, float pos);
	float find_best_split_plane( BVHNode& node, int& axis, float& splitPos);
	void subdivide( uint nodeIdx);
};

BVHBuilder::BVHBuilder(BVH& bvh, const std::vector<Tri>& tris)
	: bvh(bvh)
	, tris(tris)
{
	// assign all triangles to root node
	BVHNode& root = bvh.bvhNodes[0];
	root.left_first = 0, root.tri_count = (unsigned int)tris.size();

	update_node_bounds(0);
	// subdivide recursively
	subdivide(0);
}

void BVHBuilder::update_node_bounds( uint nodeIdx)
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

float BVHBuilder::evaluate_sah(BVHNode& node, int axis, float pos)
{
	// determine triangle counts and bounds for this split candidate
	glm::vec3 left_min  = glm::vec3(1e30f);
	glm::vec3 left_max  = glm::vec3(1e-30f);
	glm::vec3 right_min = glm::vec3(1e30f);
	glm::vec3 right_max = glm::vec3(1e-30f);

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

float BVHBuilder::find_best_split_plane(BVHNode& node, int& axis, float& splitPos)
{
	float bestCost = 1e30f;
	for (int a = 0; a < 3; a++) 
	{
		float boundsMin = node.min[a];
		float boundsMax = node.max[a];

		if (boundsMin == boundsMax) 
			continue;

		int split_planes = 8;

		float scale = (boundsMax - boundsMin) / split_planes;
		for (uint i = 1; i < (uint)split_planes; i++)
		{
			float candidatePos = boundsMin + i * scale;
			float cost = evaluate_sah( node, a, candidatePos);

			if (cost < bestCost)
			{
				splitPos = candidatePos;
				axis = a;
				bestCost = cost;
			}
		}
	}
	return bestCost;
}

void BVHBuilder::subdivide( uint nodeIdx)
{
	BVHNode& node = bvh.bvhNodes[nodeIdx];

	// determine split axis using SAH
	int axis;
	float splitPos;

	float parentCost = get_node_cost(node);
	float splitCost = find_best_split_plane( node, axis, splitPos);

	if (splitCost >= parentCost) return;
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

	update_node_bounds(leftChildIdx);
	update_node_bounds(rightChildIdx);
	// recurse
	subdivide( leftChildIdx);
	subdivide( rightChildIdx);
}

BVH::BVH(const std::vector<Tri>& tris)
{

	size_t primitive_count = tris.size();

	triIdx.resize(primitive_count);
	bvhNodes.resize(primitive_count * 2);
	centroids.resize(tris.size());

	// populate triangle index array
	for (int i = 0; i < primitive_count; i++) 
		triIdx[i] = i;

	// calculate triangle centroids for partitioning
	for (int i = 0; i < primitive_count; i++)
		centroids[i] = (tris[i].vertex0 + tris[i].vertex1 + tris[i].vertex2) * 0.3333f;
	
	BVHBuilder(*this, tris);
}
