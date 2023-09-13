#pragma once
#include <cstdint>
#include <string>

struct GLFWwindow;

namespace Raytracer
{
	void init();
	void raytrace(int width, int height, uint32_t* buffer);
	void update(const float delta_time_ms);
	void ui();

	int get_target_fps();

	namespace Input
	{
		void key_input(GLFWwindow* window, int key, int scancode, int action, int mods);
		void mouse_button_input(GLFWwindow* window, int button, int action, int mods);
		void cursor_input(GLFWwindow* window, double xpos, double ypos);
		void scroll_input(GLFWwindow* window, double xoffset, double yoffset);
	};

	void import_lut(const std::string& path);
	void apply_lut(const std::string& name);
};