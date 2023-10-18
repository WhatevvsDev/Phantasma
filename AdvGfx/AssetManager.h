#pragma once
#include <filesystem>
#include "Compute.h"
#include "Math.h"
#include "PrimitiveTypes.h"

struct MeshHeader
{
	u32 tris_offset {};
	u32 tris_count {};

	u32 normals_offset {};
	u32 normals_count {}; // Is in theory always 3x tris_count;

	u32 root_bvh_node_idx {};
	u32 bvh_node_count {}; // Technically could be unnecessary

	u32 tri_idx_offset {};
	u32 tri_idx_count {};
};

namespace AssetManager
{
	void init();

	void load_mesh(const std::filesystem::path path);

	void reconstruct_bvh(const std::string mesh);

	ComputeWriteBuffer& get_tris_compute_buffer();
	ComputeWriteBuffer& get_normals_compute_buffer();
	ComputeWriteBuffer& get_bvh_compute_buffer();
	ComputeWriteBuffer& get_tri_idx_compute_buffer();
	ComputeWriteBuffer& get_mesh_header_buffer();

	std::vector<MeshHeader>& get_mesh_headers();
}