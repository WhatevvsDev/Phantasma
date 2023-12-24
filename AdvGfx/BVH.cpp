#include "BVH.h"
#include "World.h"
#include "Assets.h"

const u32 SPLIT_PLANES = 8;

f32 aabb_area(const glm::vec3& extent)
{
	return extent.x * extent.y + extent.y * extent.z + extent.x * extent.z;
}

f32 get_node_cost(BVHNode& node)
{
	glm::vec3 extent = node.max - node.min;
	f32 cost = node.primitive_count * aabb_area(extent);
	return cost > 0 ? cost : 1e30f;
}

glm::vec3 aabb_centroid(AABB& aabb)
{
	return (aabb.min + aabb.max) * 0.5f;
}

// Transforms the current 8 corners of an AABB, and gets their AABB
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

	for (u32 i = 0; i < 8; i++)
	{
		aabb.min = glm::min(aabb.min, glm::vec3(corners[i].x, corners[i].y, corners[i].z));
		aabb.max = glm::max(aabb.max, glm::vec3(corners[i].x, corners[i].y, corners[i].z));
	}
}

AABB get_triangle_aabb(const Tri& triangle)
{
	AABB aabb;
	aabb.min = glm::min(triangle.vertex0, triangle.vertex1, triangle.vertex2);
	aabb.max = glm::max(triangle.vertex0, triangle.vertex1, triangle.vertex2);
	return aabb;
}

// Does not actually hold the BVH, just creates it wherever it was called from
struct BVHConstructor
{
	BVH& bvh;
	BVHConstructionPrimitiveAABBData primitive_aabb_data;

	BVHConstructor(BVH& bvh, const BVHConstructionPrimitiveAABBData& aabb_list);

	// Construction functions
	void update_node_bounds(BVHNode& node);
	void subdivide(BVHNode& node);

	// Helper construction functions
	f32 evaluate_sah(BVHNode& node, u32 axis, f32 pos);
	f32 find_best_split_plane(BVHNode& node, u32& axis, f32& split_pos);
};

BVHConstructor::BVHConstructor(BVH& bvh, const BVHConstructionPrimitiveAABBData& aabb_list)
	: bvh(bvh)
	, primitive_aabb_data(aabb_list)
{
	// assign all triangles to root node
	BVHNode& root = bvh.nodes[0];
	root.left_first = 0;
	root.primitive_count = (u32)aabb_list.primitive_count;

	for (u32 i = 0; i < aabb_list.primitive_count; i++)
	{
		bvh.primitive_idx[i] = i;
	}

	update_node_bounds(root);
	subdivide(root);
}

void BVHConstructor::update_node_bounds(BVHNode& node)
{
	node.min = glm::vec3(1e30f);
	node.max = glm::vec3(-1e30f);
	for (u32 i = 0; i < node.primitive_count; i++)
	{
		u32 aabb_index = bvh.primitive_idx[node.left_first + i];
		const AABB& aabb = primitive_aabb_data.primitive_aabbs[aabb_index];
		node.min = glm::min(node.min, aabb.min);
		node.max = glm::max(node.max, aabb.max);
	}
}

void BVHConstructor::subdivide(BVHNode& node)
{
	// Determine split axis using SAH
	u32 axis { 0 };
	f32 split_pos { 0.0f };

	f32 parent_cost = get_node_cost(node);
	f32 split_cost = find_best_split_plane(node, axis, split_pos);

	if (split_cost >= parent_cost) 
		return;

	// Sort and partition child primitives
	i32 i = node.left_first;
	i32 j = i + node.primitive_count - 1;
	while (i <= j)
	{
		if (primitive_aabb_data.centroids[bvh.primitive_idx[i]][axis] < split_pos)
			i++;
		else
			std::swap(bvh.primitive_idx[i], bvh.primitive_idx[j--]);
	}

	// Return early if one node is empty
	i32 left_count = i - node.left_first;

	bool one_node_is_empty = (left_count == 0) || (left_count == (i32)node.primitive_count);

	if (one_node_is_empty) 
		return;

	// create child nodes
	i32 left_child_idx = bvh.next_node_idx++;
	i32 right_child_idx = bvh.next_node_idx++;

	bvh.nodes[left_child_idx].left_first = node.left_first;
	bvh.nodes[left_child_idx].primitive_count = left_count;
	bvh.nodes[right_child_idx].left_first = i;
	bvh.nodes[right_child_idx].primitive_count = node.primitive_count - left_count;

	node.left_first = left_child_idx;
	node.primitive_count = 0;

	// Update child nodes and continue splitting
	update_node_bounds(bvh.nodes[left_child_idx]);
	update_node_bounds(bvh.nodes[right_child_idx]);

	subdivide(bvh.nodes[left_child_idx]);
	subdivide(bvh.nodes[right_child_idx]);
}

