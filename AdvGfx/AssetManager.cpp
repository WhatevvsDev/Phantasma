#include "AssetManager.h"

#include "Mesh.h"
#include "BVH.h"

#include <unordered_map>

struct
{
	std::unordered_map<std::string, Mesh> meshes;

	std::vector<MeshHeader> headers {};

	std::vector<Tri> consolidated_tris {};
	std::vector<glm::vec4> consolidated_normals {};
	std::vector<BVHNode> consolidated_nodes {};
	std::vector<u32> consolidated_tri_idxs {};

	ComputeWriteBuffer* tris_compute_buffer		{ nullptr };
	ComputeWriteBuffer* normals_compute_buffer	{ nullptr };
	ComputeWriteBuffer* bvh_compute_buffer		{ nullptr };
	ComputeWriteBuffer* tri_idx_compute_buffer	{ nullptr };

	ComputeWriteBuffer* header_compute_buffer	{ nullptr };

} internal;

void AssetManager::init()
{
	
}

void AssetManager::load_mesh(const std::filesystem::path path)
{
	internal.meshes.insert({
		path.string(), Mesh(path.string())
	});

	delete internal.tris_compute_buffer;
	delete internal.normals_compute_buffer;
	delete internal.bvh_compute_buffer;
	delete internal.tri_idx_compute_buffer;

	Mesh& loaded_mesh = internal.meshes.find(path.string())->second;

	// Create Mesh header for compute
	MeshHeader loaded_mesh_header;
	loaded_mesh_header.tris_count     = (u32)loaded_mesh.tris.size();
	loaded_mesh_header.normals_count  = (u32)loaded_mesh.normals.size();
	loaded_mesh_header.tri_idx_count  = (u32)loaded_mesh.bvh->triIdx.size();
	loaded_mesh_header.bvh_node_count = (u32)loaded_mesh.bvh->bvhNodes.size();

	// Tris
	loaded_mesh_header.tris_offset = (u32)internal.consolidated_tris.size();
	internal.consolidated_tris.reserve(loaded_mesh_header.tris_count);
	// TODO: This could potentially be slow 
	internal.consolidated_tris.insert(internal.consolidated_tris.end(), loaded_mesh.tris.begin(), loaded_mesh.tris.end());

	// Normals
	loaded_mesh_header.normals_offset = (u32)internal.consolidated_normals.size();
	internal.consolidated_normals.reserve(loaded_mesh_header.normals_count);
	// TODO: This could potentially be slow 
	internal.consolidated_normals.insert(internal.consolidated_normals.end(), loaded_mesh.normals.begin(), loaded_mesh.normals.end());

	// Tri Idx	
	loaded_mesh_header.tri_idx_offset = (u32)internal.consolidated_tri_idxs.size();
	internal.consolidated_tri_idxs.reserve(loaded_mesh_header.tri_idx_count);
	// TODO: This could potentially be slow 
	internal.consolidated_tri_idxs.insert(internal.consolidated_tri_idxs.end(), loaded_mesh.bvh->triIdx.begin(), loaded_mesh.bvh->triIdx.end());

	// BVH nodes
	loaded_mesh_header.root_bvh_node_idx = (u32)internal.consolidated_nodes.size();
	internal.consolidated_nodes.reserve(loaded_mesh_header.bvh_node_count);
	// TODO: This could potentially be slow 
	internal.consolidated_nodes.insert(internal.consolidated_nodes.end(), loaded_mesh.bvh->bvhNodes.begin(), loaded_mesh.bvh->bvhNodes.end());

	internal.headers.push_back(loaded_mesh_header);
	internal.header_compute_buffer = new ComputeWriteBuffer({internal.headers});

	internal.tris_compute_buffer	= new ComputeWriteBuffer({internal.consolidated_tris});
	internal.normals_compute_buffer	= new ComputeWriteBuffer({internal.consolidated_normals});
	internal.bvh_compute_buffer		= new ComputeWriteBuffer({internal.consolidated_nodes});
	internal.tri_idx_compute_buffer	= new ComputeWriteBuffer({internal.consolidated_tri_idxs});
}

void AssetManager::reconstruct_bvh(std::string mesh)
{
	internal.meshes.find(mesh)->second.reconstruct_bvh();
}

ComputeWriteBuffer& AssetManager::get_tris_compute_buffer()
{
	return *internal.tris_compute_buffer;
}

ComputeWriteBuffer& AssetManager::get_normals_compute_buffer()
{
	return *internal.normals_compute_buffer;
}

ComputeWriteBuffer& AssetManager::get_bvh_compute_buffer()
{
	return *internal.bvh_compute_buffer;
}

ComputeWriteBuffer& AssetManager::get_tri_idx_compute_buffer()
{
	return *internal.tri_idx_compute_buffer;
}

ComputeWriteBuffer& AssetManager::get_mesh_header_buffer()
{
	return *internal.header_compute_buffer;
}

std::vector<MeshHeader>& AssetManager::get_mesh_headers()
{
	return internal.headers;
}