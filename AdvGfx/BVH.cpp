#include "BVH.h"
#include "World.h"
#include "Assets.h"

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

glm::vec3 aabb_centroid(AABB& aabb)
{
	return (aabb.min + aabb.max) * 0.5f;
}

BVHConstructor::BVHConstructor(BVH& bvh, const BVHConstructionAABBList& aabb_list)
	: bvh(bvh)
	, aabb_list(aabb_list)
{
	// assign all triangles to root node
	BVHNode& root = bvh.bvhNodes[0];
	root.left_first = 0, root.tri_count = (unsigned int)aabb_list.primitive_aabbs.size();

	for (u32 i = 0; i < aabb_list.centroids.size(); i++)
	{
		bvh.triIdx[i] = i;
	}

	update_node_bounds(0);
	// subdivide recursively
	subdivide(0);
}

void BVHConstructor::update_node_bounds(uint nodeIdx)
{
	BVHNode& node = bvh.bvhNodes[nodeIdx];

	node.min = glm::vec3(1e30f);
	node.max = glm::vec3(-1e30f);
	for (u32 i = 0; i < node.tri_count; i++)
	{
		uint leafTriIdx = bvh.triIdx[node.left_first + i];
		const AABB& leafTri = aabb_list.primitive_aabbs[leafTriIdx];
		node.min = glm::min(node.min, leafTri.min);
		node.max = glm::max(node.max, leafTri.max);
	}
}

float BVHConstructor::evaluate_sah(BVHNode& node, int axis, float pos)
{
	// determine triangle counts and bounds for this split candidate
	glm::vec3 left_min = glm::vec3(1e30f);
	glm::vec3 left_max = glm::vec3(1e-30f);
	glm::vec3 right_min = glm::vec3(1e30f);
	glm::vec3 right_max = glm::vec3(1e-30f);

	int leftCount = 0, rightCount = 0;
	for (uint i = 0; i < node.tri_count; i++)
	{
		const AABB& aabb = aabb_list.primitive_aabbs[bvh.triIdx[node.left_first + i]];
		const glm::vec3& centroid = aabb_list.centroids[bvh.triIdx[node.left_first + i]];

		if (centroid[axis] < pos)
		{
			leftCount++;
			left_min = glm::min(left_min, aabb.min);
			left_max = glm::max(left_max, aabb.max);
		}
		else
		{
			rightCount++;
			right_min = glm::min(right_min, aabb.min);
			right_max = glm::max(right_max, aabb.max);
		}
	}
	glm::vec3 left_extent = left_max - left_min;
	glm::vec3 right_extent = right_max - right_min;
	float left_area = aabb_area(left_extent);
	float right_area = aabb_area(right_extent);

	float cost = leftCount * left_area + rightCount * right_area;
	return cost > 0 ? cost : 1e30f;
}

