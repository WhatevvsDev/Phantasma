#include "Mesh.h"
#include "LogUtility.h"

#include "BVH.h"

#include <stb_image.h>
#include <stb_image_write.h>

#define TINYGLTF_NOEXCEPTION
//#define TINYGLTF_NO_STB_IMAGE
//#define TINYGLTF_NO_STB_IMAGE_WRITE
//#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_INCLUDE_JSON
#define TINYGLTF_NO_INCLUDE_RAPIDJSON
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#define TINYGLTF_USE_CPP14
#define TINYGLTF_IMPLEMENTATION
#include "json.hpp"
#include "tiny_gltf.h"

#include "Timer.h"
#include "LogUtility.h"
#include "PrimitiveTypes.h"
#include "IOUtility.h"

Timer build_timer;

Mesh::Mesh(const std::string& path)
{
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;

	std::string error;
	std::string warning;

	// Loading mesh 
	loader.LoadASCIIFromFile(&model, &error, &warning, path);

	if(!error.empty())
		LOGERROR(error)

	if(!warning.empty())
		LOGDEFAULT(warning)

	auto primitive = model.meshes[0].primitives[0];

	// Vertex position
	auto& vertex_position_accessor = model.accessors[primitive.attributes["POSITION"]];
	auto& vertex_position_buffer = model.bufferViews[vertex_position_accessor.bufferView];

	int vertex_position_data_size = tinygltf::GetNumComponentsInType(vertex_position_accessor.type) * tinygltf::GetComponentSizeInBytes(vertex_position_accessor.componentType);
	int vertex_position_count = (int)vertex_position_buffer.byteLength / vertex_position_data_size;

	// Vertex normals
	auto& vertex_normal_accessor = model.accessors[primitive.attributes["NORMAL"]];
	auto& vertex_normal_buffer = model.bufferViews[vertex_normal_accessor.bufferView];

	// Indices
	auto& index_accessor = model.accessors[primitive.indices];
	auto& index_buffer_view = model.bufferViews[index_accessor.bufferView];

	int index_data_size = tinygltf::GetNumComponentsInType(index_accessor.type) * tinygltf::GetComponentSizeInBytes(index_accessor.componentType);
	size_t index_count = index_buffer_view.byteLength / index_data_size;

	// TODO: Do this properly
	bool indices_exist = (index_buffer_view.byteLength != 0);

	if(!indices_exist)
		index_count = vertex_position_count;
		
	tris.resize(index_count / 3);
	normals.resize(index_count);

	std::vector<int> indices;
	
	if(indices_exist)
		indices = reinterpret_gltf_data_primitive_buffer_as_vector<int>(index_accessor, model);

	auto vert_pos_buf = model.buffers[vertex_position_buffer.buffer].data.data();
	auto vert_nor_buf = model.buffers[vertex_normal_buffer.buffer].data.data();

	for(int i = 0, t = 0; i < index_count; i += 3, t++)
	{
		int tri_indices[3] = { i + 0, i + 1, i + 2};
		// Indices into the vertex buffer for each vertex of a triangle

		if(indices_exist)
			for(int j = 0; j < 3; j++)
				tri_indices[j] = indices[i + j];


		auto pos_buf = (glm::vec3*) &vert_pos_buf[vertex_position_buffer.byteOffset];
		auto normal_buf = (glm::vec3*) &vert_nor_buf[vertex_normal_buffer.byteOffset];

		tris[t].vertex0 = pos_buf[tri_indices[0]];
		tris[t].vertex1 = pos_buf[tri_indices[1]];
		tris[t].vertex2 = pos_buf[tri_indices[2]];
			
		normals[i + 0] = glm::vec4(normal_buf[tri_indices[0]], 0);
		normals[i + 1] = glm::vec4(normal_buf[tri_indices[1]], 0);
		normals[i + 2] = glm::vec4(normal_buf[tri_indices[2]], 0);
	}

	build_timer.start();
	bvh = new BVH(tris);
	LOGDEBUG(std::format("Built BVH for {} in {} ms", get_file_name_from_path_string(path), (u32)build_timer.to_now()));

}

void Mesh::reconstruct_bvh()
{
	delete bvh;
	bvh = new BVH(tris);
}