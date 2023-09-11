#include "Raytracer.h"
#include "Math.h"
#include "Common.h"
#include "LogUtility.h"
#include "IOUtility.h"
#include "Compute.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include <GLFW/glfw3.h>

#include <utility>

struct Tri 
{ 
    glm::vec3 vertex0;
	float pad_0;
	glm::vec3 vertex1;
	float pad_1;
	glm::vec3 vertex2;
	float pad_2;
    glm::vec3 centroid;
	float pad_3;
};

struct Ray 
{ 
	glm::vec3 O {};
	float pad_0;
	glm::vec3 D {}; 
	float t = 1e30f; 
	uint ray_hit;
};

struct BVHNode
{
    glm::vec3 min;
    uint left_first;
    glm::vec3 max;
	uint tri_count;
};

 #define N 4

// TODO: Swap triangle to bouding box centroid, instead of vertex centroid :)

void UpdateNodeBounds( uint nodeIdx );
void Subdivide( uint nodeIdx );

// application data
Tri tris[N];
uint triIdx[N];
BVHNode bvhNode[N * 2];
uint rootNodeIdx = 0, nodes_used = 1;

void BuildBVH()
{
	// populate triangle index array
	for (int i = 0; i < N; i++) triIdx[i] = i;
	// calculate triangle centroids for partitioning
	for (int i = 0; i < N; i++)
		tris[i].centroid = (tris[i].vertex0 + tris[i].vertex1 + tris[i].vertex2) * 0.3333f;
	// assign all triangles to root node
	BVHNode& root = bvhNode[rootNodeIdx];
	root.left_first = 0, root.tri_count = N;
	UpdateNodeBounds( rootNodeIdx );
	// subdivide recursively
	Subdivide( rootNodeIdx );
}

void UpdateNodeBounds( uint nodeIdx )
{
	BVHNode& node = bvhNode[nodeIdx];
	node.min = glm::vec3( 1e30f );
	node.max = glm::vec3( -1e30f );
	for (uint first = node.left_first, i = 0; i < node.tri_count; i++)
	{
		uint leafTriIdx = triIdx[first + i];
		Tri& leafTri = tris[leafTriIdx];
		node.min = glm::min( node.min, leafTri.vertex0 ),
		node.min = glm::min( node.min, leafTri.vertex1 ),
		node.min = glm::min( node.min, leafTri.vertex2 ),
		node.max = glm::max( node.max, leafTri.vertex0 ),
		node.max = glm::max( node.max, leafTri.vertex1 ),
		node.max = glm::max( node.max, leafTri.vertex2 );
	}
}

void Subdivide( uint nodeIdx )
{
	// terminate recursion
	BVHNode& node = bvhNode[nodeIdx];
	if (node.tri_count <= 2) return;
	// determine split axis and position
	glm::vec3 extent = node.max - node.min;
	int axis = 0;
	if (extent.y > extent.x) axis = 1;
	if (extent.z > extent[axis]) axis = 2;
	float splitPos = node.min[axis] + extent[axis] * 0.5f;
	// in-place partition
	int i = node.left_first;
	int j = i + node.tri_count - 1;
	while (i <= j)
	{
		if (tris[triIdx[i]].centroid[axis] < splitPos)
			i++;
		else
			std::swap( triIdx[i], triIdx[j--] );
	}
	// abort split if one of the sides is empty
	int leftCount = i - node.left_first;
	if (leftCount == 0 || leftCount == (int)node.tri_count) return;
	// create child nodes
	int leftChildIdx = nodes_used++;
	int rightChildIdx = nodes_used++;

	bvhNode[leftChildIdx].left_first = node.left_first;
	bvhNode[leftChildIdx].tri_count = leftCount;
	bvhNode[rightChildIdx].left_first = i;
	bvhNode[rightChildIdx].tri_count = node.tri_count - leftCount;

	node.left_first = leftChildIdx;
	node.tri_count = 0;

	UpdateNodeBounds( leftChildIdx );
	UpdateNodeBounds( rightChildIdx );
	// recurse
	Subdivide( leftChildIdx );
	Subdivide( rightChildIdx );
}

struct SceneData
{
	uint resolution[2]	{ 0, 0 };
	uint tri_count		{ 0 };
	uint dummy			{ 0 };
	glm::vec4 cam_pos	{ 0.0f };
	glm::vec4 cam_forward;
	glm::vec4 cam_right;
	glm::vec4 cam_up;
} sceneData;

// Temporary scuffed input
bool move_w = false;
bool move_a = false;
bool move_s = false;
bool move_d = false;
bool move_space = false;
bool move_lctrl = false;
float mouse_x = 0.0f;
float mouse_y = 0.0f;
glm::vec3 cam_rotation;
bool mouse_active = false;
bool screenshot = false;

void Raytracer::key_input(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// Unused parameters
	(void)window;
	(void)scancode;
	(void)action;
	(void)mods;

	if(action != GLFW_PRESS && action != GLFW_RELEASE)
		return;

	bool is_pressed = (action == GLFW_PRESS);

	switch(key)
	{
		case GLFW_KEY_W:
		move_w = is_pressed;
		break;
		case GLFW_KEY_A:
		move_a = is_pressed;
		break;
		case GLFW_KEY_S:
		move_s = is_pressed;
		break;
		case GLFW_KEY_D:
		move_d = is_pressed;
		break;
		case GLFW_KEY_P:
		if(is_pressed)
			screenshot = true;
		break;
		case GLFW_KEY_SPACE:
		move_space = is_pressed;
		break;
		case GLFW_KEY_LEFT_CONTROL:
		move_lctrl = is_pressed;
		break;
		case GLFW_KEY_ESCAPE:
		exit(0);
		break;
		case GLFW_KEY_F1:
		if(is_pressed)
			mouse_active = !mouse_active;
		break;
	}

	if(mouse_active)
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	else
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}	

