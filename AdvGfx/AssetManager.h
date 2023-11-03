#pragma once
#include "Compute.h"
#include "Mesh.h"
#include "BVH.h"

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

struct DiskAsset
{
	std::filesystem::path path;
	std::string file_name;
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
	std::unordered_map<std::string, Mesh>& get_meshes();

	std::vector<DiskAsset>& get_disk_files_by_extension(const std::string& extension);

	BVHNode get_root_bvh_node_of_mesh(u32 idx);
}