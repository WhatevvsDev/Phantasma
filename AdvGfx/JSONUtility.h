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
	LOGMSG(Log::MessageType::Debug,("Couldn't load json attribute " #varName));\
}
#endif