#include "WorldManager.h"
#include "AssetManager.h"
#include "JSONUtility.h"

#include <fstream>

struct
{
	WorldManagerDeviceData device_data;
	std::vector<CameraInstance> cameras;
	std::vector<MeshInstanceHeader> mesh_instances {};
} internal;


i32 WorldManager::add_instance_of_mesh(u32 mesh_idx)
{
	MeshInstanceHeader new_mesh_instance;
	new_mesh_instance.transform = glm::identity<glm::mat4>();
	new_mesh_instance.inverse_transform = glm::inverse(new_mesh_instance.transform);
	new_mesh_instance.mesh_idx = mesh_idx;

	internal.mesh_instances.push_back(new_mesh_instance);

	return ((i32)internal.mesh_instances.size() - 1);
}

void WorldManager::remove_mesh_instance(i32 instance_idx)
{
	internal.mesh_instances.erase(internal.mesh_instances.begin() + instance_idx);
}

MeshInstanceHeader& WorldManager::get_mesh_device_data(usize instance_idx)
{
	return internal.mesh_instances[instance_idx];
}

WorldManagerDeviceData& WorldManager::get_world_device_data()
{
	memset(internal.device_data.mesh_instances, 0, 4096 * sizeof(MeshInstanceHeader));
	memcpy(internal.device_data.mesh_instances, internal.mesh_instances.data(), internal.mesh_instances.size() * sizeof(MeshInstanceHeader));
	internal.device_data.mesh_instance_count = internal.mesh_instances.size();

	return internal.device_data;
}
void WorldManager::serialize_scene()
{
	json scene_data;

	scene_data["MeshInstanceHeaders"] = internal.mesh_instances;

	std::ofstream o("phantasma.scene.json");
	o << scene_data << std::endl;
}

void WorldManager::deserialize_scene()
{
	std::ifstream f("phantasma.scene.json");

	bool file_opened_successfully = f.good();

	if(file_opened_successfully)
	{
		json scene_data = json::parse(f);
		internal.mesh_instances = scene_data["MeshInstanceHeaders"];
	}
}