f32 BVHConstructor::evaluate_sah(BVHNode& node, u32 axis, f32 pos)
{
	// determine triangle counts and bounds for this split candidate
	glm::vec3 left_min = glm::vec3(1e30f);
	glm::vec3 left_max = glm::vec3(1e-30f);
	glm::vec3 right_min = glm::vec3(1e30f);
	glm::vec3 right_max = glm::vec3(1e-30f);

	i32 left_count = 0;
	i32 right_count = 0;

	for (uint i = 0; i < node.primitive_count; i++)
	{
		const AABB& aabb = primitive_aabb_data.primitive_aabbs[bvh.primitive_idx[node.left_first + i]];
		const glm::vec3& centroid = primitive_aabb_data.centroids[bvh.primitive_idx[node.left_first + i]];

		if (centroid[axis] < pos)
		{
			left_count++;
			left_min = glm::min(left_min, aabb.min);
			left_max = glm::max(left_max, aabb.max);
		}
		else
		{
			right_count++;
			right_min = glm::min(right_min, aabb.min);
			right_max = glm::max(right_max, aabb.max);
		}
	}
	glm::vec3 left_extent = left_max - left_min;
	glm::vec3 right_extent = right_max - right_min;
	f32 left_area = aabb_area(left_extent);
	f32 right_area = aabb_area(right_extent);

	f32 cost = left_count * left_area + right_count * right_area;
	return cost > 0 ? cost : 1e30f;
}

f32 BVHConstructor::find_best_split_plane(BVHNode& node, u32& axis, f32& split_pos)
{
	f32 best_cost = 1e30f;
	for (u32 a = 0; a < 3; a++)
	{
		f32 bounds_min = node.min[a];
		f32 bounds_max = node.max[a];

		if (bounds_min == bounds_max)
			continue;

		f32 scale = (bounds_max - bounds_min) / (f32)(SPLIT_PLANES + 1);
		for (uint i = 1; i < SPLIT_PLANES; i++)
		{
			f32 potential_position = bounds_min + i * scale;
			f32 cost = evaluate_sah(node, a, potential_position);

			if (cost < best_cost)
			{
				split_pos = potential_position;
				axis = a;
				best_cost = cost;
			}
		}
	}

	return best_cost;
}

BVHConstructionPrimitiveAABBData::BVHConstructionPrimitiveAABBData(const std::vector<Tri>& triangles)
	: primitive_count(triangles.size())
{
	primitive_aabbs.resize(primitive_count);
	centroids.resize(primitive_count);

	for (u32 i = 0; i < primitive_count; i++)
	{
		auto& triangle = triangles[i];
		primitive_aabbs[i] = get_triangle_aabb(triangle);
		centroids[i] = aabb_centroid(primitive_aabbs[i]);
	}
}

BVHConstructionPrimitiveAABBData::BVHConstructionPrimitiveAABBData(const std::vector<AABB>& aabbs)
	: primitive_count(aabbs.size())
{
	primitive_aabbs.resize(primitive_count);
	centroids.resize(primitive_count);

	for (u32 i = 0; i < primitive_count; i++)
	{
		primitive_aabbs[i] = aabbs[i];
		centroids[i] = aabb_centroid(primitive_aabbs[i]);
	}
}

void BuildTLAS(BVH& bvh)
{
	auto& world_data = World::get_world_device_data();
	u32 instance_count = world_data.mesh_instance_count;

	std::vector<AABB> transformed_aabbs;

	usize primitive_count = instance_count;

	bvh.primitive_idx.resize(primitive_count);
	bvh.nodes.resize(primitive_count * 2);

	for (u32 i = 0; i < instance_count; i++)
	{
		bvh.primitive_idx[i] = i;

		auto bvhnode = Assets::get_root_bvh_node_of_mesh(world_data.mesh_instances[i].mesh_idx);

		AABB aabb = { bvhnode.min, bvhnode.max };

		transform_aabb(aabb, world_data.mesh_instances[i].transform);

		transformed_aabbs.push_back(aabb);
	}

	BVHConstructor(bvh, transformed_aabbs);
}

void BuildBLAS(BVH& bvh, const BVHConstructionPrimitiveAABBData& aabb_list)
{
	usize primitive_count = aabb_list.primitive_count;

	bvh.primitive_idx.resize(primitive_count);
	bvh.nodes.resize(primitive_count * 2);

	// populate triangle index array
	for (i32 i = 0; i < primitive_count; i++)
		bvh.primitive_idx[i] = i;

	BVHConstructor(bvh, aabb_list.primitive_aabbs);
}