void Raytracer::mouse_button_input(GLFWwindow* window, int button, int action, int mods)
{
	// Unused parameters
	(void)window;
	(void)button;
	(void)action;
	(void)mods;
}

void Raytracer::cursor_input(GLFWwindow* window, double xpos, double ypos)
{
	// Ignore first input to prevent janky motion.
	static bool first_input = true;
	if(first_input = false)
		return;

	if(mouse_active)
		return;

	// Unused parameters
	(void)window;

	static double last_xpos = 0.0;
	static double last_ypos = 0.0;

	mouse_x = xpos - last_xpos;
	mouse_y = ypos - last_ypos;

	cam_rotation += glm::vec3(-mouse_y, -mouse_x, 0) * 0.1f;

	// limit pitch
	if(fabs(cam_rotation.x) > 89.9f)
	{
		cam_rotation.x = 89.9f * sgn(cam_rotation.x);
	}

	last_xpos = xpos;
	last_ypos = ypos;
}

struct
{
	bool recompile_changed_shaders_automatically { true };
} settings;


namespace Raytracer
{	
	ComputeOperation* perform_raytracing;

	void init()
	{
		glm::vec3 a( -100, 0, -100 );
		glm::vec3 b( 100, 0, -100 );
		glm::vec3 c( -100, 0, 100 );
		glm::vec3 d( 100, 0, 100 );

		tris[0].vertex0 = a;
		tris[0].vertex1 = b;
		tris[0].vertex2 = c;
		tris[1].vertex0 = b;
		tris[1].vertex1 = c;
		tris[1].vertex2 = d;

		tris[2].vertex0 = glm::vec3( 0, 0, 5 );
		tris[2].vertex1 = glm::vec3( 10, 0, 5 );
		tris[2].vertex2 = glm::vec3( 5, 5, 5 );

        BuildBVH();
		Compute::create_kernel("C:/Users/Matt/Desktop/AdvGfx/AdvGfx/compute/raytrace_tri.cl", "raytrace");
	}
	
	void update(const float delta_time_ms)
	{
		int moveHor = (move_d) - (move_a);
		int moveVer = (move_space) - (move_lctrl);
		int moveWard = (move_w) - (move_s);
		
		glm::mat4 rotation = glm::eulerAngleXYZ(glm::radians(cam_rotation.x), glm::radians(cam_rotation.y), glm::radians(cam_rotation.z));

		sceneData.cam_forward = glm::vec4(0, 0, 1.0f, 0) * rotation;
		sceneData.cam_right = glm::vec4(1.0f, 0, 0, 0) * rotation;
		sceneData.cam_up = glm::vec4(0, 1.0f, 0, 0) * rotation;

		glm::vec3 dir = 
			sceneData.cam_forward * moveWard + 
			sceneData.cam_up * moveVer + 
			sceneData.cam_right * moveHor;
		glm::normalize(dir);

		sceneData.cam_pos += glm::vec4(dir * delta_time_ms * 0.01f, 0.0f);

		if(settings.recompile_changed_shaders_automatically)
			Compute::recompile_kernels(ComputeKernelRecompilationCondition::SourceChanged);
	}

	void raytrace(int width, int height, uint32_t* buffer)
	{
		sceneData.resolution[0] = width;
		sceneData.resolution[1] = height;
		sceneData.tri_count = N;

		ComputeOperation("raytrace_tri.cl")
			.read({buffer, (size_t)(width * height)})
			.write({&sceneData, 1})
			.write({tris, N})
			.write({bvhNode, N * 2})
			.write({triIdx, N})
			.global_dispatch({width, height, 1})
			.execute();

		if(screenshot)
		{
			stbi_flip_vertically_on_write(true);
			stbi_write_jpg("render.jpg", width, height, 4, buffer, width * 4 );
			stbi_flip_vertically_on_write(false);
			LOGMSG(Log::MessageType::Debug, "Saved screenshot.");
			screenshot = false;
		}
    }

	void ui()
	{
		if(!mouse_active)
			return;

		auto latest_msg = Log::get_latest_msg();

		auto message_color = (latest_msg.second == Log::MessageType::Error) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 255, 255);

		ImGui::GetForegroundDrawList()->AddText(ImVec2(10, 10), message_color,latest_msg.first.data() ,latest_msg.first.data() + latest_msg.first.length());


		ImGui::Begin("Debug");

		if (ImGui::Button("Recompile Shaders"))
			Compute::recompile_kernels(ComputeKernelRecompilationCondition::Force);

		if (ImGui::Button("Recompile Changed Shaders"))
			Compute::recompile_kernels(ComputeKernelRecompilationCondition::SourceChanged);

		ImGui::Checkbox("Automatically recompile changed shaders?", &settings.recompile_changed_shaders_automatically);

		ImGui::End();
	}
}