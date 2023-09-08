#pragma once
#include "Common.h"

struct AppDesc
{
	int			width	{ 1200 };
	int			height	{ 800 };
	std::string title	{ "Window"};
};

namespace App
{
	int  init(AppDesc& desc);
	void update();
	void render();
}