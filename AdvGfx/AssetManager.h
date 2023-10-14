#pragma once
#include <filesystem>
#include "Compute.h"
#include "Math.h"

struct MeshHeader
{
	uint tris_offset {};
	uint tris_count {};

	uint normals_offset {};
	uint normals_count {}; // Is in theory always 3x tris_count;

	uint root_bvh_node_idx {};
	uint bvh_node_count {}; // Technically could be unnecessary

	uint tri_idx_offset {};
	uint tri_idx_count {};
};

namespace AssetManager
{
	void init();

	void load_mesh(const std::filesystem::path path);

	void reconstruct_bvh(const std::string mesh);

	int loaded_mesh_count();

	ComputeWriteBuffer& get_tris_compute_buffer();
	ComputeWriteBuffer& get_normals_compute_buffer();
	ComputeWriteBuffer& get_bvh_compute_buffer();
	ComputeWriteBuffer& get_tri_idx_compute_buffer();
	ComputeWriteBuffer& get_mesh_header_buffer();
}