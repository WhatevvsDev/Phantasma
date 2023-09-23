#include "IOUtility.h"
#include "LogUtility.h"

#include <iostream>
#include <fstream>
#include <sstream>

std::string get_current_directory_path()
{
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	
	return std::string(buffer).substr(0, pos);
}

std::string latest_msg;
Log::MessageType latest_msg_type;

void Log::print(Log::MessageType type, const char* file, int line_number, const char* func, const std::string& message)
{
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);

	// Modify color of text
	WORD attribute = type_to_color(type);
	SetConsoleTextAttribute(handle, attribute);

	// Get only file name
	std::string fileName { file };
	fileName = fileName.substr(fileName.find_last_of("/\\") + 1).c_str();

	latest_msg = message;
	latest_msg_type = type;

	// Print and reset color
	printf("[%s: %i | %s] - %s\n", fileName.c_str(), line_number, func, message.c_str());
	SetConsoleTextAttribute(handle, 15);
}


std::pair<std::string, Log::MessageType> Log::get_latest_msg()
{
	return {latest_msg, latest_msg_type};
}

std::string read_file_to_string(const std::string& path)
{
	std::stringstream source_buffer;
    std::ifstream program_source(path);
    source_buffer << program_source.rdbuf();
    program_source.close();

	return source_buffer.str();
}