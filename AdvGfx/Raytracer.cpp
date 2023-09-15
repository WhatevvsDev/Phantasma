#include "Raytracer.h"
#include "Math.h"
#include "Common.h"
#include "LogUtility.h"
#include "IOUtility.h"
#include "Compute.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_write.h>

#include <fstream>
#include <ostream>
#include "json.hpp"
using json = nlohmann::json;

#include <GLFW/glfw3.h>

#include <utility>
#include <format>

#include "BVH.h"

enum class TonemappingType
{
	None,
	ApproximateACES,
	//Filmic,
	Reinhard
};

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

 #define N 12582 

// application data
Tri tris[N];
uint triIdx[N];
BVHNode bvhNode[N * 2];
uint rootNodeIdx = 0, nodes_used = 1;

// TODO: Swap triangle to bouding box centroid, instead of vertex centroid :)

void build_bvh()
{
	// populate triangle index array
	for (int i = 0; i < N; i++) triIdx[i] = i;
	// calculate triangle centroids for partitioning
	for (int i = 0; i < N; i++)
		tris[i].centroid = (tris[i].vertex0 + tris[i].vertex1 + tris[i].vertex2) * 0.3333f;
	// assign all triangles to root node
	BVHNode& root = bvhNode[rootNodeIdx];
	root.left_first = 0, root.tri_count = N;
	update_node_bounds( rootNodeIdx );
	// subdivide recursively
	subdivide( rootNodeIdx );
}

void update_node_bounds( uint nodeIdx )
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

void subdivide( uint nodeIdx )
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

	update_node_bounds( leftChildIdx );
	update_node_bounds( rightChildIdx );
	// recurse
	subdivide( leftChildIdx );
	subdivide( rightChildIdx );
}

namespace Raytracer
{	
	struct SceneData
	{
		uint resolution[2]	{ 0, 0 };
		uint tri_count		{ 0 };
		uint dummy			{ 0 };
		glm::vec4 cam_pos	{ 0.0f };
		glm::vec4 cam_forward { 0.0f };
		glm::vec4 cam_right { 0.0f };
		glm::vec4 cam_up { 0.0f };
	} sceneData;

	struct
	{
		bool recompile_changed_shaders_automatically { true };
		bool fps_limit_enabled { true };
		int fps_limit_value { 80 };

		float camera_speed_t {0.5f};

		TonemappingType active_tonemapping { TonemappingType::None };
	} settings;

	namespace Input
	{
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
		float show_move_speed_timer = 0;

		void key_input(GLFWwindow* window, int key, int scancode, int action, int mods)
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

		void mouse_button_input(GLFWwindow* window, int button, int action, int mods)
		{
			// Unused parameters
			(void)window;
			(void)button;
			(void)action;
			(void)mods;
		}

		void cursor_input(GLFWwindow* window, double xpos, double ypos)
		{
			if(mouse_active)
				return;

			// Unused parameters
			(void)window;

			static double last_xpos = 0.0;
			static double last_ypos = 0.0;

			mouse_x = (float)(xpos - last_xpos);
			mouse_y = (float)(ypos - last_ypos);

			cam_rotation += glm::vec3(-mouse_y, -mouse_x, 0) * 0.1f;

			// limit pitch
			if(fabs(cam_rotation.x) > 89.9f)
			{
				cam_rotation.x = 89.9f * sgn(cam_rotation.x);
			}

			last_xpos = xpos;
			last_ypos = ypos;
		}

		void scroll_input(GLFWwindow* window, double xoffset, double yoffset)
		{
			(void)xoffset;
			(void)window;

		float old_camera_speed_t = settings.camera_speed_t;
		settings.camera_speed_t += (float)yoffset * 0.01f;
		settings.camera_speed_t = glm::fclamp(settings.camera_speed_t, 0.0f, 1.0f);
		if(settings.camera_speed_t != old_camera_speed_t)
			show_move_speed_timer = 2.0f;
	}
	}

	float camera_speed_t_to_m_per_second()
	{
		float speed = 0;
		float adjusted_t = settings.camera_speed_t * 2.0f - 1.0f;

		float value = pow(adjusted_t * 9.95f, 2.0f) * sgn(adjusted_t);

		if(settings.camera_speed_t > 0.5f)
		{
			speed = value + 1.0f;
		}
		else
		{
			speed = 1.0f / fabsf(value - 1.0f);
		}

		return glm::clamp(speed, 0.01f, 100.0f);
	}
	#pragma warning(disable:4996)
	void load_model()
	{
		FILE* file = fopen( "C:/Users/Matt/Desktop/AdvGfx/x64/Debug/unity.tri", "r" );
		float a, b, c, d, e, f, g, h, i;
		for (int t = 0; t < N; t++) 
		{
			fscanf_s( file, "%f %f %f %f %f %f %f %f %f\n", 
				&a, &b, &c, &d, &e, &f, &g, &h, &i );
			tris[t].vertex0 = glm::vec3( a, b, c );
			tris[t].vertex1 = glm::vec3( d, e, f );
			tris[t].vertex2 = glm::vec3( g, h, i );
		}
		fclose( file );
	}

