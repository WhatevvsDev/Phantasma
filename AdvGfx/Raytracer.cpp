#include "Raytracer.h"

#include "Math.h"
#include "Common.h"
#include "Compute.h"
#include "BVH.h"
#include "Mesh.h"
#include "LogUtility.h"
#include "IOUtility.h"

#include <fstream>
#include <ostream>
#include <format>
#include <filesystem>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#include <GLFW/glfw3.h>
#include <ImGuizmo.h>

enum class TonemappingType
{
	None,
	ApproximateACES,
	Reinhard
};

namespace Raytracer
{	
	struct
	{
		bool recompile_changed_shaders_automatically { true };
		bool fps_limit_enabled { true };
		int fps_limit_value { 80 }; // TODO: this is sometimes very inaccurate in practice?

		float camera_speed_t {0.5f};
		float camera_rotation_smoothing = { 0.1f };
		float camera_position_smoothing = { 0.1f };

		bool orbit_camera_enabled { false };
		glm::vec3 orbit_camera_position { 0.0f };
		float orbit_camera_distance { 10.0f };
		float orbit_camera_height { 5.0f };
		float orbit_camera_rotations_per_second { 0.1f };

		TonemappingType active_tonemapping { TonemappingType::None };
	} settings;

	struct SceneData
	{
		uint resolution[2]		{ 0, 0 };
		uint mouse_pos[2] {};
		glm::vec3 cam_pos		{ 0.0f, 10.0f, 0.0f };
		uint tri_count			{ 0 };
		glm::vec3 cam_forward	{ 0.0f };
		float pad_1				{ 0.0f };
		glm::vec3 cam_right		{ 0.0f };
		float pad_2				{ 0.0f };
		glm::vec3 cam_up		{ 0.0f };
		float pad_3				{ 0.0f };
		uint dummy_1;
		uint dummy_2;
		//glm::mat4 object_tran;
	} sceneData;

	struct
	{
		uint32_t  mouse_click_tri = 0; // TODO: Purely for testing ImGuizmo, backing code should be changed later
		uint32_t* buffer { nullptr };
		float show_move_speed_bar_time { 0.0f };
		bool show_debug_ui { false };
		bool screenshot { false };

		bool world_dirty { false };
		int mouse_over_idx;
	} internal;

	namespace Input
	{
		// Temporary scuffed input
		glm::vec3 cam_rotation;

		void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			// Unused parameters
			(void)window;
			(void)scancode;
			(void)action;
			(void)mods;

			bool key_is_pressed = (action == GLFW_PRESS);

			if(!key_is_pressed)
				return;

			switch(key)
			{
				case GLFW_KEY_P:
					internal.screenshot = true;
				break;
				case GLFW_KEY_ESCAPE:
					Raytracer::terminate();
					exit(0);
				break;
				case GLFW_KEY_F1:
					internal.show_debug_ui = !internal.show_debug_ui;
					glfwSetInputMode(window, GLFW_CURSOR, internal.show_debug_ui ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
				break;
			}
		}	
	}

	float camera_speed_t_to_m_per_second()
	{
		float adjusted_t = settings.camera_speed_t * 2.0f - 1.0f;
		float value = pow(adjusted_t * 9.95f, 2.0f) * sgn(adjusted_t);

		if(settings.camera_speed_t > 0.5f)
		{
			return glm::clamp(value + 1.0f, 0.01f, 100.0f);
		}
		else
		{
			return glm::clamp(1.0f / fabsf(value - 1.0f), 0.01f, 100.0f);
		}
	}
	#pragma warning(disable:4996)

	// TODO: Temporary variables, will be consolidated into one system later
	ComputeReadBuffer* screen_compute_buffer;
	ComputeWriteBuffer* tris_compute_buffer;
	ComputeWriteBuffer* bvh_compute_buffer;
	ComputeWriteBuffer* tri_idx_compute_buffer;

	Mesh* loaded_model { nullptr };
	glm::mat4 object_matrix;

