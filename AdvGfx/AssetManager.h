#pragma once
#include <filesystem>
#include "Compute.h"

namespace AssetManager
{
	void init();

	void load_mesh(const std::filesystem::path path);

	void reconstruct_bvh(const std::string mesh);

	ComputeWriteBuffer& get_tris_compute_buffer();
	ComputeWriteBuffer& get_normals_compute_buffer();
	ComputeWriteBuffer& get_bvh_compute_buffer();
	ComputeWriteBuffer& get_tri_idx_compute_buffer();
}