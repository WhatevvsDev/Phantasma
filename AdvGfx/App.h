#pragma once
#include "Common.h"

struct AppDesc
{
	int			width	{ 1600 };
	int			height	{ 1080 };
	std::string title	{ "Phantasma" };
};

namespace App
{
	int  init(AppDesc& desc);
	void update();
}