#pragma once
#include <cstdint>
#include <string>

#include "PrimitiveTypes.h"

#include "LogUtility.h"
#include <assert.h>

struct GLFWwindow;

namespace Raytracer::Input
{
	void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
}

struct RaytracerInitDesc
{
	u32 width_px { 0 };
	u32 height_px { 0 };
	u32* screen_buffer_ptr { nullptr };

	inline bool validate() const
	{
		bool valid { true };

		assert((screen_buffer_ptr != nullptr) && "RaytracerInitDesc did not have screen_buffer_ptr set properly");
	
		if(width_px == 0)
		{
			LOGERROR("RaytracerInitDesc has an invalid width");
			return false;
		}

		if (height_px == 0)
		{
			LOGERROR("RaytracerInitDesc has an invalid height");
			return false;
		}

		return valid;
	}
};

struct RaytracerResizeDesc
{
	u32 width_px { 0 };
	u32 height_px { 0 };
	u32* new_buffer_ptr { nullptr }; // Optional

	inline bool validate() const
	{
		bool valid { true };
	
		if(width_px == 0)
		{
			LOGERROR("RaytracerResizeDesc has an invalid width");
			return false;
		}

		if (height_px == 0)
		{
			LOGERROR("RaytracerResizeDesc has an invalid height");
			return false;
		}

		return valid;
	}
};

namespace Raytracer
{
	void init(const RaytracerInitDesc& desc);
	void terminate();
	void resize(const RaytracerResizeDesc& desc);
	void raytrace();
	void update(const f32 delta_time_ms);
	void ui();

	u32 get_target_fps();
};