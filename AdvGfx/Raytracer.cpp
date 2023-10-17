#include "Raytracer.h"

#include "Math.h"
#include "Common.h"
#include "Compute.h"
#include "BVH.h"
#include "Mesh.h"
#include "LogUtility.h"
#include "IOUtility.h"
#include "Utilities.h"
#include "Timer.h"
#include "AssetManager.h"


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
#include <ImPlot.h>
#include <IconsFontAwesome6.h>

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

namespace Raytracer
{	
	struct DiskAsset
	{
		std::filesystem::path path;
		std::string filename;
	};

	struct
	{
		bool recompile_changed_shaders_automatically { true };
		bool fps_limit_enabled { true };
		int fps_limit_value { 80 }; // TODO: this is sometimes very inaccurate in practice?

		float camera_speed_t {0.5f};

		bool orbit_camera_enabled { false };
		glm::vec3 orbit_camera_position { 0.0f };
		float orbit_camera_distance { 10.0f };
		float orbit_camera_angle { 0.0f };
		float orbit_camera_rotations_per_second { 0.1f };
		bool orbit_automatically { true };

		glm::vec3 saved_camera_position { 0.0f };

		bool show_onscreen_log { false };

		bool accumulate_frames { true };
		bool limit_accumulated_frames { true };
		int accumulated_frame_limit { 32 };
	} settings;

	struct SceneData
	{
		uint resolution[2] { 0, 0 };
		uint mouse_pos[2] {};
		glm::vec3 cam_pos { 0.0f, 10.0f, 0.0f };
		int frame_number { 0 };
		glm::vec3 cam_forward { 0.0f };
		float pad_1 { 0.0f };
		glm::vec3 cam_right { 0.0f };
		float pad_2 { 0.0f };
		glm::vec3 cam_up { 0.0f };
		float pad_3 { 0.0f };
		glm::mat4 object_inverse_transform { glm::mat4(1) };
		bool reset_accumulator { false };
		uint mesh_idx { 0 };
		uint exr_width;
		uint exr_height;
	} sceneData;

	struct
	{
		uint32_t  mouse_click_tri = 0; // TODO: Purely for testing ImGuizmo, backing code should be changed later
		uint32_t* buffer { nullptr };
		float show_move_speed_bar_time { 0.0f };
		bool show_debug_ui { false };
		bool screenshot { false };

		bool world_dirty { false };
		bool camera_dirty { true };
		int mouse_over_idx { -1 };

		int accumulated_samples { 0 };
		int render_width { 0 };
		int render_height { 0 };
		const int render_channel_count { 4 };
		
		ImGuizmo::OPERATION current_gizmo_operation { ImGuizmo::TRANSLATE };
		ComputeGPUOnlyBuffer* gpu_accumulation_buffer { nullptr };

		std::vector<DiskAsset> exr_assets_on_disk;
		float* loaded_exr_data { nullptr };
		std::string current_exr { "None" };

		struct
		{
			Timer timer;
			static const int data_samples { 256 };
			float update_times_ms[data_samples] {};
			float render_times_ms[data_samples] {};
			float max_time = 0.0f;
		} performance;
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

	// TODO: Temporary variables, will be consolidated into one system later
	ComputeReadBuffer* screen_compute_buffer	{ nullptr };

	ComputeWriteBuffer* exr_buffer	{ nullptr };

	// Resizes buffers and sets internal state
	void resize(const RaytracerResizeDesc& desc)
	{
		bool replace_buffer = desc.new_buffer_ptr != nullptr;

		if(replace_buffer)
		{
			internal.buffer = desc.new_buffer_ptr;
		}

		internal.render_width = desc.width_px;
		internal.render_height = desc.height_px;
	}

	// Creates the necessary buffers and sets internal state
	void init_internal(const RaytracerInitDesc& desc)
	{
		desc.validate();

		RaytracerResizeDesc resize_desc;
		resize_desc.width_px = desc.width_px;
		resize_desc.height_px = desc.height_px;
		resize_desc.new_buffer_ptr = desc.screen_buffer_ptr;

		resize(resize_desc);
	}

