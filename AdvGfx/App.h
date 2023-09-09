#pragma once
#include "Common.h"

struct AppDesc
{
	int			width	{ 1200 };
	int			height	{ 800 };
	std::string title	{ "Phantasma" };
};

namespace App
{
	int  init(AppDesc& desc);
	void update();
}