float BVHConstructor::find_best_split_plane(BVHNode& node, int& axis, float& splitPos)
{
	float bestCost = 1e30f;
	for (int a = 0; a < 3; a++)
	{
		float boundsMin = node.min[a];
		float boundsMax = node.max[a];

		if (boundsMin == boundsMax)
			continue;

		int split_planes = 8;

		float scale = (boundsMax - boundsMin) / (split_planes + 1);
		for (uint i = 1; i < (uint)split_planes; i++)
		{
			float candidatePos = boundsMin + i * scale;
			float cost = evaluate_sah(node, a, candidatePos);

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

void BVHConstructor::subdivide(uint nodeIdx)
{
	BVHNode& node = bvh.bvhNodes[nodeIdx];

	// determine split axis using SAH
	int axis;
	float splitPos;

	float parentCost = get_node_cost(node);
	float splitCost = find_best_split_plane(node, axis, splitPos);

	if (splitCost >= parentCost) return;
	// in-place partition
	int i = node.left_first;
	int j = i + node.tri_count - 1;
	while (i <= j)
	{
		if (aabb_list.centroids[bvh.triIdx[i]][axis] < splitPos)
			i++;
		else
			std::swap(bvh.triIdx[i], bvh.triIdx[j--]);
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
	subdivide(leftChildIdx);
	subdivide(rightChildIdx);
}

void transform_aabb(AABB& aabb, glm::mat4& transform)
{
	glm::vec4 corners[8] =
	{
		transform * glm::vec4(aabb.max.x, aabb.max.y, aabb.max.z, 1.0f),
		transform * glm::vec4(aabb.max.x, aabb.max.y, aabb.min.z, 1.0f),
		transform * glm::vec4(aabb.max.x, aabb.min.y, aabb.max.z, 1.0f),
		transform * glm::vec4(aabb.max.x, aabb.min.y, aabb.min.z, 1.0f),

		transform * glm::vec4(aabb.min.x, aabb.max.y, aabb.max.z, 1.0f),
		transform * glm::vec4(aabb.min.x, aabb.max.y, aabb.min.z, 1.0f),
		transform * glm::vec4(aabb.min.x, aabb.min.y, aabb.max.z, 1.0f),
		transform * glm::vec4(aabb.min.x, aabb.min.y, aabb.min.z, 1.0f)
	};

	aabb.min = glm::vec3(1e30f);
	aabb.max = glm::vec3(-1e30f);

	for(int i = 0; i < 8; i++)
	{
		aabb.min = glm::min(aabb.min, glm::vec3(corners[i].x, corners[i].y, corners[i].z));
		aabb.max = glm::max(aabb.max, glm::vec3(corners[i].x, corners[i].y, corners[i].z));
	}
}

BVH::BVH()
{
}

AABB get_triangle_aabb(const Tri& triangle)
{
	AABB aabb;
	aabb.min = glm::vec3(1e30f);
	aabb.max = glm::vec3(-1e30f);
	aabb.min = glm::min(aabb.min, triangle.vertex0, triangle.vertex1, triangle.vertex2);
	aabb.max = glm::max(aabb.max, triangle.vertex0, triangle.vertex1, triangle.vertex2);
	return aabb;
}

BVHConstructionAABBList::BVHConstructionAABBList(const std::vector<Tri>& triangles)
{
	primitive_aabbs.resize(triangles.size());
	centroids.resize(triangles.size());

	for (u32 i = 0; i < triangles.size(); i++)
	{
		auto& triangle = triangles[i];
		primitive_aabbs[i] = get_triangle_aabb(triangle);
		centroids[i] = aabb_centroid(primitive_aabbs[i]);
	}
}

BVHConstructionAABBList::BVHConstructionAABBList(const std::vector<AABB>& aabbs)
{
	primitive_aabbs.resize(aabbs.size());
	centroids.resize(aabbs.size());

	for (u32 i = 0; i < aabbs.size(); i++)
	{
		primitive_aabbs[i] = aabbs[i];
		centroids[i] = aabb_centroid(primitive_aabbs[i]);
	}
}

TLASConstructor::TLASConstructor(BVH& bvh)
{
	auto& world_data = World::get_world_device_data();
	u32 instance_count = world_data.mesh_instance_count;

	std::vector<AABB> transformed_aabbs;

	size_t primitive_count = instance_count;

	bvh.triIdx.resize(primitive_count);
	bvh.bvhNodes.resize(primitive_count * 2);
	bvh.centroids.resize(primitive_count);

	for (u32 i = 0; i < instance_count; i++)
	{
		bvh.triIdx[i] = i;

		auto bvhnode = Assets::get_root_bvh_node_of_mesh(world_data.mesh_instances[i].mesh_idx);

		AABB aabb = { bvhnode.min, bvhnode.max };

		transform_aabb(aabb, world_data.mesh_instances[i].transform);

		transformed_aabbs.push_back(aabb);
	}

	BVHConstructor(bvh, transformed_aabbs);
}

BLASConstructor::BLASConstructor(BVH& bvh, const BVHConstructionAABBList& aabb_list)
{
	size_t primitive_count = aabb_list.primitive_aabbs.size();

	bvh.triIdx.resize(primitive_count);
	bvh.bvhNodes.resize(primitive_count * 2);
	bvh.centroids.resize(primitive_count);

	// populate triangle index array
	for (int i = 0; i < primitive_count; i++)
		bvh.triIdx[i] = i;

	BVHConstructor(bvh, aabb_list.primitive_aabbs);
}