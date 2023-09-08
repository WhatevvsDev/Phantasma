#pragma once
#include <cstdint>

namespace Raytracer
{
	void init();
	void raytrace(int width, int height, uint32_t* buffer);
};