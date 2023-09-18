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
		LOGMSG(Log::MessageType::Error, error)

	if(!warning.empty())
		LOGMSG(Log::MessageType::Default, warning)

	auto primitive = model.meshes[0].primitives[0];

	// Vertices
	auto& vertex_accessor = model.accessors[primitive.attributes["POSITION"]];
	auto& vertex_buffer = model.bufferViews[vertex_accessor.bufferView];

	int vertex_data_size = tinygltf::GetNumComponentsInType(vertex_accessor.type) * tinygltf::GetComponentSizeInBytes(vertex_accessor.componentType);
	int vertex_count = vertex_buffer.byteLength / vertex_data_size;

	// Indices

	auto& index_accessor = model.accessors[primitive.indices];
	auto& index_buffer = model.bufferViews[index_accessor.bufferView];

	int index_data_size = tinygltf::GetNumComponentsInType(index_accessor.type) * tinygltf::GetComponentSizeInBytes(index_accessor.componentType);
	int index_count = index_buffer.byteLength / index_data_size;

	// TODO: Do this properly
	bool indices_exist = (index_buffer.byteStride == 0);

	if(!indices_exist)
		index_count = vertex_count;
		
	tris.resize(index_count / 3);

	for(int i = 0, t = 0; i < index_count; i += 3, t++)
	{
		// Indices into the vertex buffer for each vertex of a triangle
		int tri_indices[3] {};

		if(indices_exist)
		{
			memcpy(&tri_indices[0], &model.buffers[index_buffer.buffer].data[(i + 0) * index_data_size + index_buffer.byteOffset], index_data_size);
			memcpy(&tri_indices[1], &model.buffers[index_buffer.buffer].data[(i + 1) * index_data_size + index_buffer.byteOffset], index_data_size);
			memcpy(&tri_indices[2], &model.buffers[index_buffer.buffer].data[(i + 2) * index_data_size + index_buffer.byteOffset], index_data_size);

			/*
			switch(index_accessor.componentType)
			{
			default:
				LOGMSG(Log::MessageType::Error, std::format("Unhandled index type {}", index_type));
				break;
			case 5123:
				for(int v = 0; v < 3; v++)
					tri_indices[v] = (unsigned int)*(unsigned short*)model.buffers[index_buffer.buffer].data[(i + v) * index_buffer.byteStride];
				break;
			case 5125:
				for(int v = 0; v < 3; v++)
					tri_indices[v] = *(unsigned int*)model.buffers[index_buffer.buffer].data[(i + v) * index_buffer.byteStride];
				break;
			}
			*/
		}
		else
		{
			tri_indices[0] = i + 0;
			tri_indices[1] = i + 1;
			tri_indices[2] = i + 2;
		}

		auto vert_buf = model.buffers[vertex_buffer.buffer].data.data();

		auto float_buf = (glm::vec3*) &vert_buf[vertex_buffer.byteOffset];

		tris[t].vertex0 = float_buf[tri_indices[0]];
		tris[t].vertex1 = float_buf[tri_indices[1]];
		tris[t].vertex2 = float_buf[tri_indices[2]];

	}

	bvh = new BVH(tris);
}