	// Deserialize data from phantasma.data.json
	void init_load_saved_data()
	{
		// Load settings
		std::ifstream f("phantasma.data.json");

		bool file_opened_successfully = f.good();

		if(file_opened_successfully)
		{
			json save_data = json::parse(f);

			TryFromJSONVal(save_data, settings, camera_speed_t);
			TryFromJSONVal(save_data, settings, fps_limit_enabled);
			TryFromJSONVal(save_data, settings, fps_limit_value);
			TryFromJSONVal(save_data, settings, recompile_changed_shaders_automatically);

			TryFromJSONVal(save_data, settings, orbit_camera_enabled);
			TryFromJSONVal(save_data, settings, orbit_camera_position);
			TryFromJSONVal(save_data, settings, orbit_camera_distance);
			TryFromJSONVal(save_data, settings, orbit_camera_angle);
			TryFromJSONVal(save_data, settings, orbit_camera_rotations_per_second);

			TryFromJSONVal(save_data, settings, accumulate_frames);
			TryFromJSONVal(save_data, settings, limit_accumulated_frames);
			TryFromJSONVal(save_data, settings, accumulated_frame_limit);

			TryFromJSONVal(save_data, sceneData, cam_pos);
		}
	}

	// Search for, and automatically compile compute shaders
	void init_find_assets()
	{
		std::string compute_directory = get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\";
		for (const auto & asset_path : std::filesystem::recursive_directory_iterator(compute_directory))
		{
			std::string file_path = asset_path.path().string();
			std::string file_name_with_extension = file_path.substr(file_path.find_last_of("/\\") + 1);
			std::string file_extension = file_name_with_extension.substr(file_name_with_extension.find_last_of(".") + 1);
			std::string file_name = file_name_with_extension.substr(0, file_name_with_extension.length() - file_extension.length() - 1);

			bool wrong_file_extension = (file_extension != "cl") && (file_extension != "exr");
			bool already_exists = (file_extension == "cl") && Compute::kernel_exists(file_name);
			bool file_is_common_source = (file_name == "common");

			if(wrong_file_extension || already_exists || file_is_common_source)
				continue;

			if(file_extension == "cl")
			{
				Compute::create_kernel(file_path, file_name);
				continue;
			}

			if(file_extension == "exr")
			{
				internal.exr_assets_on_disk.push_back({asset_path, file_name});
				printf("Found exr file: %s\n", file_name.c_str());
				continue;
			}
		}
	}

	void load_exr(uint index = 0)
	{
		std::string exr_path = internal.exr_assets_on_disk[index].path.string();
		std::string file_name_with_extension = exr_path.substr(exr_path.find_last_of("/\\") + 1);

		internal.current_exr = file_name_with_extension;

		int width;
		int height;
		const char* err = NULL; // or nullptr in C++11

		delete[] internal.loaded_exr_data;
		int ret = LoadEXR(&internal.loaded_exr_data, &width, &height, exr_path.c_str(), &err);

		sceneData.exr_width = (uint)width;
		sceneData.exr_height = (uint)height;

		exr_buffer = new ComputeWriteBuffer({internal.loaded_exr_data, (size_t)(width * height * 4)});
		internal.camera_dirty = true;
	}

