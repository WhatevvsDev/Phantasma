#include "WorldManager.h"
#include "AssetManager.h"

struct
{
	WorldManagerDeviceData device_data;
	std::vector<CameraInstance> cameras;
} internal;

int WorldManager::add_instance_of_mesh(u32 mesh_idx)
{
	MeshInstanceHeader new_mesh_instance;
	new_mesh_instance.transform = glm::identity<glm::mat4>();
	new_mesh_instance.inverse_transform = glm::inverse(new_mesh_instance.transform);
	new_mesh_instance.mesh_idx = mesh_idx;

	u32 idx = internal.device_data.mesh_instance_count;

	internal.device_data.mesh_instances[idx] = new_mesh_instance;

	internal.device_data.mesh_instance_count++;

	return (internal.device_data.mesh_instance_count - 1);
}

WorldManagerDeviceData& WorldManager::get_world_device_data()
{
	return internal.device_data;
}