#pragma once
#include "PrimitiveTypes.h"

#include <vector>
#include "Math.h"

struct MeshInstanceHeader
{
	glm::mat4 transform;
	glm::mat4 inverse_transform;
		
	u32 mesh_idx;
};

struct WorldManagerDeviceData
{
	u32 mesh_count;
	MeshInstanceHeader instances[256];
};

namespace WorldManager
{
	void add_instance_of_mesh(u32 mesh_idx);

	WorldManagerDeviceData& get_world_device_data();
}