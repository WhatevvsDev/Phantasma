#include "BVH.h"
#include "World.h"
#include "Assets.h"

/*

BVHConstructor::BVHConstructor(BVH& bvh, const BVHConstructionAABBList& aabb_list)
	: bvh(bvh)
	, aabb_list(aabb_list)
{
	// assign all triangles to root node
	BVHNode& root = bvh.nodes[0];
	root.left_first = 0, root.tri_count = (unsigned int)aabb_list.primitive_aabbs.size();

	update_node_bounds(0);
	// subdivide recursively
	subdivide(0);
}

void BVHConstructor::update_node_bounds(uint nodeIdx)
{
	BVHNode& node = bvh.nodes[nodeIdx];

	node.min = glm::vec3(1e30f);
	node.max = glm::vec3(-1e30f);
	for (uint i = 0; i < node.tri_count; i++)
	{
		uint leaf_primitive_index = bvh.triIdx[node.left_first + i];
		const AABB& aabb = aabb_list.primitive_aabbs[leaf_primitive_index];
		node.min = glm::min(node.min, aabb.min);
		node.max = glm::max(node.max, aabb.max);
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
		const AABB& leaf_primitive_aabb = aabb_list.primitive_aabbs[bvh.triIdx[node.left_first + i]];
		if (aabb_list.centroids[bvh.triIdx[node.left_first + i]][axis] < pos)
		{
			leftCount++;
			left_min = glm::min(left_min, leaf_primitive_aabb.min);
			left_max = glm::max(left_max, leaf_primitive_aabb.max);
		}
		else
		{
			rightCount++;
			right_min = glm::min(right_min, leaf_primitive_aabb.min);
			right_max = glm::max(right_max, leaf_primitive_aabb.max);
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

		float scale = (boundsMax - boundsMin) / split_planes;
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
	BVHNode& node = bvh.nodes[nodeIdx];

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
	bvh.nodes[leftChildIdx].left_first = node.left_first;
	bvh.nodes[leftChildIdx].tri_count = leftCount;
	bvh.nodes[rightChildIdx].left_first = i;
	bvh.nodes[rightChildIdx].tri_count = node.tri_count - leftCount;
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




TLASConstructor::TLASConstructor(BVH& bvh)
{
	auto& world_data = World::get_world_device_data();
	u32 instance_count = world_data.mesh_instance_count;

	std::vector<AABB> transformed_aabbs;

	bvh.triIdx.resize(instance_count);
	bvh.nodes.resize(instance_count * 2);

	// populate triangle index array
	for (u32 i = 0; i < instance_count; i++)
		bvh.triIdx[i] = i;

	for (u32 i = 0; i < instance_count; i++)
	{
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
	bvh.nodes.resize(primitive_count * 2);

	// populate triangle index array
	for (u32 i = 0; i < primitive_count; i++)
		bvh.triIdx[i] = i;

	BVHConstructor(bvh, aabb_list.primitive_aabbs);
}
*/

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

struct BVHBuilder
{
	BVH& bvh;
	const BVHConstructionAABBList& primitive_aabb_list;
	bool is_tlas;

	BVHBuilder(BVH& bvh, const BVHConstructionAABBList& primitives, bool tlas = false);

	void update_node_bounds(uint nodeIdx);
	float evaluate_sah(BVHNode& node, int axis, float pos);
	float find_best_split_plane(BVHNode& node, int& axis, float& splitPos);
	void subdivide(uint nodeIdx);
};

BVHBuilder::BVHBuilder(BVH& bvh, const BVHConstructionAABBList& primitives, bool tlas)
	: bvh(bvh)
	, primitive_aabb_list(primitives)
{

	is_tlas = tlas;
	// assign all triangles to root node
	BVHNode& root = bvh.nodes[0];
	root.left_first = 0, root.tri_count = (unsigned int)primitives.primitive_aabbs.size();

	bvh.centroids = primitive_aabb_list.centroids;

	for (u32 i = 0; i < root.tri_count; i++)
	{
		bvh.triIdx[i] = i;
	}

	update_node_bounds(0);
	// subdivide recursively
	subdivide(0);
}

void BVHBuilder::update_node_bounds(uint nodeIdx)
{
	BVHNode& node = bvh.nodes[nodeIdx];

	node.min = glm::vec3(1e30f);
	node.max = glm::vec3(-1e30f);
	for (uint first = node.left_first, i = 0; i < node.tri_count; i++)
	{
		uint leafTriIdx = bvh.triIdx[first + i];
		const AABB& leafTri = primitive_aabb_list.primitive_aabbs[leafTriIdx];
		node.min = glm::min(node.min, leafTri.min);
		node.max = glm::max(node.max, leafTri.max);
	}
}

