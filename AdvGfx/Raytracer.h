#pragma once

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
};

struct RaytracerResizeDesc
{
	u32 width_px { 0 };
	u32 height_px { 0 };
	u32* new_buffer_ptr { nullptr }; // Optional
};

namespace Raytracer
{
	void init(const RaytracerInitDesc& desc);
	void terminate();
	void resize(const RaytracerResizeDesc& desc);
	void raytrace();
	void update(const f32 delta_time_ms);
	void ui();

	bool ui_is_visible();
};