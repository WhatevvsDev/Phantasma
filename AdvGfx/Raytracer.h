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
	void init(uint32_t* screen_buffer_ptr);
	void terminate();
	void raytrace(int width, int height);
	void update(const float delta_time_ms);
	void ui();

	int get_target_fps();
};