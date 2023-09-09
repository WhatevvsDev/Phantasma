#pragma once
#include <unordered_map>
#include <string>

namespace Compute
{
	void init();

	void create_kernel(const std::string& path);
}