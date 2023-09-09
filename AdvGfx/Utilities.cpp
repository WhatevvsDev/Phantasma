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

void Log::print(Log::MessageType type, const char* file, int line_number, const std::string& message)
{
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);

	// Modify color of text
	WORD attribute = type_to_color(type);
	SetConsoleTextAttribute(handle, attribute);

	// Get only file name
	std::string fileName { file };
	fileName = fileName.substr(fileName.find_last_of("/\\") + 1).c_str();

	// Print and reset color
	printf("[%s: %i] - %s\n", fileName.c_str(), line_number, message.c_str());
	SetConsoleTextAttribute(handle, 15);
}

std::string read_file_to_string(const std::string& path)
{
	std::stringstream source_buffer;
    std::ifstream program_source(path);
    source_buffer << program_source.rdbuf();
    program_source.close();

	return source_buffer.str();
}