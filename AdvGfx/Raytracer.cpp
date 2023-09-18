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

#include <ImGuizmo.h>
#include "Math.h"

#include <format>
#include <filesystem>

#include "BVH.h"
#include "Mesh.h"

enum class TonemappingType
{
	None,
	ApproximateACES,
	//Filmic,
	Reinhard
};

// TODO: Swap triangle to bouding box centroid, instead of vertex centroid :)


namespace Raytracer
{	
	struct
	{
		bool recompile_changed_shaders_automatically { true };
		bool fps_limit_enabled { true };
		int fps_limit_value { 80 };

		float camera_speed_t {0.5f};
		float camera_rotation_smoothing = { 0.1f };
		float camera_position_smoothing = { 0.1f };

		TonemappingType active_tonemapping { TonemappingType::None };
	} settings;

	namespace Input
	{
		// Temporary scuffed input
		float mouse_x = 0.0f;
		float mouse_y = 0.0f;
		glm::vec3 cam_rotation;
		bool mouse_active = false;
		bool screenshot = false;
		float show_move_speed_timer = 0;

		void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			// Unused parameters
			(void)window;
			(void)scancode;
			(void)action;
			(void)mods;

			bool is_pressed = (action == GLFW_PRESS);

			if(is_pressed)
			{
				switch(key)
				{
					case GLFW_KEY_P:
						screenshot = true;
					break;
					case GLFW_KEY_ESCAPE:
						Raytracer::terminate();
						exit(0);
					break;
					case GLFW_KEY_F1:
						mouse_active = !mouse_active;
					break;
				}
			}

