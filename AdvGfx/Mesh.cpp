#include "Mesh.h"

#include "BVH.h"

Timer mesh_build_timer;

i32 get_gltf_type_size(tinygltf::Accessor accessor)
{
	return tinygltf::GetNumComponentsInType(accessor.type) * tinygltf::GetComponentSizeInBytes(accessor.componentType);
}

Mesh::Mesh(const std::string& path)
{
	mesh_build_timer.start();

	tinygltf::Model model;
	tinygltf::TinyGLTF loader;

	std::string error;
	std::string warning;

	// Loading mesh 
	loader.LoadASCIIFromFile(&model, &error, &warning, path);

	if (!error.empty())
		LOGERROR(error)

	if (!warning.empty())
		LOGDEFAULT(warning)

	f32 imported_file_ms = mesh_build_timer.lap_delta();

	auto primitive = model.meshes[0].primitives[0];

	// Vertex position
	auto& vertex_position_accessor = model.accessors[primitive.attributes["POSITION"]];
	auto& vertex_position_buffer = model.bufferViews[vertex_position_accessor.bufferView];

	int vertex_position_data_size = get_gltf_type_size(vertex_position_accessor);
	int vertex_position_count = (int)vertex_position_buffer.byteLength / vertex_position_data_size;

	// Vertex normals
	auto& vertex_normal_accessor = model.accessors[primitive.attributes["NORMAL"]];
	auto& vertex_normal_buffer = model.bufferViews[vertex_normal_accessor.bufferView];

	// Vertex UVs
	has_uvs = primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end();

	auto& vertex_uv_accessor = model.accessors[primitive.attributes["TEXCOORD_0"]];
	auto& vertex_uv_buffer = model.bufferViews[vertex_uv_accessor.bufferView];

	// Indices
	auto& index_accessor = model.accessors[primitive.indices];
	auto& index_buffer_view = model.bufferViews[index_accessor.bufferView];

	int index_data_size = get_gltf_type_size(index_accessor);
	size_t index_count = index_buffer_view.byteLength / index_data_size;

	bool indices_exist = (index_buffer_view.byteLength != 0);

	if(!indices_exist)
		index_count = vertex_position_count;
		
	tris.resize(index_count / 3);
	vertex_data.resize(index_count);

	std::vector<int> indices;
	
	if(indices_exist)
		indices = reinterpret_gltf_data_primitive_buffer_as_vector<int>(index_accessor, model);

	auto vert_pos_buf = model.buffers[vertex_position_buffer.buffer].data.data();
	auto vert_nor_buf = model.buffers[vertex_normal_buffer.buffer].data.data();
	auto vert_uv_buf = model.buffers[vertex_uv_buffer.buffer].data.data();

	// TODO: This entire model loading part is so ugly and convoluted, fix this
	// Edit: its been made slightly less horrible, but still needs a redo

	for(int i = 0, t = 0; i < index_count; i += 3, t++)
	{
		for (i32 j = 0; j < 3; j++)
		{
			auto pos_buf =		(glm::vec3*)&vert_pos_buf[vertex_position_buffer.byteOffset];
			auto normal_buf =	(glm::vec3*)&vert_nor_buf[vertex_normal_buffer.byteOffset];
			auto uv_buf =		(glm::vec2*)&vert_uv_buf[vertex_uv_buffer.byteOffset];

			i32 index = indices_exist
				? (indices[i + j])
				: (i + j);

			// To keep proper padding, vertices[] is glm::vec4
			tris[t].vertices[j] = glm::vec4(pos_buf[index], tris[t].vertices[j].a);

			vertex_data[i + j].normal = normal_buf[index];

			if (has_uvs)
				vertex_data[i + j].uv = uv_buf[index];

		}
	}

	std::string file_name_with_extension = path.substr(path.find_last_of("/\\") + 1);
	name = file_name_with_extension;

	f32 parsed_data_time_ms = mesh_build_timer.lap_delta();

	reconstruct_bvh();

	f32 built_bvh_time_ms = mesh_build_timer.lap_delta();

	LOGDEBUG(std::format("New mesh {} Imported in {} ms | Parsed in {} ms | Build BVH in {} ms", get_file_name_from_path_string(path), (u32)imported_file_ms, (u32)parsed_data_time_ms, (u32)built_bvh_time_ms));
}

void Mesh::reconstruct_bvh()
{
	delete bvh;
	bvh = new BVH();

	BuildBLAS(*bvh, tris);
}