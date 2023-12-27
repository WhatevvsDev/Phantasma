#include "Assets.h"

#include "BVH.h"

#include <stb_image.h>

#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#define TINYEXR_USE_THREAD 1
#include "tinyexr.h"

struct
{

	// GPU Data Headers
	std::vector<MeshHeader> mesh_headers {};
	std::vector<TextureHeader> texture_headers {};

	// CPU Data
	std::unordered_map<std::string, EXR_CPU> exrs_cpu {};
	std::unordered_map<std::string, Mesh> meshes_cpu;

	std::vector<Tri> consolidated_tris {};
	std::vector<glm::vec4> consolidated_normals {};
	std::vector<glm::vec2> consolidated_uvs {};
	std::vector<BVHNode> consolidated_nodes {};
	std::vector<u32> consolidated_tri_idxs {};

	std::vector<u8> consolidated_textures {};

	ComputeWriteBuffer* tris_compute_buffer				{ nullptr };
	ComputeWriteBuffer* uvs_compute_buffer			{ nullptr };
	ComputeWriteBuffer* normals_compute_buffer			{ nullptr };
	ComputeWriteBuffer* bvh_compute_buffer				{ nullptr };
	ComputeWriteBuffer* tri_idx_compute_buffer			{ nullptr };

	ComputeWriteBuffer* mesh_header_compute_buffer		{ nullptr };

	ComputeWriteBuffer* texture_compute_buffer			{ nullptr };
	ComputeWriteBuffer* texture_header_compute_buffer	{ nullptr };

	std::unordered_map<std::string, std::vector<DiskAsset>> disk_assets {};

} internal;

// Search for, and automatically import assets from disk 
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
	
				bool is_not_folder = (file_extension != file_name);

				if(is_not_folder)
					LOGDEFAULT("Filetype not supported for " + file_path);

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
				Assets::import_exr(file_path);
				break;
			}
			case hashstr("gltf"):
			{
				Assets::import_mesh(file_path);
				break;
			}
			case hashstr("png"):
			case hashstr("jpg"):
			case hashstr("jpeg"):
			{
				Assets::import_texture(file_path);
				break;
			}
		}

		internal.disk_assets[file_extension].push_back(disk_asset);
	}
}

void Assets::init()
{
	find_disk_assets();
}

const std::vector<DiskAsset>& Assets::get_disk_files_by_extension(const std::string& extension)
{
	// TODO: Don't know if theres a better way to do this? (What i meant is an "empty reference")
	// EDIT: I guess this "works fine" now that I've made it const
	const static std::vector<DiskAsset> empty_vector;

	auto assets_vector = internal.disk_assets.find(extension);

	if(assets_vector == internal.disk_assets.end())
		return empty_vector;
	
	return assets_vector->second;
}

void Assets::import_mesh(const std::filesystem::path path)
{
	std::string file_name = path.filename().string();

	internal.meshes_cpu.insert({
		file_name, Mesh(path.string())
	});

	delete internal.tris_compute_buffer;
	delete internal.normals_compute_buffer;
	delete internal.uvs_compute_buffer;
	delete internal.bvh_compute_buffer;
	delete internal.tri_idx_compute_buffer;

	Mesh& loaded_mesh = internal.meshes_cpu.find(file_name)->second;

	// Create Mesh header for compute
	MeshHeader loaded_mesh_header;
	loaded_mesh_header.tris_count      = (u32)loaded_mesh.tris.size();
	loaded_mesh_header.normals_count   = (u32)loaded_mesh.normals.size();
	loaded_mesh_header.tri_idx_count   = (u32)loaded_mesh.bvh->primitive_idx.size();
	loaded_mesh_header.bvh_node_count  = (u32)loaded_mesh.bvh->nodes.size();

	u32 uvs_count	   = (u32)loaded_mesh.uvs.size();

	// Tris
	loaded_mesh_header.tris_offset = (u32)internal.consolidated_tris.size();
	internal.consolidated_tris.reserve(loaded_mesh_header.tris_count);
	internal.consolidated_tris.insert(internal.consolidated_tris.end(), loaded_mesh.tris.begin(), loaded_mesh.tris.end());

	// Normals
	loaded_mesh_header.normals_offset = (u32)internal.consolidated_normals.size();
	internal.consolidated_normals.reserve(loaded_mesh_header.normals_count);
	internal.consolidated_normals.insert(internal.consolidated_normals.end(), loaded_mesh.normals.begin(), loaded_mesh.normals.end());

	// UVs
	loaded_mesh_header.uvs_offset = (u32)internal.consolidated_uvs.size();
	internal.consolidated_uvs.reserve(uvs_count);
	internal.consolidated_uvs.insert(internal.consolidated_uvs.end(), loaded_mesh.uvs.begin(), loaded_mesh.uvs.end());

	// Tri Idx	
	loaded_mesh_header.tri_idx_offset = (u32)internal.consolidated_tri_idxs.size();
	internal.consolidated_tri_idxs.reserve(loaded_mesh_header.tri_idx_count);
	internal.consolidated_tri_idxs.insert(internal.consolidated_tri_idxs.end(), loaded_mesh.bvh->primitive_idx.begin(), loaded_mesh.bvh->primitive_idx.end());

	// BVH nodes
	loaded_mesh_header.root_bvh_node_idx = (u32)internal.consolidated_nodes.size();
	internal.consolidated_nodes.reserve(loaded_mesh_header.bvh_node_count);
	internal.consolidated_nodes.insert(internal.consolidated_nodes.end(), loaded_mesh.bvh->nodes.begin(), loaded_mesh.bvh->nodes.end());

	internal.mesh_headers.push_back(loaded_mesh_header);
	internal.mesh_header_compute_buffer = new ComputeWriteBuffer({internal.mesh_headers});

	internal.tris_compute_buffer	= new ComputeWriteBuffer({internal.consolidated_tris});
	internal.normals_compute_buffer	= new ComputeWriteBuffer({internal.consolidated_normals});
	internal.uvs_compute_buffer	= new ComputeWriteBuffer({internal.consolidated_uvs});
	internal.bvh_compute_buffer		= new ComputeWriteBuffer({internal.consolidated_nodes});
	internal.tri_idx_compute_buffer	= new ComputeWriteBuffer({internal.consolidated_tri_idxs});
}

