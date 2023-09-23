#pragma once
#include <vector>

#include "LogUtility.h"
#include "Math.h"
#include "tiny_gltf.h"

struct BVH;

struct Tri 
{ 
	glm::vec3 vertex0;
	float pad_0;
	glm::vec3 vertex1;
	float pad_1;
	glm::vec3 vertex2;
	float pad_2;
};

struct Mesh
{
	Mesh(const std::string& path);
	std::vector<Tri> tris;
	BVH* bvh;

	Tri& get_tri_ref(int idx);
	void reconstruct_bvh();
};

template <typename T>
inline std::vector<T> reinterpret_gltf_primitive_buffer_as_vector(tinygltf::Accessor& accessor, tinygltf::Model& model)
{
	auto& buffer_view = model.bufferViews[(size_t)accessor.bufferView];
	auto& buffer = model.buffers[buffer_view.buffer];

	int data_size = tinygltf::GetNumComponentsInType(accessor.type) * tinygltf::GetComponentSizeInBytes(accessor.componentType);
	int data_count = buffer_view.byteLength / data_size;

	std::vector<T> result;
	result.resize(data_count);

	void* data_ptr = (void*)&buffer.data[buffer_view.byteOffset];
				
	switch(accessor.componentType)
	{
		default:
			LOGERROR(std::format("Unsupported index type {}", accessor.componentType));
			break;
		case 5120: // signed byte
		{
			char* casted = (char*)data_ptr;
			for(int i = 0; i < data_count; i++)
				result[i] = casted[i];
			return result;
		}
		case 5121: // unsigned byte
		{
			unsigned char* casted = (unsigned char*)data_ptr;
			for(int i = 0; i < data_count; i++)
				result[i] = casted[i];
			return result;
		}
		case 5122: // signed short
		{
			short* casted = (short*)data_ptr;
			for(int i = 0; i < data_count; i++)
				result[i] = casted[i];
			return result;
		}
		case 5123: // unsigned short
		{
			unsigned short* casted = (unsigned short*)data_ptr;
			for(int i = 0; i < data_count; i++)
				result[i] = casted[i];
			return result;
		}
		case 5125: // unsigned int
		{
			unsigned int* casted = (unsigned int*)data_ptr;
			for(int i = 0; i < data_count; i++)
				result[i] = casted[i];
			return result;
		}
		case 5126: // float
		{
			float* casted = (float*)data_ptr;
			for(int i = 0; i < data_count; i++)
				result[i] = casted[i];
			return result;
		}
	}

	return result;
}