#include "World.h"
#include "Assets.h"
#include "JSONUtility.h"
#include "Camera.h"

#include <fstream>

struct
{
	WorldDeviceData device_data;
	std::vector<MeshInstanceHeader> mesh_instances {};

	Material default_material = {glm::vec4(1.0f, 0.5f, 0.7f, 0.0f), 1.5f, 0.1f, MaterialType::Diffuse, 0.0f, 0.0f, 0.0f};
	std::vector<Material> materials = {default_material};
} internal;


i32 World::add_instance_of_mesh(u32 mesh_idx)
{
	MeshInstanceHeader new_mesh_instance;
	new_mesh_instance.transform = glm::identity<glm::mat4>();
	new_mesh_instance.inverse_transform = glm::inverse(new_mesh_instance.transform);
	new_mesh_instance.mesh_idx = mesh_idx;

	internal.mesh_instances.push_back(new_mesh_instance);

	return ((i32)internal.mesh_instances.size() - 1);
}

void World::remove_mesh_instance(i32 instance_idx)
{
	internal.mesh_instances.erase(internal.mesh_instances.begin() + instance_idx);
}

MeshInstanceHeader& World::get_mesh_device_data(usize instance_idx)
{
	return internal.mesh_instances[instance_idx];
}

Material& World::add_material()
{
	internal.materials.push_back(internal.default_material);
	return internal.materials.at(internal.materials.size() - 1);
}

Material& World::get_material_ref(u32 material_idx)
{
	return internal.materials[material_idx];
}

u32 World::get_material_count()
{
	return (u32)internal.materials.size();
}

std::vector<Material>& World::get_material_vector()
{
	return internal.materials;
}

WorldDeviceData& World::get_world_device_data()
{
	memset(internal.device_data.mesh_instances, 0, 1024 * sizeof(MeshInstanceHeader));
	memcpy(internal.device_data.mesh_instances, internal.mesh_instances.data(), internal.mesh_instances.size() * sizeof(MeshInstanceHeader));
	internal.device_data.mesh_instance_count = (u32)internal.mesh_instances.size();

	return internal.device_data;
}
void World::serialize_scene()
{
	json scene_data;

	scene_data["MeshInstanceHeaders"] = internal.mesh_instances;
	scene_data["Materials"] = internal.materials;

	std::ofstream o("phantasma.scene.json");
	o << scene_data << std::endl;
}

void World::deserialize_scene()
{
	std::ifstream f("phantasma.scene.json");

	bool file_opened_successfully = f.good();

	if(file_opened_successfully)
	{
		json scene_data = json::parse(f);
		internal.mesh_instances = scene_data["MeshInstanceHeaders"];

		if(scene_data.find("Materials") != scene_data.end())
			internal.materials = scene_data["Materials"];
	}
}