float BVHBuilder::evaluate_sah(BVHNode& node, int axis, float pos)
{
	// determine triangle counts and bounds for this split candidate
	glm::vec3 left_min = glm::vec3(1e30f);
	glm::vec3 left_max = glm::vec3(1e-30f);
	glm::vec3 right_min = glm::vec3(1e30f);
	glm::vec3 right_max = glm::vec3(1e-30f);

	int leftCount = 0, rightCount = 0;
	for (uint i = 0; i < node.tri_count; i++)
	{
		const AABB& aabb = primitive_aabb_list.primitive_aabbs[bvh.triIdx[node.left_first + i]];

		if (primitive_aabb_list.centroids[bvh.triIdx[node.left_first + i]][axis] < pos)
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

	float left_cost = (leftCount > 0) ? leftCount * left_area : 0;
	float right_cost = (rightCount > 0) ? rightCount * right_area : 0;

	float cost = left_cost + right_cost;
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

void BVHBuilder::subdivide(uint nodeIdx)
{
	BVHNode& node = bvh.nodes[nodeIdx];

	// determine split axis using SAH
	int axis;
	float splitPos;

	float parentCost = get_node_cost(node);
	float splitCost = find_best_split_plane(node, axis, splitPos);

	if (is_tlas)
	{
		LOGERROR(std::format("BVH parent/split cost: {} / {}", parentCost, splitCost));
	}

	if (splitCost >= parentCost) return;
	// in-place partition
	int i = node.left_first;
	int j = i + node.tri_count - 1;
	while (i <= j)
	{
		
		if (primitive_aabb_list.centroids[bvh.triIdx[i]][axis] < splitPos)
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
	bvh.nodes[leftChildIdx].left_first = node.left_first;
	bvh.nodes[leftChildIdx].tri_count = leftCount;
	bvh.nodes[rightChildIdx].left_first = i;
	bvh.nodes[rightChildIdx].tri_count = node.tri_count - leftCount;
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

	for (int i = 0; i < 8; i++)
	{
		aabb.min = glm::min(aabb.min, glm::vec3(corners[i].x, corners[i].y, corners[i].z));
		aabb.max = glm::max(aabb.max, glm::vec3(corners[i].x, corners[i].y, corners[i].z));
	}
}

void TLASBuilder::update_node_bounds(BVHNode& node)
{
	node.min = glm::vec3(1e30f);
	node.max = glm::vec3(-1e30f);

	u32 first_child_instance = node.left_first;

	for (u32 i = 0; i < node.tri_count; i++)
	{
		AABB instance_bounding_box = instance_bounding_boxes[first_child_instance + i];

		node.min = glm::min(node.min, instance_bounding_box.min);
		node.max = glm::max(node.max, instance_bounding_box.max);
	}
}

glm::vec3 aabb_centroid(AABB& aabb)
{
	return (aabb.min + aabb.max) * 0.5f;
}

float TLASBuilder::evaluate_sah(BVHNode& node, int axis, float pos)
{
	// determine triangle counts and bounds for this split candidate
	glm::vec3 left_min = glm::vec3(1e30f);
	glm::vec3 left_max = glm::vec3(1e-30f);
	glm::vec3 right_min = glm::vec3(1e30f);
	glm::vec3 right_max = glm::vec3(1e-30f);

	int leftCount = 0, rightCount = 0;
	for (uint i = 0; i < node.tri_count; i++)
	{
		const AABB& aabb = instance_bounding_boxes[tri_idx[node.left_first + i]];
		if (aabb_centroid(instance_bounding_boxes[tri_idx[node.left_first + i]])[axis] < pos)
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

float TLASBuilder::find_best_split_plane(BVHNode& node, int& axis, float& splitPos)
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

void TLASBuilder::subdivide(uint nodeIdx)
{
	BVHNode& node = nodes[nodeIdx];

	// determine split axis using SAH
	int axis;
	float splitPos;

	float parentCost = get_node_cost(node);
	float splitCost = find_best_split_plane(node, axis, splitPos);

	LOGERROR(std::format("TLAS parent/split cost: {} / {}", parentCost, splitCost));

	if (splitCost >= parentCost) return;
	// in-place partition
	int i = node.left_first;
	int j = i + node.tri_count - 1;
	while (i <= j)
	{
		if (aabb_centroid(instance_bounding_boxes[tri_idx[i]])[axis] < splitPos)
			i++;
		else
			std::swap(tri_idx[i], tri_idx[j--]);
	}
	// abort split if one of the sides is empty
	int leftCount = i - node.left_first;
	if (leftCount == 0 || leftCount == (int)node.tri_count) return;
	// create child nodes
	int leftChildIdx = next_node_idx++;
	int rightChildIdx = next_node_idx++;
	nodes[leftChildIdx].left_first = node.left_first;
	nodes[leftChildIdx].tri_count = leftCount;
	nodes[rightChildIdx].left_first = i;
	nodes[rightChildIdx].tri_count = node.tri_count - leftCount;
	node.left_first = leftChildIdx;
	node.tri_count = 0;

	update_node_bounds(nodes[leftChildIdx]);
	update_node_bounds(nodes[rightChildIdx]);
	// recurse
	subdivide(leftChildIdx);
	subdivide(rightChildIdx);
}

TLASBuilder::TLASBuilder()
{
	auto& world_data = World::get_world_device_data();
	u32 instance_count = world_data.mesh_instance_count;

	if (instance_count == 0)
	{
		nodes.resize(2);
		tri_idx.resize(2);
		return;
	}

	nodes.resize(instance_count * 2);
	instance_bounding_boxes.resize(instance_count);
	tri_idx.resize(instance_count);

	for (u32 i = 0; i < instance_count; i++)
	{
		tri_idx[i] = i;

		auto bvhnode = Assets::get_root_bvh_node_of_mesh(world_data.mesh_instances[i].mesh_idx);

		instance_bounding_boxes[i].min = bvhnode.min;
		instance_bounding_boxes[i].max = bvhnode.max;

		AABB bb = instance_bounding_boxes[i];

		transform_aabb(bb, world_data.mesh_instances[i].transform);

		instance_bounding_boxes[i] = bb;
	}

	BVHConstructionAABBList instance_aabbs(instance_bounding_boxes);

	nodes[0].tri_count = instance_count;

	next_node_idx = 1;

	update_node_bounds(nodes[0]);

	subdivide(0);

	/* ------------------------------------------------------ */
	/* ------------------------------------------------------ */
	/* ------------------------------------------------------ */

	BVH test_bvh;

	test_bvh.nodes.resize(instance_count * 2);
	test_bvh.centroids.resize(instance_count);
	test_bvh.triIdx.resize(instance_count);

	BVHBuilder(test_bvh, instance_aabbs, true);

	LOGERROR("PRINTING NORMAL TLAS");
	for (int i = 0; i < (int)instance_count; i++)
		LOGERROR(std::format("I AM A DEFAULT TLAS NODE {}", nodes[i].left_first));

	LOGERROR("\n\nPRINTING NORMAL BVH");
	for (int i = 0; i < (int)instance_count; i++)
		LOGERROR(std::format("I AM A NORMAL BVH NODE {}", test_bvh.nodes[i].left_first));


	tri_idx = test_bvh.triIdx;
	instance_aabbs.centroids = test_bvh.centroids;
	//nodes = test_bvh.nodes;

}

BVH::BVH(const std::vector<Tri>& tris)
{

	size_t primitive_count = tris.size();

	triIdx.resize(primitive_count);
	nodes.resize(primitive_count * 2);
	centroids.resize(tris.size());

	// populate triangle index array
	for (int i = 0; i < primitive_count; i++)
		triIdx[i] = i;

	// calculate triangle centroids for partitioning
	for (int i = 0; i < primitive_count; i++)
		centroids[i] = (tris[i].vertex0 + tris[i].vertex1 + tris[i].vertex2) * 0.3333f;

	BVHBuilder(*this, tris);
}

// READDED NEW // READDED NEW // READDED NEW // READDED NEW // READDED NEW // READDED NEW
// READDED NEW // READDED NEW // READDED NEW // READDED NEW // READDED NEW // READDED NEW
// READDED NEW // READDED NEW // READDED NEW // READDED NEW // READDED NEW // READDED NEW
// READDED NEW // READDED NEW // READDED NEW // READDED NEW // READDED NEW // READDED NEW
// READDED NEW // READDED NEW // READDED NEW // READDED NEW // READDED NEW // READDED NEW

/*
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
*/

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