#pragma once
#include "Math.h"

struct MeshInstanceHeader
{
	glm::mat4 transform;
	glm::mat4 inverse_transform;
		
	u32 mesh_idx;
	u32 material_idx { 0 };
};

struct WorldManagerDeviceData
{
	u32 instance_count;
	MeshInstanceHeader instances[4096];
};

namespace WorldManager
{
	// Returns index of object
	int add_instance_of_mesh(u32 mesh_idx);

	WorldManagerDeviceData& get_world_device_data();
}