	void init()
	{
		std::ifstream f("phantasma.settings.json");

		if(f.good())
		{
			json settings_data = json::parse(f);

			settings.active_tonemapping = settings_data["settings"]["active_tonemapping"];
			settings.camera_speed_t = settings_data["settings"]["camera_speed_t"];
			settings.fps_limit_enabled = settings_data["settings"]["fps_limit_enabled"];
			settings.fps_limit_value = settings_data["settings"]["fps_limit_value"];
			settings.recompile_changed_shaders_automatically = settings_data["settings"]["recompile_changed_shaders_automatically"];
		}

		// TODO: Implement the other tonemapping filters
		// Create tonemapping shaders
		Compute::create_kernel("C:/Users/Matt/Desktop/AdvGfx/AdvGfx/compute/reinhard_tonemapping.cl", "reinhard");
		Compute::create_kernel("C:/Users/Matt/Desktop/AdvGfx/AdvGfx/compute/approximate_aces_tonemapping.cl", "approximate_aces");
		//Compute::create_kernel("C:/Users/Matt/Desktop/AdvGfx/AdvGfx/compute/raytrace_tri.cl", "raytrace");

		/*
		for (int i = 0; i < N; i++)
		{
			glm::vec3 r0 = glm::vec3( RandomFloat(), RandomFloat(), RandomFloat() );
			glm::vec3 r1 = glm::vec3( RandomFloat(), RandomFloat(), RandomFloat() );
			glm::vec3 r2 = glm::vec3( RandomFloat(), RandomFloat(), RandomFloat() );
			tris[i].vertex0 = r0 * 250 - glm::vec3( 125 );
			tris[i].vertex1 = tris[i].vertex0 + r1, tris[i].vertex2 = tris[i].vertex0 + r2;
		}
		*/
		load_model();

        build_bvh();
		Compute::create_kernel("C:/Users/Matt/Desktop/AdvGfx/AdvGfx/compute/raytrace_tri.cl", "raytrace");
	}

	void terminate()
	{
		json settings_data;

		settings_data["settings"]["active_tonemapping"] = settings.active_tonemapping;
		settings_data["settings"]["camera_speed_t"] = settings.camera_speed_t;
		settings_data["settings"]["fps_limit_enabled"] = settings.fps_limit_enabled;
		settings_data["settings"]["fps_limit_value"] = settings.fps_limit_value;
		settings_data["settings"]["recompile_changed_shaders_automatically"] = settings.recompile_changed_shaders_automatically;

		std::ofstream o("phantasma.settings.json");
		o << settings_data << std::endl;
	}

	void update(const float delta_time_ms)
	{
		Input::show_move_speed_timer -= (delta_time_ms / 1000.0f);
		int moveHor = (Input::move_d) - (Input::move_a);
		int moveVer = (Input::move_space) - (Input::move_lctrl);
		int moveWard = (Input::move_w) - (Input::move_s);
		
		glm::mat4 rotation = glm::eulerAngleXYZ(glm::radians(Input::cam_rotation.x), glm::radians(Input::cam_rotation.y), glm::radians(Input::cam_rotation.z));

		sceneData.cam_forward = glm::vec4(0, 0, 1.0f, 0) * rotation;
		sceneData.cam_right = glm::vec4(1.0f, 0, 0, 0) * rotation;
		sceneData.cam_up = glm::vec4(0, 1.0f, 0, 0) * rotation;

		glm::vec3 dir = 
			sceneData.cam_forward * moveWard + 
			sceneData.cam_up * moveVer + 
			sceneData.cam_right * moveHor;
		glm::normalize(dir);

		sceneData.cam_pos += glm::vec4(dir * delta_time_ms * 0.01f, 0.0f) * camera_speed_t_to_m_per_second();

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
			.write({tris, N})
			.write({bvhNode, N * 2})
			.write({triIdx, N})
			.write({&sceneData, 1})
			.global_dispatch({width, height, 1})
			.execute();

		
		if(settings.active_tonemapping != TonemappingType::None)
		{
			ComputeOperation* op { nullptr};

			switch(settings.active_tonemapping)
			{
				case TonemappingType::Reinhard:
					op = new ComputeOperation("reinhard_tonemapping.cl");
					break;
				case TonemappingType::ApproximateACES:
					op = new ComputeOperation("approximate_aces_tonemapping.cl");
					break;
			}

			op->read_write({buffer, (size_t)(width * height)})
				.write({&sceneData, 1})
				.global_dispatch({width, height, 1})
				.execute();
			delete op;
		}

		if(Input::screenshot)
		{
			stbi_flip_vertically_on_write(true);
			stbi_write_jpg("render.jpg", width, height, 4, buffer, width * 4 );
			stbi_flip_vertically_on_write(false);
			LOGMSG(Log::MessageType::Debug, "Saved screenshot.");
			Input::screenshot = false;
		}
    }

