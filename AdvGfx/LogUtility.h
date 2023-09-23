#pragma once

#include <string>
#include "strippedwindows.h"

namespace Log
{

	enum class MessageType
	{
		Default,
		Debug,
		Error,
		RANGE
	};

	namespace
	{
		// Get Windows Console specific attribute for different text color
		WORD type_to_color(MessageType aType)
		{
			switch (aType)
			{
			default:
			case MessageType::Default:
				return 14;
			case MessageType::Debug:
				return 8;
			case MessageType::Error:
				return 12;
			}
		}
	}

	void print(Log::MessageType type, const char* file, int line_number, const char* func, const std::string& message);
	
	std::pair<std::string, MessageType> get_latest_msg();
}

#define LOGMSG(type, message) Log::print(type, __FILE__, __LINE__, __func__,  message);
#define LOGDEBUG(message) Log::print(Log::MessageType::Debug, __FILE__, __LINE__, __func__, message);
#define LOGDEFAULT(message) Log::print(Log::MessageType::Default, __FILE__, __LINE__, __func__, message);
#define LOGERROR(message) Log::print(Log::MessageType::Error, __FILE__, __LINE__, __func__, message);