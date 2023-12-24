#pragma once

// Required for Tri definition
#include "Mesh.h"

struct BVHNode
{
	glm::vec3 min		{ 1e30f };
	u32 left_first		{ 0 };
	glm::vec3 max		{ -1e30f };
	u32 primitive_count	{ 0 };
};

struct BVH
{
	std::vector<u32>       primitive_idx;
	std::vector<BVHNode>   nodes;
	u32 next_node_idx { 1 };
};

struct AABB
{
	glm::vec3 min;
	glm::vec3 max;
};

struct BVHConstructionPrimitiveAABBData
{
	usize primitive_count;
	std::vector<AABB> primitive_aabbs;
	std::vector<glm::vec3> centroids;

	BVHConstructionPrimitiveAABBData(const std::vector<Tri>& tris);
	BVHConstructionPrimitiveAABBData(const std::vector<AABB>& aabbs);
};


void BuildTLAS(BVH& bvh);

void BuildBLAS(BVH& bvh, const BVHConstructionPrimitiveAABBData& aabb_list);