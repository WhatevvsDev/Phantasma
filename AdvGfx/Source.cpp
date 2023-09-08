#include "App.h"

/*

	GLFW
	//OpenGL (Async Timewarp)
	OpenCL


	Process ->

	1. Raytrace scene (OpenCL)
	
	2. Save to OpenGL texture
	
	3. Async Timewarp

*/

int main()
{
	// We maintain default settings
	AppDesc desc;

	App::init(desc);
}