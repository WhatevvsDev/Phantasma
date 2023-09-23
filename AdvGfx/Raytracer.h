#pragma once
#include <cstdint>
#include <string>

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

	bool validate() const;
};

struct RaytracerResizeDesc
{
	unsigned int width_px { 0 };
	unsigned int height_px { 0 };
	uint32_t* new_buffer_ptr { nullptr }; // Optional

	bool validate() const;
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