	void init(uint32_t* screen_buffer_ptr)
	{
		internal.buffer = screen_buffer_ptr;

		// TODO: Temporary, will probably be replaced with asset browser?
		loaded_model = new Mesh(get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\only_sphere.gltf");

		// Load settings
		std::ifstream f("phantasma.settings.json");
		if(f.good())
		{
			json settings_data = json::parse(f)["settings"];

			TryFromJSONVal(settings_data, settings, active_tonemapping);
			TryFromJSONVal(settings_data, settings, camera_speed_t);
			TryFromJSONVal(settings_data, settings, fps_limit_enabled);
			TryFromJSONVal(settings_data, settings, fps_limit_value);
			TryFromJSONVal(settings_data, settings, recompile_changed_shaders_automatically);
			TryFromJSONVal(settings_data, settings, camera_position_smoothing);
			TryFromJSONVal(settings_data, settings, camera_rotation_smoothing);

			TryFromJSONVal(settings_data, settings, orbit_camera_enabled);
			TryFromJSONVal(settings_data, settings, orbit_camera_position);
			TryFromJSONVal(settings_data, settings, orbit_camera_distance);
			TryFromJSONVal(settings_data, settings, orbit_camera_height);
			TryFromJSONVal(settings_data, settings, orbit_camera_rotations_per_second);

			TryFromJSONVal(settings_data, sceneData, cam_pos);
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

		// TODO: temporary, will be consolidated into one system later
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

		settings_data["settings"]["orbit_camera_enabled"] = settings.orbit_camera_enabled;
		settings_data["settings"]["orbit_camera_position"] = settings.orbit_camera_position;
		settings_data["settings"]["orbit_camera_distance"] = settings.orbit_camera_distance;
		settings_data["settings"]["orbit_camera_height"] = settings.orbit_camera_height;
		settings_data["settings"]["orbit_camera_rotations_per_second"] = settings.orbit_camera_rotations_per_second;
		
		settings_data["sceneData"]["cam_pos"] = sceneData.cam_pos;

		std::ofstream o("phantasma.settings.json");
		o << settings_data << std::endl;
	}

	void update(const float delta_time_ms)
	{
		internal.show_move_speed_bar_time -= (delta_time_ms / 1000.0f);
		int moveHor =	(ImGui::IsKeyDown(ImGuiKey_D))		- (ImGui::IsKeyDown(ImGuiKey_A));
		int moveVer =	(ImGui::IsKeyDown(ImGuiKey_Space))	- (ImGui::IsKeyDown(ImGuiKey_LeftCtrl));
		int moveWard =	(ImGui::IsKeyDown(ImGuiKey_W))		- (ImGui::IsKeyDown(ImGuiKey_S));
		
		if(settings.orbit_camera_enabled)
		{
			static float orbit_cam_t = 0.0f;
			orbit_cam_t += (delta_time_ms / 1000.0f) * settings.orbit_camera_rotations_per_second;

			glm::vec3 offset = glm::vec3(0.0f, settings.orbit_camera_height, settings.orbit_camera_distance);
			glm::mat3 rotation = glm::eulerAngleY(glm::radians(orbit_cam_t * 360.0f));

			offset = offset * rotation; // Rotate it
			offset += settings.orbit_camera_position; // Position it

			glm::vec3 to_center = glm::normalize(settings.orbit_camera_position - offset);

			sceneData.cam_forward = to_center;
			sceneData.cam_right = glm::cross(sceneData.cam_forward, glm::vec3(0.0f, 1.0f, 0.0f));
			sceneData.cam_up = glm::cross(sceneData.cam_right, sceneData.cam_forward);
			sceneData.cam_pos = offset;
		}
		else
		{
			glm::mat3 rotation = glm::eulerAngleXYZ(glm::radians(Input::cam_rotation.x), glm::radians(Input::cam_rotation.y), glm::radians(Input::cam_rotation.z));

			sceneData.cam_forward = glm::vec3(0, 0, 1.0f) * rotation;
			sceneData.cam_right = glm::vec3(-1.0f, 0, 0) * rotation;
			sceneData.cam_up = glm::vec3(0, 1.0f, 0) * rotation;

			glm::vec3 dir = 
				sceneData.cam_forward * moveWard + 
				sceneData.cam_up * moveVer + 
				sceneData.cam_right * moveHor;

			glm::normalize(dir);

			static glm::vec3 target_cam_pos;
			float position_t = (settings.camera_position_smoothing != 0 ? (1.0f - settings.camera_position_smoothing * 0.75f) * (delta_time_ms / 200.0f) : 1.0f);
			target_cam_pos += glm::vec3(dir * delta_time_ms * 0.01f) * camera_speed_t_to_m_per_second();
			sceneData.cam_pos = glm::lerp(sceneData.cam_pos, target_cam_pos, position_t);

			bool allow_camera_rotation = !internal.show_debug_ui;

			if(allow_camera_rotation)
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
		}

		if(settings.recompile_changed_shaders_automatically)
			Compute::recompile_kernels(ComputeKernelRecompilationCondition::SourceChanged);

		float old_camera_speed_t = settings.camera_speed_t;
		settings.camera_speed_t += ImGui::GetIO().MouseWheel * 0.01f;
		settings.camera_speed_t = glm::fclamp(settings.camera_speed_t, 0.0f, 1.0f);
		if(settings.camera_speed_t != old_camera_speed_t)
			internal.show_move_speed_bar_time = 2.0f;

		if(internal.world_dirty)
		{
			loaded_model->reconstruct_bvh();

			tris_compute_buffer->update(loaded_model->tris);
			bvh_compute_buffer->update(loaded_model->bvh->bvhNodes);
			tri_idx_compute_buffer->update(loaded_model->bvh->triIdx);

			internal.world_dirty = false;
		}
	}

	void raytrace(int width, int height)
	{
		sceneData.resolution[0] = width;
		sceneData.resolution[1] = height;
		sceneData.tri_count = loaded_model->tris.size();

		ComputeReadWriteBuffer screen_buffer({internal.buffer, (size_t)(width * height)});

		ComputeOperation("raytrace.cl")
			.read(ComputeReadBuffer({internal.buffer, (size_t)(width * height)}))
			.read(ComputeReadBuffer({&internal.mouse_over_idx, 1}))
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

		if(internal.show_debug_ui)
		{
			bool clicked_on_non_gizmo = (ImGui::GetIO().MouseReleased[0] && !ImGuizmo::IsOver());

			auto cursor_pos = ImGui::GetIO().MousePos;

			sceneData.mouse_pos[0] = glm::clamp((int)cursor_pos.x, 0, width);
			sceneData.mouse_pos[1] = glm::clamp((int)cursor_pos.y, 0, height);

			if(clicked_on_non_gizmo)
			{
				internal.mouse_click_tri = internal.mouse_over_idx;
			}
		}

		if(internal.screenshot)
		{
			stbi_flip_vertically_on_write(true);
			stbi_write_jpg("render.jpg", width, height, 4, internal.buffer, width * 4 );
			stbi_flip_vertically_on_write(false);
			LOGMSG(Log::MessageType::Debug, "Saved screenshot.");
			internal.screenshot = false;
		}
    }

	void ui()
	{
		// Bless this mess
		auto& draw_list = *ImGui::GetForegroundDrawList();

		if(internal.show_move_speed_bar_time > 0 || internal.show_debug_ui)
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

		if(!internal.show_debug_ui)
			return;

		auto view = glm::lookAtRH(glm::vec3(sceneData.cam_pos), glm::vec3(sceneData.cam_pos) + glm::vec3(sceneData.cam_forward), glm::vec3(0.0f, 1.0f, 0.0f));

		glm::mat4 projection = glm::perspectiveRH(glm::radians(90.0f), 1200.0f / 800.0f, 0.1f, 1000.0f);

		if(internal.mouse_click_tri != -1)
		{
			Tri& ref = loaded_model->get_tri_ref(internal.mouse_click_tri);
			glm::vec3 tri_pos = (ref.vertex0 + ref.vertex1 + ref.vertex2) * 0.3333f;

			object_matrix = glm::mat4(1.0f);
			object_matrix = glm::translate(tri_pos);

			if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), ImGuizmo::TRANSLATE, ImGuizmo::WORLD, glm::value_ptr(object_matrix)))
			{
				internal.world_dirty = true;
			}

			float matrixTranslation[3], matrixRotation[3], matrixScale[3];
			ImGuizmo::DecomposeMatrixToComponents((float*)&object_matrix, matrixTranslation, matrixRotation, matrixScale);
			glm::vec3 move = glm::vec3(matrixTranslation[0], matrixTranslation[1], matrixTranslation[2]) - tri_pos;

			ref.vertex0 += move;
			ref.vertex1 += move;
			ref.vertex2 += move;
		}

		auto latest_msg = Log::get_latest_msg();
		auto message_color = (latest_msg.second == Log::MessageType::Error) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 255, 255);

		ImGui::GetForegroundDrawList()->AddText(ImVec2(10, 10), message_color,latest_msg.first.data() ,latest_msg.first.data() + latest_msg.first.length());

		// Debug settings window
		// TODO: Remake this as not just a standard debug window, but something more user friendly

		ImGui::Begin(" Debug settings window");
		
		ImGui::Checkbox("Orbit camera enabled?", &settings.orbit_camera_enabled);
		if(!settings.orbit_camera_enabled)
			ImGui::BeginDisabled();

		ImGui::DragFloat3("Orbit position", glm::value_ptr(settings.orbit_camera_position));
		ImGui::DragFloat("Orbit distance", &settings.orbit_camera_distance, 0.1f);
		ImGui::DragFloat("Orbit height", &settings.orbit_camera_height, 0.1f);
		ImGui::DragFloat("Orbit rotations per second", &settings.orbit_camera_rotations_per_second, 0.01f, -1.0f, 1.0f);

		if(!settings.orbit_camera_enabled)
			ImGui::EndDisabled();

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