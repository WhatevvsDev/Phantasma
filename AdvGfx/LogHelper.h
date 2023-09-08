#pragma once

#include <string>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

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

	inline void print(MessageType aType, const char* aFile, int aLineNumber, const std::string& aMessage, bool aAssertion = true)
	{
		// If assertion didn't pass, throw an exception
		if(!aAssertion)
			throw std::exception(aMessage.c_str());

		HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);

		// Modify color of text
		WORD attribute = type_to_color(aType);
		SetConsoleTextAttribute(handle, attribute);

		// Get only file name
		std::string fileName { aFile };
		fileName = fileName.substr(fileName.find_last_of("/\\") + 1).c_str();

		// Print and reset color
		printf("[%s: %i] - %s\n", fileName.c_str(), aLineNumber, aMessage.c_str());
		SetConsoleTextAttribute(handle, 15);
	}
}

#define LOGMSG(type, message) Log::print(type, __FILE__, __LINE__, message, true);