	void init(const RaytracerInitDesc& desc)
	{
		init_internal(desc);
		init_load_saved_data();
		init_find_assets();
		load_exr();

		internal.gpu_accumulation_buffer = new ComputeGPUOnlyBuffer((size_t)(internal.render_width * internal.render_height * internal.render_channel_count * sizeof(float)));

		internal.performance.timer.start();

		// TODO: make this "automatic", keep track of things in asset folder
		// make them loadable to CPU, and then also optionally GPU using a free list or sm
		AssetManager::init();
		//AssetManager::load_mesh(get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\stanfordbunny.gltf");
		AssetManager::load_mesh(get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\flat_vs_smoothed.gltf");
		//AssetManager::load_mesh(get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\mid_poly_sphere.gltf");
	}

	// Saves data such as settings, or world state to phantasma.data.json
	void terminate_save_data()
	{
		json save_data;

		ToJSONVal(save_data, settings, camera_speed_t);
		ToJSONVal(save_data, settings, fps_limit_enabled);
		ToJSONVal(save_data, settings, fps_limit_value);
		ToJSONVal(save_data, settings, recompile_changed_shaders_automatically);

		ToJSONVal(save_data, settings, orbit_camera_enabled);
		ToJSONVal(save_data, settings, orbit_camera_position);
		ToJSONVal(save_data, settings, orbit_camera_distance);
		ToJSONVal(save_data, settings, orbit_camera_angle);
		ToJSONVal(save_data, settings, orbit_camera_rotations_per_second);

		ToJSONVal(save_data, settings, accumulate_frames);
		ToJSONVal(save_data, settings, limit_accumulated_frames);
		ToJSONVal(save_data, settings, accumulated_frame_limit);

		ToJSONVal(save_data, sceneData, cam_pos);

		std::ofstream o("phantasma.data.json");
		o << save_data << std::endl;
	}

	void terminate()
	{
		terminate_save_data();
	}

	void update_orbit_camera_behavior(float delta_time_ms)
	{
		static float orbit_cam_t = 0.0f;

		if(settings.orbit_automatically)
		{
			orbit_cam_t += (delta_time_ms / 1000.0f) * -settings.orbit_camera_rotations_per_second;

			bool camera_is_orbiting = (settings.orbit_camera_rotations_per_second != 0.0f);

			if(camera_is_orbiting)
				internal.camera_dirty = true;
		}

		glm::vec3 offset = glm::vec3(0.0f, 0.0f, 1.0f);
		glm::mat3 rotation = glm::eulerAngleXY(glm::radians(settings.orbit_camera_angle), glm::radians(orbit_cam_t * 360.0f));

		offset = offset * rotation; // Rotate it

		sceneData.cam_forward = -glm::normalize(offset);
		sceneData.cam_right = glm::normalize(glm::cross(sceneData.cam_forward, glm::vec3(0.0f, 1.0f, 0.0f)));
		sceneData.cam_up = glm::normalize(glm::cross(sceneData.cam_right, sceneData.cam_forward));
		sceneData.cam_pos = offset * settings.orbit_camera_distance + settings.orbit_camera_position;
	}

	void update_free_float_camera_behavior(float delta_time_ms)
	{
		int moveHor =	(ImGui::IsKeyDown(ImGuiKey_D))		- (ImGui::IsKeyDown(ImGuiKey_A));
		int moveVer =	(ImGui::IsKeyDown(ImGuiKey_Space))	- (ImGui::IsKeyDown(ImGuiKey_LeftCtrl));
		int moveWard =	(ImGui::IsKeyDown(ImGuiKey_W))		- (ImGui::IsKeyDown(ImGuiKey_S));

		glm::mat3 rotation = glm::eulerAngleXYZ(glm::radians(Input::cam_rotation.x), glm::radians(Input::cam_rotation.y), glm::radians(Input::cam_rotation.z));

		sceneData.cam_forward = glm::vec3(0, 0, 1.0f) * rotation;
		sceneData.cam_right = glm::vec3(-1.0f, 0, 0) * rotation;
		sceneData.cam_up = glm::vec3(0, 1.0f, 0) * rotation;

		glm::vec3 dir = 
			sceneData.cam_forward * moveWard + 
			sceneData.cam_up * moveVer + 
			sceneData.cam_right * moveHor;

		glm::normalize(dir);
		glm::vec3 camera_move_delta = glm::vec3(dir * delta_time_ms * 0.01f) * camera_speed_t_to_m_per_second();

		bool camera_is_moving = (glm::dot(camera_move_delta, camera_move_delta) != 0.0f);

		if(camera_is_moving)
		{
			internal.camera_dirty = true;
				
			sceneData.cam_pos += camera_move_delta;
		}

		bool allow_camera_rotation = !internal.show_debug_ui;

		if(allow_camera_rotation)
		{
			auto mouse_delta = ImGui::GetIO().MouseDelta;

			glm::vec3 camera_angular_delta = glm::vec3(-mouse_delta.y, mouse_delta.x, 0) * 0.1f;

			bool camera_is_rotating = (glm::dot(camera_angular_delta, camera_angular_delta) != 0.0f);

			if(camera_is_rotating)
			{
				Input::cam_rotation += camera_angular_delta;
				bool pitch_exceeds_limit = (fabs(Input::cam_rotation.x) > 89.9f);

				if(pitch_exceeds_limit)
					Input::cam_rotation.x = 89.9f * sgn(Input::cam_rotation.x);

				internal.camera_dirty = true;
			}
		}
	}

	void update(const float delta_time_ms)
	{
		internal.performance.timer.reset();

		internal.show_move_speed_bar_time -= (delta_time_ms / 1000.0f);
		
		settings.orbit_camera_enabled
			? update_orbit_camera_behavior(delta_time_ms)
			: update_free_float_camera_behavior(delta_time_ms);

		if(settings.recompile_changed_shaders_automatically)
		{
			bool recompiled_any_shaders = Compute::recompile_kernels(ComputeKernelRecompilationCondition::SourceChanged);
			 
			if(recompiled_any_shaders)
			{
				internal.camera_dirty = true;
			}
		}

		float old_camera_speed_t = settings.camera_speed_t;
		settings.camera_speed_t += ImGui::GetIO().MouseWheel * 0.01f;
		settings.camera_speed_t = glm::fclamp(settings.camera_speed_t, 0.0f, 1.0f);
		if(settings.camera_speed_t != old_camera_speed_t)
			internal.show_move_speed_bar_time = 2.0f;

		if(internal.world_dirty)
		{
			/*loaded_model->reconstruct_bvh();

			tris_compute_buffer->update(loaded_model->tris);
			bvh_compute_buffer->update(loaded_model->bvh->bvhNodes);
			tri_idx_compute_buffer->update(loaded_model->bvh->triIdx);*/

			internal.world_dirty = false;
		}

		rotate_array_right(internal.performance.update_times_ms, internal.performance.data_samples);
		internal.performance.update_times_ms[0] = internal.performance.timer.to_now();
		internal.performance.max_time = glm::max(internal.performance.max_time, internal.performance.update_times_ms[0]);
	}

	// Averages out acquired samples, and renders them to the screen
	void raytrace_average_samples(const ComputeReadWriteBuffer& screen_buffer, const ComputeGPUOnlyBuffer& accumulated_samples)
	{
		float samples_reciprocal = 1.0f / (float)internal.accumulated_samples;

		// TODO: pass this a buffer of heatmap colors, instead of hardcoding it into the shader
		ComputeOperation("average_accumulated.cl")
			.read_write(accumulated_samples)
			.read_write(screen_buffer)
			.write({&samples_reciprocal, 1})
			.write({&sceneData, 1})
			.global_dispatch({internal.render_width, internal.render_height, 1})
			.execute();
	}

	void raytrace()
	{
		internal.performance.timer.reset();

		sceneData.resolution[0] = internal.render_width;
		sceneData.resolution[1] = internal.render_height;
		sceneData.frame_number++;

		if(!settings.accumulate_frames)
			internal.camera_dirty = true;

		// TODO: we currently don't take into account world changes!
		if(internal.camera_dirty)
		{
			sceneData.reset_accumulator = true;
			internal.camera_dirty = false;
			internal.accumulated_samples = 0;
		}

		unsigned int render_area = internal.render_width * internal.render_height;
		ComputeReadWriteBuffer screen_buffer({internal.buffer, (size_t)(render_area)});
		
		bool accumulated_enough_frames = (internal.accumulated_samples > settings.accumulated_frame_limit);

		if(settings.limit_accumulated_frames && accumulated_enough_frames)
		{
			goto skip_rendering_goto;
		}

		internal.accumulated_samples++;

		AssetManager::get_mesh_header_buffer().update(AssetManager::get_mesh_headers());

		ComputeOperation("raytrace.cl")
			.read_write(*internal.gpu_accumulation_buffer)
			.read(ComputeReadBuffer({&internal.mouse_over_idx, 1}))
			.write(AssetManager::get_normals_compute_buffer())
			.write(AssetManager::get_tris_compute_buffer())
			.write(AssetManager::get_bvh_compute_buffer())
			.write(AssetManager::get_tri_idx_compute_buffer())
			.write(AssetManager::get_mesh_header_buffer())
			.write({&sceneData, 1})
			.write(*exr_buffer)
			.global_dispatch({internal.render_width, internal.render_height, 1})
			.execute();

		raytrace_average_samples(screen_buffer, (*internal.gpu_accumulation_buffer));

		sceneData.reset_accumulator = false;

		skip_rendering_goto:;

		// We query performance here to avoid screenshot/ui code
		rotate_array_right(internal.performance.render_times_ms, internal.performance.data_samples);
		internal.performance.render_times_ms[0] = internal.performance.timer.to_now();
		internal.performance.max_time = glm::max(internal.performance.max_time, internal.performance.render_times_ms[0]);

		if(internal.show_debug_ui)
		{
			bool clicked_on_non_gizmo = (ImGui::GetIO().MouseReleased[0] && !ImGuizmo::IsOver() && !ImGui::IsAnyItemHovered());

			auto cursor_pos = ImGui::GetIO().MousePos;

			sceneData.mouse_pos[0] = glm::clamp((int)cursor_pos.x, 0, internal.render_width - 1);
			sceneData.mouse_pos[1] = glm::clamp((int)cursor_pos.y, 0, internal.render_height - 1);

			if(clicked_on_non_gizmo)
			{
				internal.mouse_click_tri = internal.mouse_over_idx;
			}
		}

		if(internal.screenshot)
		{
			stbi_write_jpg("render.jpg", internal.render_width, internal.render_height, internal.render_channel_count, internal.buffer, 100);
			LOGDEBUG("Saved screenshot.");
			internal.screenshot = false;
		}
	}

	void ui()
	{
		if(!internal.show_debug_ui)
			return;

		ImGui::PushStyleColor(ImGuiCol_WindowBg, {0, 0, 0, 0});
		ImGui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoDockingInCentralNode);
		ImGui::PopStyleColor();

		ImGui::Begin("Controls Window", 0, ImGuiWindowFlags_NoTitleBar);
		ImGui::BeginTabBar("Controls Window Tab Bar");

		if(ImGui::BeginTabItem("Settings"))
		{
			ImGui::SeparatorText("General");
			ImGui::Indent();

			
			ImGui::Checkbox("Show onscreen log?", &settings.show_onscreen_log);

			ImGui::Unindent();
			ImGui::Dummy({20, 20});
			ImGui::SeparatorText("Camera");
			ImGui::Indent();

			if (ImGui::BeginCombo("Camera Movement", settings.orbit_camera_enabled ? "Orbit" : "Free"))
			{
				if (ImGui::Selectable("Freecam", !settings.orbit_camera_enabled))
				{
					settings.orbit_camera_enabled = false;
					internal.camera_dirty = true;
				}

				if (ImGui::Selectable("Orbit", settings.orbit_camera_enabled))
				{
					settings.orbit_camera_enabled = true;
					internal.camera_dirty = true;
				}
				ImGui::EndCombo();
			}

			if(settings.orbit_camera_enabled)
			{
				internal.camera_dirty |= ImGui::DragFloat3("Position", glm::value_ptr(settings.orbit_camera_position), 0.1f);
				internal.camera_dirty |= ImGui::DragFloat("Distance", &settings.orbit_camera_distance, 0.1f);
				internal.camera_dirty |= ImGui::DragFloat("Angle", &settings.orbit_camera_angle, 0.25f);
				settings.orbit_camera_angle = glm::clamp(settings.orbit_camera_angle, -89.9f, 89.9f);
				
				ImGui::Checkbox("Orbit automatically?", &settings.orbit_automatically);
				if(!settings.orbit_automatically)
					ImGui::BeginDisabled();

				ImGui::DragFloat("Rotations/s", &settings.orbit_camera_rotations_per_second, 0.001f, -1.0f, 1.0f);
				
				if(!settings.orbit_automatically)
					ImGui::EndDisabled();
			}

			ImGui::Unindent();
			ImGui::Dummy({20, 20});
			ImGui::SeparatorText("Accumulation & Frames");
			ImGui::Indent();

			ImGui::Checkbox("Accumulate frames?", &settings.accumulate_frames);
			if(!settings.accumulate_frames)
				ImGui::BeginDisabled();
			ImGui::Text("Accumulation Limit");
			ImGui::Checkbox("##Limit accumulated frames checkbox", &settings.limit_accumulated_frames);
			ImGui::SameLine();
			ImGui::InputInt("##Frame limit", &settings.accumulated_frame_limit, 0, 0);
			if(!settings.accumulate_frames)
				ImGui::EndDisabled();

			ImGui::Text("Framerate Limit");
			ImGui::Checkbox("##Limit framerate checkbox", &settings.fps_limit_enabled);
			ImGui::SameLine();
			ImGui::InputInt("##Framerate limit", &settings.fps_limit_value, 0, 0);

			ImGui::Unindent();
			ImGui::Dummy({20, 20});
			ImGui::SeparatorText("Shaders");
			ImGui::Indent();

			if (ImGui::Button("Recompile Shaders"))
				Compute::recompile_kernels(ComputeKernelRecompilationCondition::Force);

			if (ImGui::Button("Recompile Changed Shaders"))
				Compute::recompile_kernels(ComputeKernelRecompilationCondition::SourceChanged);

		ImGui::Checkbox("Automatically recompile changed shaders?", &settings.recompile_changed_shaders_automatically);

			ImGui::EndTabItem();
		}

		if(ImGui::BeginTabItem("Performance"))
		{
			ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0});
			ImPlot::SetNextAxisToFit(ImAxis_Y1);
			if (ImPlot::BeginPlot("Performance plot (last 512 frames)", {-1, 0}, ImPlotFlags_NoInputs))
			{
				ImPlot::SetNextFillStyle({0, 0, 0, -1}, 0.2f);
				ImPlot::PlotLine("Update time (ms)", internal.performance.update_times_ms, internal.performance.data_samples, 1.0f, 0.0f, ImPlotLineFlags_Shaded);
				ImPlot::SetNextFillStyle({0, 0, 0, -1}, 0.2f);
				ImPlot::PlotLine("Render time (ms)", internal.performance.render_times_ms, internal.performance.data_samples, 1.0f, 0.0f, ImPlotLineFlags_Shaded);
				ImPlot::EndPlot();
			}
			ImGui::PopStyleColor();
			ImGui::EndTabItem();