	void ui()
	{
		auto& draw_list = *ImGui::GetForegroundDrawList();

		if(Input::show_move_speed_timer > 0 || Input::mouse_active)
		{
			{ // Movement speed bar

				static float camera_speed_visual_t;

				camera_speed_visual_t = glm::lerp(camera_speed_visual_t, settings.camera_speed_t, 0.65f);

				float move_bar_width = (float)sceneData.resolution[0] * 0.35f;
				float move_bar_padding = ((float)sceneData.resolution[0] - move_bar_width) * 0.5f;
				float move_bar_height = 3;
				{
					auto minpos = ImVec2(move_bar_padding, (float)sceneData.resolution[1] - move_bar_height - 32);
					auto maxpos = ImVec2(move_bar_padding + move_bar_width, (float)sceneData.resolution[1] - 32);

					draw_list.AddRectFilled(minpos, maxpos, IM_COL32(223, 223, 223, 255), 0.0f,  ImDrawCornerFlags_All);
					draw_list.AddRect(ImVec2(minpos.x - 1, minpos.y - 1), ImVec2(maxpos.x + 1, maxpos.y + 1), IM_COL32(32, 32, 32, 255), 0.0f,  ImDrawCornerFlags_All, 2.0f);
							
					std::string text = std::format("Camera speed: {} m/s", camera_speed_t_to_m_per_second());
					auto text_size = ImGui::CalcTextSize(text.c_str());
					ImGui::GetForegroundDrawList()->AddText(ImVec2(minpos.x + move_bar_width * 0.5f - text_size.x * 0.5f, minpos.y - 32 - text_size.y), IM_COL32(223, 223, 223, 255), text.data() ,text.data() + text.length());
				}
				// Movement speed bar, divisions
				int divisior_count = 5;
				for(int i = 0; i < divisior_count + 2; i++)
				{
					float cap_extra_height = 5;
					float cap_extra_width = 0;
					float divisor_extra_height = 7;

					if(i == 0 || i == divisior_count + 1){}
					else
					{
						cap_extra_height = 0;
						cap_extra_width = 0;
					}

					float hor_offset = move_bar_width * (float(i) / ((float)divisior_count + 1.0f));
					float width_half_extent = 2;

					auto minpos = ImVec2(move_bar_padding + hor_offset - width_half_extent - cap_extra_width, sceneData.resolution[1] - move_bar_height - 32 - cap_extra_height - divisor_extra_height *0.5f);
					auto maxpos = ImVec2(move_bar_padding + hor_offset + width_half_extent + cap_extra_width, sceneData.resolution[1] - 32 + cap_extra_height + divisor_extra_height *0.5f);

					draw_list.AddRectFilled(minpos, maxpos, IM_COL32(223, 223, 223, 255), 0.0f,  ImDrawCornerFlags_All);
					draw_list.AddRect(ImVec2(minpos.x - 1, minpos.y - 1), ImVec2(maxpos.x + 1, maxpos.y + 1), IM_COL32(0, 0, 0, 255), 0.0f,  ImDrawCornerFlags_All, 2.0f);
				}
				{
					float t_bar_half_width = 4;
					float t_bar_half_height = 10;

					auto minpos = ImVec2(move_bar_padding - t_bar_half_width + move_bar_width * camera_speed_visual_t, sceneData.resolution[1] - move_bar_height - 32 - t_bar_half_height);
					auto maxpos = ImVec2(move_bar_padding + t_bar_half_width + move_bar_width * camera_speed_visual_t, sceneData.resolution[1] - 32 + t_bar_half_height);

					draw_list.AddRectFilled(minpos, maxpos, IM_COL32(223, 223, 223, 255), 0.0f,  ImDrawCornerFlags_All);
					draw_list.AddRect(ImVec2(minpos.x - 1, minpos.y - 1), ImVec2(maxpos.x + 1, maxpos.y + 1), IM_COL32(32, 32, 32, 255), 0.0f,  ImDrawCornerFlags_All, 2.0f);
				}
			}
		}

		if(!Input::mouse_active)
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

		ImGui::Text("");
		ImGui::Text("Framerate Limit");	
		ImGui::InputInt("## Framerate Limit Value Input Int", &settings.fps_limit_value, 0, 0);
		ImGui::Checkbox("Limit framerate?", &settings.fps_limit_enabled);
		
		// Has to be in order!
		static std::pair<TonemappingType, std::string> tonemapping_text[] =
		{
			{TonemappingType::None, "None"},
			{TonemappingType::ApproximateACES, "Approximate ACES"},
			{TonemappingType::Reinhard, "Reinhard"},
		};


		if(ImGui::BeginCombo("Tonemapping", tonemapping_text[(int)settings.active_tonemapping].second.c_str()))
		{
			for(auto& tonemap : tonemapping_text)
			{
				if(ImGui::Selectable(tonemap.second.c_str(), settings.active_tonemapping == tonemap.first))
				{
					settings.active_tonemapping = tonemap.first;
				}
			}
			ImGui::EndCombo();
		}

		ImGui::End();
	}

	int Raytracer::get_target_fps()
	{
		return settings.fps_limit_enabled ? 
			settings.fps_limit_value :
			1000;
	}
}