#include "AssetManager.h"

#include "BVH.h"

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

	std::unordered_map<std::string, std::vector<DiskAsset>> disk_assets {};

} internal;

// Search for, and automatically compile compute shaders
void find_disk_assets()
{
	std::string assets_directory = get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\";
	for (const auto & asset_path : std::filesystem::recursive_directory_iterator(assets_directory))
	{
		std::string file_path = asset_path.path().string();
		std::string file_name_with_extension = file_path.substr(file_path.find_last_of("/\\") + 1);
		std::string file_extension = file_name_with_extension.substr(file_name_with_extension.find_last_of(".") + 1);
		std::string file_name = file_name_with_extension.substr(0, file_name_with_extension.length() - file_extension.length() - 1);

		bool file_is_common_shader_source = (file_name == "common");

		DiskAsset disk_asset;
		disk_asset.file_name = file_name_with_extension;
		disk_asset.path = file_path;

		if(file_is_common_shader_source)
			continue;

		switch(hashstr(file_extension.c_str()))
		{
			default:
			{
				// Ignore the file
				continue;
			}
			case hashstr("cl"):
			{
				if(!Compute::kernel_exists(file_name))
					Compute::create_kernel(file_path, file_name);

				break;
			}
			case hashstr("exr"):
			{
				// Do nothing, it will get entered after the switch
				break;
			}
			case hashstr("gltf"):
			{
				AssetManager::load_mesh(file_path);
				break;
			}
		}

		internal.disk_assets[file_extension].push_back(disk_asset);
	}
}

void AssetManager::init()
{
	find_disk_assets();
}

std::vector<DiskAsset>& AssetManager::get_disk_files_by_extension(const std::string& extension)
{
	// TODO: Don't know if theres a better way to do this?
	static std::vector<DiskAsset> empty_vector;

	auto assets_vector = internal.disk_assets.find(extension);

	if(assets_vector == internal.disk_assets.end())
		return empty_vector;
	
	return assets_vector->second;
}

void AssetManager::load_mesh(const std::filesystem::path path)
{
	std::string file_name = path.filename().string();

	internal.meshes.insert({
		file_name, Mesh(path.string())
	});

	delete internal.tris_compute_buffer;
	delete internal.normals_compute_buffer;
	delete internal.bvh_compute_buffer;
	delete internal.tri_idx_compute_buffer;

	Mesh& loaded_mesh = internal.meshes.find(file_name)->second;

	// Create Mesh header for compute
	MeshHeader loaded_mesh_header;
	loaded_mesh_header.tris_count     = (u32)loaded_mesh.tris.size();
	loaded_mesh_header.normals_count  = (u32)loaded_mesh.normals.size();
	loaded_mesh_header.tri_idx_count  = (u32)loaded_mesh.bvh->triIdx.size();
	loaded_mesh_header.bvh_node_count = (u32)loaded_mesh.bvh->bvhNodes.size();

	// Tris
	loaded_mesh_header.tris_offset = (u32)internal.consolidated_tris.size();
	internal.consolidated_tris.reserve(loaded_mesh_header.tris_count);
	internal.consolidated_tris.insert(internal.consolidated_tris.end(), loaded_mesh.tris.begin(), loaded_mesh.tris.end());

	// Normals
	loaded_mesh_header.normals_offset = (u32)internal.consolidated_normals.size();
	internal.consolidated_normals.reserve(loaded_mesh_header.normals_count);
	internal.consolidated_normals.insert(internal.consolidated_normals.end(), loaded_mesh.normals.begin(), loaded_mesh.normals.end());

	// Tri Idx	
	loaded_mesh_header.tri_idx_offset = (u32)internal.consolidated_tri_idxs.size();
	internal.consolidated_tri_idxs.reserve(loaded_mesh_header.tri_idx_count);
	internal.consolidated_tri_idxs.insert(internal.consolidated_tri_idxs.end(), loaded_mesh.bvh->triIdx.begin(), loaded_mesh.bvh->triIdx.end());

	// BVH nodes
	loaded_mesh_header.root_bvh_node_idx = (u32)internal.consolidated_nodes.size();
	internal.consolidated_nodes.reserve(loaded_mesh_header.bvh_node_count);
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

std::unordered_map<std::string, Mesh>& AssetManager::get_meshes()
{
	return internal.meshes;
}

BVHNode AssetManager::get_root_bvh_node_of_mesh(u32 idx)
{
	auto mesh_header = internal.headers[idx];

	return internal.consolidated_nodes[mesh_header.root_bvh_node_idx];
}