#pragma once
#include <cstdint>
#include <string>

struct GLFWwindow;

namespace Raytracer::Input
{
	void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
	void cursor_callback(GLFWwindow* window, double xpos, double ypos);
}

namespace Raytracer
{
	void init();
	void terminate();
	void raytrace(int width, int height, uint32_t* buffer);
	void update(const float delta_time_ms);
	void ui();

	int get_target_fps();
};