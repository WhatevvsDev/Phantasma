#pragma once
#include <cstdint>
#include <string>

#include "LogUtility.h"
#include <assert.h>

struct GLFWwindow;

namespace Raytracer::Input
{
	void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
}

struct RaytracerInitDesc
{
	unsigned int width_px { 0 };
	unsigned int height_px { 0 };
	uint32_t* screen_buffer_ptr { nullptr };

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
	unsigned int width_px { 0 };
	unsigned int height_px { 0 };
	uint32_t* new_buffer_ptr { nullptr }; // Optional

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
	void update(const float delta_time_ms);
	void ui();

	int get_target_fps();
};