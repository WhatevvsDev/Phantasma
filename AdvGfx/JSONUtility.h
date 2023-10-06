#pragma once
#include "json.hpp"
using json = nlohmann::json;

#ifndef TryFromJSONValPtr
// For values that might not exist (text attribute, but NOT for stuff like node_id)
#define TryFromJSONVal(lookIn, saveTo, varName) \
if(lookIn[#saveTo].find(#varName) != lookIn[#saveTo].end()) \
{\
	lookIn[#saveTo].at(#varName).get_to(saveTo.varName);\
} \
else \
{\
	LOGDEBUG(("Couldn't load json attribute " #varName));\
}
#endif

#define ToJSONVal(lookIn, saveTo, varName) \
lookIn[#saveTo][#varName] = saveTo.varName;