			float min = 1e34f;
			float max = -1e34f;
			float avg = 0.0f;

			for(auto& value : internal.performance.render_times_ms)
			{
				min = glm::fmin(min, value);
				max = glm::fmax(max, value);
				avg += value;
			}

			avg /= (float)internal.performance.data_samples;

			ImGui::Text("Average: %.2fms", avg);
			ImGui::Text("Min: %.2fms", min);
			ImGui::Text("Max: %.2fms", max);
		}

		if(ImGui::BeginTabItem("Scene Settings"))
		{
			if(ImGui::BeginCombo("EXRs", internal.current_exr.c_str()))
			{
				uint idx = 0;
				for(auto& exr : internal.exr_assets_on_disk)
				{
					if (ImGui::Selectable((exr.filename + ".exr").c_str(), exr.filename == internal.current_exr))
					{
						internal.current_exr = exr.filename;
						load_exr(idx);
					}

					idx++;
				}

				ImGui::EndCombo();
			}

			int mesh_idx_proxy = (int)sceneData.mesh_idx;
			if (ImGui::DragInt("Mesh index", &mesh_idx_proxy, 1.0f, 0, AssetManager::loaded_mesh_count() - 1))
			{
				internal.camera_dirty = true;
			}
			sceneData.mesh_idx = (unsigned int)glm::clamp(mesh_idx_proxy, 0, glm::max(AssetManager::loaded_mesh_count() - 1, 0));
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
		
		ImGui::End();

		ImGui::SetNextWindowPos({});
		ImGui::Begin("Transform tools", 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

		float icon_font_size = 50.0f;	

		ImGui::SetWindowFontScale(0.9f);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 1));

