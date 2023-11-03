#pragma once

#include "Mesh.h"

struct BVHNode
{
	glm::vec3 min { 1e30f };
	uint left_first { 0 };
	glm::vec3 max { -1e30f };
	uint tri_count { 0 };
};

struct BVH
{
	BVH(const std::vector<Tri>& tris);

	std::vector<uint>       triIdx;
	std::vector<BVHNode>    bvhNodes;
	std::vector<glm::vec3>  centroids;
	uint nodes_used = 1;

	float build_time = 0.0f;
};

struct AABB
{
	glm::vec3 min;
	glm::vec3 max;
};

struct TLASBuilder
{
	TLASBuilder();

	void update_node_bounds(BVHNode& node);
	float evaluate_sah(BVHNode& node, int axis, float pos);
	float find_best_split_plane(BVHNode& node, int& axis, float& splitPos);
	void subdivide( uint nodeIdx);

	std::vector<BVHNode> nodes;
	std::vector<AABB> instance_bounding_boxes;
	std::vector<u32> tri_idx;
	u32 next_node_idx { 0 };
};


void update_node_bounds( uint nodeIdx, BVH& bvh, const std::vector<Tri>& tris);
void subdivide( uint nodeIdx, BVH& bvh, const std::vector<Tri>& tris);