			if(mouse_active)
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			else
				glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}	
	}

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

	ComputeReadBuffer* screen_compute_buffer;
	ComputeWriteBuffer* tris_compute_buffer;
	ComputeWriteBuffer* bvh_compute_buffer;
	ComputeWriteBuffer* tri_idx_compute_buffer;

	Mesh* loaded_model;
	glm::mat4 object_matrix;
	bool world_dirty { false };

	#ifndef TryFromJSONValPtr
	// For values that might not exist (text attribute, but NOT for stuff like node_id)
	#define TryFromJSONVal(lookIn, saveTo, varName) \
	if(lookIn.find(#varName) != lookIn.end()) \
	{\
		lookIn.at(#varName).get_to(saveTo.varName);\
	} \
	else \
	{\
		LOGMSG(Log::MessageType::Debug,("Couldn't load json attribute " #varName));\
	}
	#endif

	void init()
	{
		loaded_model = new Mesh(get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\simple_test.gltf");

		// Load raytracer settings
		std::ifstream f("phantasma.settings.json");
		if(f.good())
		{
			json settings_data = json::parse(f)["settings"];
			//if(settings_data.find("active_tonemapping"))

			TryFromJSONVal(settings_data, settings, active_tonemapping);
			TryFromJSONVal(settings_data, settings, camera_speed_t);
			TryFromJSONVal(settings_data, settings, fps_limit_enabled);
			TryFromJSONVal(settings_data, settings, fps_limit_value);
			TryFromJSONVal(settings_data, settings, recompile_changed_shaders_automatically);
			TryFromJSONVal(settings_data, settings, camera_position_smoothing);
			TryFromJSONVal(settings_data, settings, camera_rotation_smoothing);
		}

		// Search for, and automatically compile compute shaders
		std::string compute_directory = get_current_directory_path() + "\\..\\..\\AdvGfx\\compute\\";
		for (const auto & possible_compute_shader : std::filesystem::directory_iterator(compute_directory))
		{
			std::string file_path = possible_compute_shader.path().string();
			std::string file_name_with_extension = file_path.substr(file_path.find_last_of("/\\") + 1);
			std::string file_extension = file_name_with_extension.substr(file_name_with_extension.find_last_of(".") + 1);
			std::string file_name = file_name_with_extension.substr(0, file_name_with_extension.length() - file_extension.length() - 1);

			bool wrong_file_extension = (file_extension != "cl");
			bool already_exists = Compute::kernel_exists(file_name);

			if(wrong_file_extension || already_exists)
				continue;
			
			Compute::create_kernel(file_path, file_name);
		}

		//screen_compute_buffer = {buffer, (size_t)(sceneData.resolution[0] * sceneData.resolution[1])};
		tris_compute_buffer		= new ComputeWriteBuffer({loaded_model->tris});
		bvh_compute_buffer		= new ComputeWriteBuffer({loaded_model->bvh->bvhNodes});
		tri_idx_compute_buffer	= new ComputeWriteBuffer({loaded_model->bvh->triIdx});
	}

	void terminate()
	{
		json settings_data;

		settings_data["settings"]["active_tonemapping"] = settings.active_tonemapping;
		settings_data["settings"]["camera_speed_t"] = settings.camera_speed_t;
		settings_data["settings"]["fps_limit_enabled"] = settings.fps_limit_enabled;
		settings_data["settings"]["fps_limit_value"] = settings.fps_limit_value;
		settings_data["settings"]["recompile_changed_shaders_automatically"] = settings.recompile_changed_shaders_automatically;
		settings_data["settings"]["camera_position_smoothing"] = settings.camera_position_smoothing;
		settings_data["settings"]["camera_rotation_smoothing"] = settings.camera_rotation_smoothing;

		std::ofstream o("phantasma.settings.json");
		o << settings_data << std::endl;
	}

	void update(const float delta_time_ms)
	{
		Input::show_move_speed_timer -= (delta_time_ms / 1000.0f);
		int moveHor =	(ImGui::IsKeyDown(ImGuiKey_D))		- (ImGui::IsKeyDown(ImGuiKey_A));
		int moveVer =	(ImGui::IsKeyDown(ImGuiKey_Space))	- (ImGui::IsKeyDown(ImGuiKey_LeftCtrl));
		int moveWard =	(ImGui::IsKeyDown(ImGuiKey_W))		- (ImGui::IsKeyDown(ImGuiKey_S));
		
		glm::mat4 rotation = glm::eulerAngleXYZ(glm::radians(Input::cam_rotation.x), glm::radians(Input::cam_rotation.y), glm::radians(Input::cam_rotation.z));

		sceneData.cam_forward = glm::vec4(0, 0, 1.0f, 0) * rotation;
		sceneData.cam_right = glm::vec4(-1.0f, 0, 0, 0) * rotation;
		sceneData.cam_up = glm::vec4(0, 1.0f, 0, 0) * rotation;

		glm::vec3 dir = 
			sceneData.cam_forward * moveWard + 
			sceneData.cam_up * moveVer + 
			sceneData.cam_right * moveHor;

		glm::normalize(dir);

		static glm::vec4 target_cam_pos;
		float position_t = (settings.camera_position_smoothing != 0 ? (1.0f - settings.camera_position_smoothing * 0.75f) * (delta_time_ms / 200.0f) : 1.0f);
		target_cam_pos += glm::vec4(dir * delta_time_ms * 0.01f, 0.0f) * camera_speed_t_to_m_per_second();
		sceneData.cam_pos = glm::lerp(sceneData.cam_pos, target_cam_pos, position_t);

		if(!Input::mouse_active)
		{
			auto mouse_delta = ImGui::GetIO().MouseDelta;

			static glm::vec3 target_rotation;
			float rotation_t = (settings.camera_rotation_smoothing != 0 ? (1.0f - settings.camera_rotation_smoothing * 0.75f) * (delta_time_ms / 50.0f) : 1.0f);
			target_rotation += glm::vec3(-mouse_delta.y, mouse_delta.x, 0) * 0.1f;
			Input::cam_rotation = glm::lerp(Input::cam_rotation, target_rotation, rotation_t);

			// limit pitch
			if(fabs(Input::cam_rotation.x) > 89.9f)
				Input::cam_rotation.x = 89.9f * sgn(Input::cam_rotation.x);
		}

		if(settings.recompile_changed_shaders_automatically)
			Compute::recompile_kernels(ComputeKernelRecompilationCondition::SourceChanged);

		float old_camera_speed_t = settings.camera_speed_t;
		settings.camera_speed_t += ImGui::GetIO().MouseWheel * 0.01f;
		settings.camera_speed_t = glm::fclamp(settings.camera_speed_t, 0.0f, 1.0f);
		if(settings.camera_speed_t != old_camera_speed_t)
			Input::show_move_speed_timer = 2.0f;

		if(world_dirty)
		{
			loaded_model->reconstruct_bvh();

			tris_compute_buffer->update(loaded_model->tris);
			bvh_compute_buffer->update(loaded_model->bvh->bvhNodes);
			tri_idx_compute_buffer->update(loaded_model->bvh->triIdx);

			world_dirty = false;
		}
	}

	unsigned int mouse_click_tri = 0;

	void raytrace(int width, int height, uint32_t* buffer)
	{
		sceneData.resolution[0] = width;
		sceneData.resolution[1] = height;
		sceneData.tri_count = loaded_model->tris.size();

		ComputeReadWriteBuffer screen_buffer({buffer, (size_t)(width * height)});

		ComputeOperation("raytrace.cl")
			.read(ComputeReadBuffer({buffer, (size_t)(width * height)}))
			.write(*tris_compute_buffer)
			.write(*bvh_compute_buffer)
			.write(*tri_idx_compute_buffer)
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

			op->read_write(screen_buffer)
				.write({&sceneData, 1})
				.global_dispatch({width, height, 1})
				.execute();
			delete op;
		}

		if(Input::mouse_active)
		{
			if(ImGui::GetIO().MouseDown[0] && !ImGuizmo::IsOver())
			{
				auto cursor_pos = ImGui::GetIO().MousePos;
				cursor_pos.y = height - cursor_pos.y; // Account for flipped coords;
				cursor_pos.x = glm::clamp((int)cursor_pos.x, 0, width);
				cursor_pos.y = glm::clamp((int)cursor_pos.y, 0, height);

				unsigned int a = (buffer[(int)cursor_pos.x + (int)cursor_pos.y * width] & 0xff000000) >> 24;

				mouse_click_tri = a;
			}
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

		auto view = glm::lookAtRH(glm::vec3(sceneData.cam_pos), glm::vec3(sceneData.cam_pos) + glm::vec3(sceneData.cam_forward), glm::vec3(0.0f, 1.0f, 0.0f));

		glm::mat4 projection = glm::perspectiveRH(glm::radians(90.0f), 1200.0f / 800.0f, 0.1f, 1000.0f);

		Tri& ref = loaded_model->get_tri_ref(mouse_click_tri);
		glm::vec3 tri_pos = (ref.vertex0 + ref.vertex1 + ref.vertex2) * 0.3333f;

		object_matrix = glm::mat4(1.0f);
		object_matrix = glm::translate(tri_pos);

		if (ImGuizmo::Manipulate((float*)&view, (float*)&projection, ImGuizmo::TRANSLATE, ImGuizmo::WORLD, (float*)&object_matrix))
		{
			world_dirty = true;
		}

		float matrixTranslation[3], matrixRotation[3], matrixScale[3];
		ImGuizmo::DecomposeMatrixToComponents((float*)&object_matrix, matrixTranslation, matrixRotation, matrixScale);
		glm::vec3 move = glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]) - tri_pos;

		ref.vertex0 += move;
		ref.vertex1 += move;
		ref.vertex2 += move;

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

		ImGui::DragFloat("Camera rotation smoothing", &settings.camera_rotation_smoothing, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Camera position smoothing", &settings.camera_position_smoothing, 0.01f, 0.0f, 1.0f);


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