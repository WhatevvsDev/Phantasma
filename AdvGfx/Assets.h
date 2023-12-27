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

	u32 uvs_offset {};

	u32 root_bvh_node_idx {};
	u32 bvh_node_count {}; // Technically could be unnecessary

	u32 tri_idx_offset {};
	u32 tri_idx_count {};
	
	u32 pad {};
};

struct TextureHeader	
{
	u32 start_offset;
	u32 width;
	u32 height;
	u32 pad;
};

struct EXR_CPU
{
	std::string name { "" };
	i32 width { 0 };
	i32 height { 0 };
	f32* data { nullptr };
};

struct DiskAsset
{
	std::filesystem::path path;
	std::string file_name;
};

namespace Assets
{
	void init();

	void import_mesh(const std::filesystem::path path);
	void import_texture(const std::filesystem::path path);
	void import_exr(const std::filesystem::path path);

	const EXR_CPU& get_exr_by_name(const std::string& name_with_extension);
	const EXR_CPU& get_exr_by_index(u32 index);

	u32 get_texture_count();

	void reconstruct_bvh(const std::string mesh);

	ComputeWriteBuffer& get_tris_compute_buffer();
	ComputeWriteBuffer& get_normals_compute_buffer();
	ComputeWriteBuffer& get_uvs_compute_buffer();
	ComputeWriteBuffer& get_bvh_compute_buffer();
	ComputeWriteBuffer& get_tri_idx_compute_buffer();
	ComputeWriteBuffer& get_mesh_header_buffer();
	ComputeWriteBuffer& get_texture_compute_buffer();
	ComputeWriteBuffer& get_texture_header_buffer();

	const std::vector<MeshHeader>& get_mesh_headers();
	const std::unordered_map<std::string, Mesh>& get_meshes();

	const std::vector<DiskAsset>& get_disk_files_by_extension(const std::string& extension);

	BVHNode get_root_bvh_node_of_mesh(u32 idx);
	const MeshHeader get_mesh_header(u32 idx);
}