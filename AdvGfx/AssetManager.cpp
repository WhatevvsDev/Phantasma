#include "AssetManager.h"

#include "Math.h"
#include "Mesh.h"
#include "BVH.h"

#include <unordered_map>

struct
{
	std::unordered_map<std::string, Mesh> meshes;

	std::vector<Tri> consolidated_tris;
	std::vector<glm::vec4> consolidated_normals;
	std::vector<BVHNode> consolidated_nodes;
	std::vector<uint> consolidated_tri_idxs;

	ComputeWriteBuffer* tris_compute_buffer		{ nullptr };
	ComputeWriteBuffer* normals_compute_buffer	{ nullptr };
	ComputeWriteBuffer* bvh_compute_buffer		{ nullptr };
	ComputeWriteBuffer* tri_idx_compute_buffer	{ nullptr };

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

	internal.tris_compute_buffer	= new ComputeWriteBuffer({loaded_mesh.tris});
	internal.normals_compute_buffer	= new ComputeWriteBuffer({loaded_mesh.normals});
	internal.bvh_compute_buffer		= new ComputeWriteBuffer({loaded_mesh.bvh->bvhNodes});
	internal.tri_idx_compute_buffer	= new ComputeWriteBuffer({loaded_mesh.bvh->triIdx});
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