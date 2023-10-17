#pragma once
#include "PrimitiveTypes.h"

#include <vector>

namespace WorldManager
{
	struct MeshInstanceHeader
	{
		f32 transform[16];
		f32 inverse_transform[16];
		
		u32 mesh_id;
	};

	std::vector<MeshInstanceHeader> world_mesh_instances;
}