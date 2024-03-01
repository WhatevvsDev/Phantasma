#pragma once
#include "Math.h"
#include "Material.h"

struct MeshInstanceHeader
{
	glm::mat4 transform {glm::identity<glm::mat4>()};
	glm::mat4 inverse_transform {glm::identity<glm::mat4>()};
		
	u32 mesh_idx { 0 };
	u32 material_idx { 0 };
	i32 texture_idx { -1 }; // TODO: We should support more textures
	u32 pad { 0 };

	// Needed to save/load vector of this
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(MeshInstanceHeader, 
		transform, 
		inverse_transform, 
		mesh_idx,
		material_idx,
		texture_idx);
};

struct WorldDeviceData
{
	u32 mesh_instance_count;
	u32 pad_0[3];
	MeshInstanceHeader mesh_instances[1024]; // TODO: we do this because you can't get the proper size of a struct if it has a std::vector in it. find a workaround?
};

namespace World
{
	// Returns index of object
	int add_instance_of_mesh(u32 mesh_idx);
	void remove_mesh_instance(i32 instance_idx);
	MeshInstanceHeader& get_mesh_device_data(usize instance_idx);

	Material& add_material();
	Material& get_material_ref(u32 material_idx);
	u32 get_material_count();
	std::vector<Material>& get_material_vector();

	WorldDeviceData& get_world_device_data();

	void serialize_scene();
	void deserialize_scene();
}