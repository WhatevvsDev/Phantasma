#pragma once
#include <cstdint>
#include <string>

struct GLFWwindow;

namespace Raytracer::Input
{
	void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
}

namespace Raytracer
{
	void init(uint32_t* screen_buffer_ptr, int width_px, int height_px);
	void resize(int width_px, int height_px);
	void terminate();
	void raytrace(int width, int height);
	void update(const float delta_time_ms);
	void ui();

	int get_target_fps();
};