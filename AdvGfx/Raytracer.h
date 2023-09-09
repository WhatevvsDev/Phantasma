#pragma once
#include <cstdint>

struct GLFWwindow;

namespace Raytracer
{
	void init();
	void raytrace(int width, int height, uint32_t* buffer);

	void key_input(GLFWwindow* window, int key, int scancode, int action, int mods);
	void mouse_button_input(GLFWwindow* window, int button, int action, int mods);
	void cursor_input(GLFWwindow* window, double xpos, double ypos);
};