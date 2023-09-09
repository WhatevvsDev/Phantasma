#pragma once
#include "Common.h"

struct AppDesc
{
	int			width	{ 640 };
	int			height	{ 640 };
	std::string title	{ "Phantasma" };
};

namespace App
{
	int  init(AppDesc& desc);
	void update();
}