		if (ImGui::Button(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, {icon_font_size, icon_font_size}))
			internal.current_gizmo_operation = ImGuizmo::TRANSLATE;
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_ARROWS_ROTATE, {icon_font_size, icon_font_size}))
			internal.current_gizmo_operation = ImGuizmo::ROTATE;
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_EXPAND, {icon_font_size, icon_font_size}))
			internal.current_gizmo_operation = ImGuizmo::SCALE;
		
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
		ImGui::SetWindowFontScale(1.0f);

		ImGui::End();

		auto view = glm::lookAtRH(glm::vec3(sceneData.cam_pos), glm::vec3(sceneData.cam_pos) + glm::vec3(sceneData.cam_forward), glm::vec3(0.0f, 1.0f, 0.0f));

		if(internal.mouse_click_tri != -1)
		{
			auto& mesh_headers = AssetManager::get_mesh_headers();
			int header_idx = internal.mouse_click_tri;

			glm::mat4 projection = glm::perspectiveRH(glm::radians(90.0f), (float)internal.render_width / (float)internal.render_height, 0.1f, 1000.0f);

			if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), internal.current_gizmo_operation, ImGuizmo::LOCAL, glm::value_ptr(mesh_headers[header_idx].transform)))
			{
				mesh_headers[header_idx].inverse_transform = glm::inverse(mesh_headers[header_idx].transform);
				internal.camera_dirty = true;
			}
		}

		/*

		// Bless this mess
		auto& draw_list = *ImGui::GetForegroundDrawList();

		bool move_speed_bar_visible = (internal.show_move_speed_bar_time > 0 || internal.show_debug_ui);

		if(move_speed_bar_visible)
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

					draw_list.AddRectFilled(minpos, maxpos, IM_COL32(223, 223, 223, 255), 0.0f);
					draw_list.AddRect(ImVec2(minpos.x - 1, minpos.y - 1), ImVec2(maxpos.x + 1, maxpos.y + 1), IM_COL32(32, 32, 32, 255), 0.0f, 2.0f);
							
					std::string text = std::format("Camera speed: {} m/s", camera_speed_t_to_m_per_second());
					auto text_size = ImGui::CalcTextSize(text.c_str());
					draw_list.AddText(ImVec2(minpos.x + move_bar_width * 0.5f - text_size.x * 0.5f, minpos.y - 32 - text_size.y), IM_COL32(223, 223, 223, 255), text.data() ,text.data() + text.length());
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

					draw_list.AddRectFilled(minpos, maxpos, IM_COL32(223, 223, 223, 255), 0.0f);
					draw_list.AddRect(ImVec2(minpos.x - 1, minpos.y - 1), ImVec2(maxpos.x + 1, maxpos.y + 1), IM_COL32(0, 0, 0, 255), 0.0f, 2.0f);
				}
				{
					float t_bar_half_width = 4;
					float t_bar_half_height = 10;

					auto minpos = ImVec2(move_bar_padding - t_bar_half_width + move_bar_width * camera_speed_visual_t, sceneData.resolution[1] - move_bar_height - 32 - t_bar_half_height);
					auto maxpos = ImVec2(move_bar_padding + t_bar_half_width + move_bar_width * camera_speed_visual_t, sceneData.resolution[1] - 32 + t_bar_half_height);

					draw_list.AddRectFilled(minpos, maxpos, IM_COL32(223, 223, 223, 255), 0.0f);
					draw_list.AddRect(ImVec2(minpos.x - 1, minpos.y - 1), ImVec2(maxpos.x + 1, maxpos.y + 1), IM_COL32(32, 32, 32, 255), 0.0f, 2.0f);
				}
			}
		}

		if(!internal.show_debug_ui)
			return;

		

		if(settings.show_onscreen_log)
		{
			auto latest_msg = Log::get_latest_msg();
			auto message_color = (latest_msg.second == Log::MessageType::Error) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 255, 255);

			ImGui::GetForegroundDrawList()->AddText(ImVec2(10, 10), message_color,latest_msg.first.data() ,latest_msg.first.data() + latest_msg.first.length());
		}

		// Debug settings window
		// TODO: Remake this as not just a standard debug window, but something more user friendly

		ImGui::Begin(" Debug settings window");
		*/
	}

	int Raytracer::get_target_fps()
	{
		return settings.fps_limit_enabled ? 
			settings.fps_limit_value :
			1000;
	}
}