void Assets::import_texture(const std::filesystem::path path)
{
	delete internal.texture_compute_buffer;
	delete internal.texture_header_compute_buffer;

	i32 channels;
	i32 width;
	i32 height;

	u8* data { nullptr } ;

	data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);

	LOGDEBUG(std::format("Loaded a texture with a size of {} x {} and {} channels", width, height, channels));

	std::vector<u8> pixels(data, data + width * height * 4);

	TextureHeader loaded_texture_header;
	loaded_texture_header.width = (u32)width;
	loaded_texture_header.height = (u32)height;
	loaded_texture_header.start_offset = (u32)internal.consolidated_textures.size();

	internal.consolidated_textures.reserve(width * height);
	internal.consolidated_textures.insert(internal.consolidated_textures.end(), pixels.begin(), pixels.end());
	internal.texture_compute_buffer	= new ComputeWriteBuffer({internal.consolidated_textures});

	internal.texture_headers.push_back(loaded_texture_header);

	internal.texture_header_compute_buffer = new ComputeWriteBuffer({internal.texture_headers});

	delete data;
}

void Assets::import_exr(const std::filesystem::path path)
{
	std::string file_name_with_extension = path.string().substr(path.string().find_last_of("/\\") + 1);

	auto& exr_entry = internal.exrs_cpu.insert({ file_name_with_extension, EXR_CPU() }).first->second;	

	const char* err = nullptr;

	delete[] exr_entry.data;
	LoadEXR(&exr_entry.data, &exr_entry.width, &exr_entry.height, path.string().c_str(), &err);

	LOGDEBUG(std::format("Loaded an exr with a size of {} x {}", exr_entry.width, exr_entry.height));

	if (err)
		LOGERROR(err);
}

const EXR_CPU& Assets::get_exr_by_name(const std::string& name_with_extension)
{
	return internal.exrs_cpu.find(name_with_extension)->second;
}

const EXR_CPU& Assets::get_exr_by_index(u32 index)
{
	return std::next(internal.exrs_cpu.begin(), index)->second;
}

u32 Assets::get_texture_count()
{
	return (u32)internal.texture_headers.size();
}

void Assets::reconstruct_bvh(std::string mesh)
{
	internal.meshes_cpu.find(mesh)->second.reconstruct_bvh();
}

// TODO: This is disgusting, find a better way
ComputeWriteBuffer& Assets::get_tris_compute_buffer()
{
	return *internal.tris_compute_buffer;
}

ComputeWriteBuffer& Assets::get_normals_compute_buffer()
{
	return *internal.normals_compute_buffer;
}

ComputeWriteBuffer& Assets::get_uvs_compute_buffer()
{
	return *internal.uvs_compute_buffer;
}

ComputeWriteBuffer& Assets::get_bvh_compute_buffer()
{
	return *internal.bvh_compute_buffer;
}

ComputeWriteBuffer& Assets::get_tri_idx_compute_buffer()
{
	return *internal.tri_idx_compute_buffer;
}

ComputeWriteBuffer& Assets::get_mesh_header_buffer()
{
	return *internal.mesh_header_compute_buffer;
}

ComputeWriteBuffer& Assets::get_texture_compute_buffer()
{
	return *internal.texture_compute_buffer;
}

ComputeWriteBuffer& Assets::get_texture_header_buffer()
{
	return *internal.texture_header_compute_buffer;
}

const std::vector<MeshHeader>& Assets::get_mesh_headers()
{
	return internal.mesh_headers;
}

const std::unordered_map<std::string, Mesh>& Assets::get_meshes()
{
	return internal.meshes_cpu;
}

BVHNode Assets::get_root_bvh_node_of_mesh(u32 idx)
{
	auto mesh_header = internal.mesh_headers[idx];

	return internal.consolidated_nodes[mesh_header.root_bvh_node_idx];
}

const MeshHeader Assets::get_mesh_header(u32 idx)
{
	return internal.mesh_headers[idx];
}