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
	BVH();

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

struct BVHConstructionAABBList
{
	std::vector<AABB> primitive_aabbs;
	std::vector<glm::vec3> centroids;

	BVHConstructionAABBList(const std::vector<Tri>& tris);
	BVHConstructionAABBList(const std::vector<AABB>& aabbs);
};


// Does not actually hold the BVH, just creates it wherever it was called from
struct BVHConstructor
{
	BVH& bvh;
	BVHConstructionAABBList aabb_list;

	BVHConstructor(BVH& bvh, const BVHConstructionAABBList& aabb_list);

	void update_node_bounds(uint nodeIdx);
	float evaluate_sah(BVHNode& node, int axis, float pos);
	float find_best_split_plane(BVHNode& node, int& axis, float& splitPos);
	void subdivide(uint nodeIdx);
};

struct TLASConstructor
{
	TLASConstructor(BVH& bvh);
};

struct BLASConstructor
{
	BLASConstructor(BVH& bvh, const BVHConstructionAABBList& aabb_list);
};