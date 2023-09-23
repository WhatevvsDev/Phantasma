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

	// Vertices
	auto& vertex_accessor = model.accessors[primitive.attributes["POSITION"]];
	auto& vertex_buffer = model.bufferViews[vertex_accessor.bufferView];

	int vertex_data_size = tinygltf::GetNumComponentsInType(vertex_accessor.type) * tinygltf::GetComponentSizeInBytes(vertex_accessor.componentType);
	int vertex_count = (int)vertex_buffer.byteLength / vertex_data_size;

	// Indices
	auto& index_accessor = model.accessors[primitive.indices];
	auto& index_buffer_view = model.bufferViews[index_accessor.bufferView];

	int index_data_size = tinygltf::GetNumComponentsInType(index_accessor.type) * tinygltf::GetComponentSizeInBytes(index_accessor.componentType);
	size_t index_count = index_buffer_view.byteLength / index_data_size;

	// TODO: Do this properly
	bool indices_exist = (index_buffer_view.byteLength != 0);

	if(!indices_exist)
		index_count = vertex_count;
		
	tris.resize(index_count / 3);

	std::vector<int> indices;
	
	if(indices_exist)
		indices = reinterpret_gltf_primitive_buffer_as_vector<int>(index_accessor, model);

	for(int i = 0, t = 0; i < index_count; i += 3, t++)
	{
		int tri_indices[3] = { i + 0, i + 1, i + 2};
		// Indices into the vertex buffer for each vertex of a triangle

		if(indices_exist)
			for(int j = 0; j < 3; j++)
				tri_indices[j] = indices[i + j];

		auto vert_buf = model.buffers[vertex_buffer.buffer].data.data();

		auto float_buf = (glm::vec3*) &vert_buf[vertex_buffer.byteOffset];

		tris[t].vertex0 = float_buf[tri_indices[0]];
		tris[t].vertex1 = float_buf[tri_indices[1]];
		tris[t].vertex2 = float_buf[tri_indices[2]];
	}

	bvh = new BVH(tris);
}

void Mesh::reconstruct_bvh()
{
	delete bvh;
	bvh = new BVH(tris);
}

Tri& Mesh::get_tri_ref(int idx)
{
	return tris[idx];
}
