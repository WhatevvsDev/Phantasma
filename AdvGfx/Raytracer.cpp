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
#include "WorldManager.h"


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
#define TINYEXR_USE_THREAD 1
#include "tinyexr.h"

// Implicit casting as error
#pragma warning(error:4244)

/*

	// Main TODO

	// 1. Camera system
	//		- A proper system for managing camera, with various settings and options (DoF, FoV, etc.)

	// 2. Asset browser/manager
	//		- A system that automatically detects, sorts and shows the state of various assets (whether its loaded to CPU/GPU or still on disk, etc.)

	// 3. Some extra EXR controls
	//		- Maybe adjusting brightness
			- Follow camera rotation? so we can see the model w the same lighting from all angles

*/

namespace Raytracer
{	
	struct DiskAsset
	{
		std::filesystem::path path;
		std::string filename;
	};

	enum class MaterialType : u32
	{
		Diffuse,
		Metal,
		Dielectric
	};

	enum class LockAxis
	{
		X,
		Y,
		Z
	};

	const ImGuizmo::OPERATION axis_bits[3] = {
			(ImGuizmo::OPERATION)(ImGuizmo::TRANSLATE_X | ImGuizmo::ROTATE_X | ImGuizmo::SCALE_X),
			(ImGuizmo::OPERATION)(ImGuizmo::TRANSLATE_Y | ImGuizmo::ROTATE_Y | ImGuizmo::SCALE_Y),
			(ImGuizmo::OPERATION)(ImGuizmo::TRANSLATE_Z | ImGuizmo::ROTATE_Z | ImGuizmo::SCALE_Z)
		};

	ImGuizmo::OPERATION all_axis_bits = (ImGuizmo::OPERATION)(axis_bits[0] | axis_bits[1] | axis_bits[2]);;


	struct Material
	{
		glm::vec4 albedo { 1.0f, 1.0f, 1.0f, 0.0f };
		float ior { 1.33f };
		float absorbtion_coefficient { 0.0f };
		MaterialType type { MaterialType::Diffuse };
		float specularity { 0.0f };
	};

	Material default_material = {glm::vec4(1.0f, 0.5f, 0.7f, 1.0f), 1.5f, 0.1f, MaterialType::Diffuse, 0.0f};

	std::vector<Material> materials = {default_material};

	enum class CameraMovementType
	{
		Freecam,
		Orbit
	};

	struct
	{
		i32 accumulated_frame_limit { 32 };
		i32 fps_limit				{ 80 };

		bool show_onscreen_log							{ true };
		bool orbit_automatically						{ true };
		bool accumulate_frames							{ true };
		bool limit_accumulated_frames					{ false };
		bool fps_limit_enabled							{ false };
		bool recompile_changed_shaders_automatically	{ true };
		 
		// TODO: Replace this with an actual camera [1]
		CameraMovementType camera_movement_type { CameraMovementType::Freecam };
		glm::vec3 orbit_camera_position			{ 0.0f };
		f32 orbit_camera_distance				{ 10.0f };
		f32 orbit_camera_angle					{ 0.0f };
		f32 orbit_camera_rotations_per_second	{ 0.1f };
		f32 camera_speed_t						{ 0.5f };
		glm::vec3 saved_camera_position			{ 0.0f };
	} settings;

	struct  
	{
		glm::vec3 position;
		glm::vec3 rotation;
	} host_camera;

	struct SceneData
	{
		u32 resolution[2] { 0, 0 };
		u32 mouse_pos[2] { 0, 0 };
		u32 frame_number { 0 };
		u32 exr_width { 0 };
		u32 exr_height { 0 };
		u32 reset_accumulator { false };
		glm::mat4 camera_transform {glm::identity<glm::mat4>()};
		f32 exr_angle { 0 };
		u32 material_idx { 0 };
	} scene_data;

	struct
	{
		i32 selected_instance_idx { -1 }; // Signed so we can easily tell if they are valid or not
		i32 hovered_instance_idx { -1 };

		u32* buffer { nullptr };

		bool show_debug_ui { false };
		bool save_render_to_file { false };
		bool render_dirty { true };

		u32 accumulated_frames { 0 };
		u32 render_width_px { 0 };
		u32 render_height_px { 0 };
		const u32 render_channel_count { 4 };
		
		ImGuizmo::OPERATION current_gizmo_operation { ImGuizmo::TRANSLATE };
		ImGuizmo::OPERATION axis_gizmo_bitmask { all_axis_bits };

		ComputeGPUOnlyBuffer* gpu_accumulation_buffer { nullptr };

		std::vector<DiskAsset> exr_assets_on_disk;
		f32* loaded_exr_data { nullptr };
		std::string current_exr { "None" };

		f32 orbit_camera_t { 0.0f };

		struct
		{
			Timer timer;
			static const u32 data_samples { 256 };
			f32 update_times_ms[data_samples] {};
			f32 render_times_ms[data_samples] {};
			f32 max_time = 0.0f;
		} performance;
	} internal;

	namespace Input
	{
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
					internal.save_render_to_file = true;
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
		f32 adjusted_t = settings.camera_speed_t * 2.0f - 1.0f;
		f32 value = pow(adjusted_t * 9.95f, 2.0f) * sgn(adjusted_t);

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

		internal.render_width_px = desc.width_px;
		internal.render_height_px = desc.height_px;
		scene_data.resolution[0] = desc.width_px;
		scene_data.resolution[1] = desc.height_px;
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

			TryFromJSONVal(save_data, settings, accumulated_frame_limit);
			TryFromJSONVal(save_data, settings, fps_limit);

			TryFromJSONVal(save_data, settings, show_onscreen_log);
			TryFromJSONVal(save_data, settings, orbit_automatically);
			TryFromJSONVal(save_data, settings, accumulate_frames);
			TryFromJSONVal(save_data, settings, limit_accumulated_frames);
			TryFromJSONVal(save_data, settings, fps_limit_enabled);
			TryFromJSONVal(save_data, settings, recompile_changed_shaders_automatically);

			TryFromJSONVal(save_data, settings, camera_movement_type);
			TryFromJSONVal(save_data, settings, orbit_camera_position);
			TryFromJSONVal(save_data, settings, orbit_camera_distance);
			TryFromJSONVal(save_data, settings, orbit_camera_angle);
			TryFromJSONVal(save_data, settings, orbit_camera_rotations_per_second);
			TryFromJSONVal(save_data, settings, camera_speed_t);
			TryFromJSONVal(save_data, settings, saved_camera_position);

			TryFromJSONVal(save_data, host_camera, position);
			TryFromJSONVal(save_data, host_camera, rotation);

			TryFromJSONVal(save_data, internal, orbit_camera_t);
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
			bool kernel_already_exists = (file_extension == "cl") && Compute::kernel_exists(file_name);
			bool file_is_common_source = (file_name == "common");

			if(wrong_file_extension || kernel_already_exists || file_is_common_source)
				continue;

			if(file_extension == "cl")
			{
				Compute::create_kernel(file_path, file_name);
				continue;
			}

			if(file_extension == "exr")
			{
				internal.exr_assets_on_disk.push_back({asset_path, file_name});
				continue;
			}
		}
	}

	// TODO: This should probably not be in Raytracer.cpp
	void load_exr(u32 index = 0)
	{
		std::string exr_path = internal.exr_assets_on_disk[index].path.string();
		std::string file_name_with_extension = exr_path.substr(exr_path.find_last_of("/\\") + 1);

		internal.current_exr = file_name_with_extension;

		i32 width;
		i32 height;
		const char* err = nullptr;

		delete[] internal.loaded_exr_data;
		LoadEXR(&internal.loaded_exr_data, &width, &height, exr_path.c_str(), &err);

		if(err)
		{
			LOGERROR(err);
		}
		
		scene_data.exr_width = (u32)width;
		scene_data.exr_height = (u32)height;

		exr_buffer = new ComputeWriteBuffer({internal.loaded_exr_data, (usize)(width * height * 4)});
		internal.render_dirty = true;
	}

	void init(const RaytracerInitDesc& desc)
	{
		init_internal(desc);
		init_load_saved_data();
		init_find_assets();
		load_exr();

		u32 render_area_px = internal.render_width_px * internal.render_height_px;

		internal.gpu_accumulation_buffer = new ComputeGPUOnlyBuffer((usize)(render_area_px * internal.render_channel_count * sizeof(float)));

		internal.performance.timer.start();

		// TODO: make this "automatic", keep track of things in asset folder
		// make them loadable to CPU, and then also optionally GPU using a free list or sm [2]
		AssetManager::init();
		AssetManager::load_mesh(get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\stanfordbunny.gltf");
		AssetManager::load_mesh(get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\flat_vs_smoothed.gltf");
		AssetManager::load_mesh(get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\mid_poly_sphere.gltf");
		AssetManager::load_mesh(get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\sah_test.gltf");
		AssetManager::load_mesh(get_current_directory_path() + "\\..\\..\\AdvGfx\\assets\\utahteapot.gltf");
	}

	// Saves data such as settings, or world state to phantasma.data.json
	void terminate_save_data()
	{
		json save_data;

		ToJSONVal(save_data, settings, accumulated_frame_limit);
		ToJSONVal(save_data, settings, fps_limit);

		ToJSONVal(save_data, settings, show_onscreen_log);
		ToJSONVal(save_data, settings, orbit_automatically);
		ToJSONVal(save_data, settings, accumulate_frames);
		ToJSONVal(save_data, settings, limit_accumulated_frames);
		ToJSONVal(save_data, settings, fps_limit_enabled);
		ToJSONVal(save_data, settings, recompile_changed_shaders_automatically);

		ToJSONVal(save_data, settings, camera_movement_type);
		ToJSONVal(save_data, settings, orbit_camera_position);
		ToJSONVal(save_data, settings, orbit_camera_distance);
		ToJSONVal(save_data, settings, orbit_camera_angle);
		ToJSONVal(save_data, settings, orbit_camera_rotations_per_second);
		ToJSONVal(save_data, settings, camera_speed_t);
		ToJSONVal(save_data, settings, saved_camera_position);

		ToJSONVal(save_data, host_camera, position);
		ToJSONVal(save_data, host_camera, rotation);

		ToJSONVal(save_data, internal, orbit_camera_t);

		std::ofstream o("phantasma.data.json");
		o << save_data << std::endl;
	}

	void terminate()
	{
		terminate_save_data();
	}

	void update_orbit_camera_behavior(f32 delta_time_ms)
	{
		if(settings.orbit_automatically)
		{
			internal.orbit_camera_t += (delta_time_ms / 1000.0f) * settings.orbit_camera_rotations_per_second;
			internal.orbit_camera_t = wrap_number(internal.orbit_camera_t, 0.0f, 1.0f); 

			bool camera_is_orbiting = (settings.orbit_camera_rotations_per_second != 0.0f);

			internal.render_dirty |= camera_is_orbiting;
		}

		glm::vec3 position_offset = glm::vec3(0.0f, 0.0f, settings.orbit_camera_distance);

		host_camera.rotation = glm::vec3(settings.orbit_camera_angle, internal.orbit_camera_t * 360.0f, 0.0f);
		position_offset = position_offset * glm::mat3(glm::eulerAngleXY(glm::radians(-host_camera.rotation.x), glm::radians(-host_camera.rotation.y)));

		host_camera.position = settings.orbit_camera_position + position_offset;
	}

	void update_free_float_camera_behavior(float delta_time_ms)
	{
		i32 move_hor =	(ImGui::IsKeyDown(ImGuiKey_D))		- (ImGui::IsKeyDown(ImGuiKey_A));
		i32 move_ver =	(ImGui::IsKeyDown(ImGuiKey_Space))	- (ImGui::IsKeyDown(ImGuiKey_LeftCtrl));
		i32 move_ward =	(ImGui::IsKeyDown(ImGuiKey_W))		- (ImGui::IsKeyDown(ImGuiKey_S));
				
		glm::vec3 move_dir = glm::vec3(move_hor, move_ver, -move_ward);

		move_dir = glm::normalize(move_dir * glm::mat3(glm::eulerAngleXY(glm::radians(-host_camera.rotation.x), glm::radians(-host_camera.rotation.y))));

		bool camera_is_moving = (glm::dot(move_dir, move_dir) > 0.5f);

		if(camera_is_moving)
		{

			glm::vec3 camera_move_delta = glm::vec3(move_dir * delta_time_ms * 0.01f) * camera_speed_t_to_m_per_second();
			
			internal.render_dirty = true;
				
			host_camera.position += camera_move_delta;
		}

		ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
		glm::vec3 camera_angular_delta = glm::vec3(-mouse_delta.y, -mouse_delta.x, 0);

		bool camera_is_rotating = (glm::dot(camera_angular_delta, camera_angular_delta) != 0.0f);
		bool allow_camera_rotation = !internal.show_debug_ui;

		if(allow_camera_rotation && camera_is_rotating)
		{
			host_camera.rotation += camera_angular_delta * 0.1f;

			bool pitch_exceeds_limit = (fabs(host_camera.rotation.x) > 89.9f);

			if(pitch_exceeds_limit)
				host_camera.rotation.x = 89.9f * sgn(host_camera.rotation.x);

			internal.render_dirty = true;
		}
	}

	void update_instance_selection_behavior()
	{
		if(!internal.show_debug_ui)
			return;
		
		bool clicked_on_non_gizmo = (ImGui::GetIO().MouseReleased[0] && !ImGuizmo::IsOver() && !ImGui::IsWindowHovered() && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AnyWindow));

		auto cursor_pos = ImGui::GetIO().MousePos;

		scene_data.mouse_pos[0] = glm::clamp((u32)cursor_pos.x, 0u, internal.render_width_px - 1);
		scene_data.mouse_pos[1] = glm::clamp((u32)cursor_pos.y, 0u, internal.render_height_px - 1);

		if(clicked_on_non_gizmo)
		{
			internal.selected_instance_idx = internal.hovered_instance_idx;
		}
	}

	void axis_lock_change(LockAxis axis)
	{
		if(internal.axis_gizmo_bitmask == all_axis_bits)
		{
			internal.axis_gizmo_bitmask = axis_bits[(int)axis];
			return;
		}

		if(internal.axis_gizmo_bitmask & axis_bits[(int)axis])
		{
			internal.axis_gizmo_bitmask = (ImGuizmo::OPERATION)(internal.axis_gizmo_bitmask ^ axis_bits[(int)axis]);
		}
		else
		{
			internal.axis_gizmo_bitmask = (ImGuizmo::OPERATION)(internal.axis_gizmo_bitmask | axis_bits[(int)axis]);
		}

		if((int)internal.axis_gizmo_bitmask == 0)
		{
			internal.axis_gizmo_bitmask = all_axis_bits;
		}

	}

	void update_instance_transform_hotkeys()
	{
		bool x_axis = ImGui::IsKeyPressed(ImGuiKey_X);
		bool y_axis = ImGui::IsKeyPressed(ImGuiKey_Y);
		bool z_axis = ImGui::IsKeyPressed(ImGuiKey_Z);
		
		bool translate = ImGui::IsKeyPressed(ImGuiKey_G);
		bool rotate = ImGui::IsKeyPressed(ImGuiKey_R);
		bool scale = ImGui::IsKeyPressed(ImGuiKey_E);

		if(translate)
			internal.current_gizmo_operation = ImGuizmo::TRANSLATE;

		if(rotate)
			internal.current_gizmo_operation = ImGuizmo::ROTATE;

		if(scale)
			internal.current_gizmo_operation = ImGuizmo::SCALE;

		if(translate || rotate || scale)
			internal.axis_gizmo_bitmask = all_axis_bits;

		if(x_axis) axis_lock_change(LockAxis::X);
		if(y_axis) axis_lock_change(LockAxis::Y);
		if(z_axis) axis_lock_change(LockAxis::Z);
	}

	void update(const f32 delta_time_ms)
	{
		internal.performance.timer.reset();
		
		settings.camera_movement_type == CameraMovementType::Orbit
			? update_orbit_camera_behavior(delta_time_ms)
			: update_free_float_camera_behavior(delta_time_ms);

		update_instance_selection_behavior();
		update_instance_transform_hotkeys();

		if(settings.recompile_changed_shaders_automatically)
		{
			bool recompiled_any_shaders = Compute::recompile_kernels(ComputeKernelRecompilationCondition::SourceChanged);
			 
			internal.render_dirty |= recompiled_any_shaders;
		}

		settings.camera_speed_t += ImGui::GetIO().MouseWheel * 0.01f;
		settings.camera_speed_t = glm::fclamp(settings.camera_speed_t, 0.0f, 1.0f);

		rotate_array_right(internal.performance.update_times_ms, internal.performance.data_samples);
		internal.performance.update_times_ms[0] = internal.performance.timer.to_now();
		internal.performance.max_time = glm::max(internal.performance.max_time, internal.performance.update_times_ms[0]);
	}

	// Averages out acquired samples, and renders them to the screen
	void raytrace_average_samples(const ComputeReadWriteBuffer& screen_buffer, const ComputeGPUOnlyBuffer& accumulated_samples)
	{
		f32 samples_reciprocal = 1.0f / (f32)internal.accumulated_frames;

		// TODO: pass this a buffer of heatmap colors, instead of hardcoding it into the shader
		ComputeOperation("average_accumulated.cl")
			.read_write(accumulated_samples)
			.read_write(screen_buffer)
			.write({&samples_reciprocal, 1})
			.write({&scene_data, 1})
			.global_dispatch({internal.render_width_px, internal.render_height_px, 1})
			.execute();
	}

	void raytrace_save_render_to_file() 
	{
		if(!internal.save_render_to_file)
			return;

		stbi_write_jpg("render.jpg", internal.render_width_px, internal.render_height_px, internal.render_channel_count, internal.buffer, 100);
		LOGDEBUG("Saved screenshot.");
		internal.save_render_to_file = false;
	}

	void raytrace()
	{
		internal.performance.timer.reset();
		scene_data.frame_number++;

		// TODO: we currently don't take into account world changes!
		if(internal.render_dirty || !settings.accumulate_frames)
		{
			scene_data.reset_accumulator = true;
			internal.render_dirty = false;
			internal.accumulated_frames = 0;
		}

		u32 render_area = internal.render_width_px * internal.render_height_px;
		ComputeReadWriteBuffer screen_buffer({internal.buffer, (usize)(render_area)});

		glm::mat4 new_transform = glm::identity<glm::mat4>();

		new_transform *= glm::translate(host_camera.position);
		new_transform *= glm::eulerAngleY(glm::radians(host_camera.rotation.y));
		new_transform *= glm::eulerAngleX(glm::radians(host_camera.rotation.x));
				
		scene_data.camera_transform = new_transform;

		bool stop_accumulating_frames = settings.limit_accumulated_frames && (internal.accumulated_frames > (u32)settings.accumulated_frame_limit);

		if(stop_accumulating_frames)
			goto skip_rendering_goto;

		internal.accumulated_frames++;

		ComputeOperation("raytrace.cl")
			.read_write(*internal.gpu_accumulation_buffer)
			.read(ComputeReadBuffer({&internal.hovered_instance_idx, 1}))
			.write(AssetManager::get_normals_compute_buffer())
			.write(AssetManager::get_tris_compute_buffer())
			.write(AssetManager::get_bvh_compute_buffer())
			.write(AssetManager::get_tri_idx_compute_buffer())
			.write(AssetManager::get_mesh_header_buffer())
			.write({&scene_data, 1})
			.write(*exr_buffer)
			.write({&WorldManager::get_world_device_data(), 1})
			.write(materials)
			.global_dispatch({internal.render_width_px, internal.render_height_px, 1})
			.execute();

		raytrace_average_samples(screen_buffer, (*internal.gpu_accumulation_buffer));

		scene_data.reset_accumulator = false;

		skip_rendering_goto:;

		// We query performance here to avoid screenshot/ui code
		rotate_array_right(internal.performance.render_times_ms, internal.performance.data_samples);
		internal.performance.render_times_ms[0] = internal.performance.timer.to_now();
		internal.performance.max_time = glm::max(internal.performance.max_time, internal.performance.render_times_ms[0]);

		raytrace_save_render_to_file();
	}

	std::string material_types_as_strings[3] =
	{
		"Diffuse",
		"Metal",
		"Dielectric"
	};

	void ui_material_editor(Material& material)
	{
		bool is_diffuse = material.type == MaterialType::Diffuse;
		bool is_metal = material.type == MaterialType::Metal;
		bool is_dielectric = material.type == MaterialType::Dielectric;

		
		internal.render_dirty |= ImGui::ColorPicker3("Albedo", glm::value_ptr(material.albedo), ImGuiColorEditFlags_NoInputs);
		internal.render_dirty |= ImGui::DragFloat("Absorbtion", &material.absorbtion_coefficient, 0.01f, 0.0f, 1.0f);

		if(is_dielectric)
		internal.render_dirty |= ImGui::DragFloat("IOR", &material.ior, 0.01f, 1.0f, 2.0f);
					
		if(is_diffuse || is_metal)
		internal.render_dirty |= ImGui::DragFloat("Specularity", &material.specularity, 0.01f, 0.0f, 1.0f);
					
		if(ImGui::BeginCombo("Material Type", material_types_as_strings[(u32)material.type].c_str()))
		{
			if(ImGui::Selectable("Diffuse", is_diffuse))
			{
				material.type = MaterialType::Diffuse;
				internal.render_dirty = true;
			}
			if(ImGui::Selectable("Metal", is_metal))
			{
				material.type = MaterialType::Metal;
				internal.render_dirty = true;
			}
			if(ImGui::Selectable("Dielectric", is_dielectric))
			{
				material.type = MaterialType::Dielectric;
				internal.render_dirty = true;
			}

			ImGui::EndCombo();
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

			if (ImGui::BeginCombo("Camera Movement", settings.camera_movement_type == CameraMovementType::Orbit ? "Orbit" : "Free"))
			{
				if (ImGui::Selectable("Freecam", settings.camera_movement_type == CameraMovementType::Freecam))
					settings.camera_movement_type = CameraMovementType::Freecam;

				if (ImGui::Selectable("Orbit", settings.camera_movement_type == CameraMovementType::Orbit))
				{
					settings.camera_movement_type = CameraMovementType::Orbit;
					internal.render_dirty = true;
				}

				ImGui::EndCombo();
			}

			if(settings.camera_movement_type == CameraMovementType::Orbit)
			{
				internal.render_dirty |= ImGui::DragFloat3("Position", glm::value_ptr(settings.orbit_camera_position), 0.1f);
				internal.render_dirty |= ImGui::DragFloat("Distance", &settings.orbit_camera_distance, 0.1f);
				internal.render_dirty |= ImGui::DragFloat("Angle", &settings.orbit_camera_angle, 0.25f);
				settings.orbit_camera_angle = glm::clamp(settings.orbit_camera_angle, -89.9f, 89.9f);
				
				ImGui::Checkbox("Orbit automatically?", &settings.orbit_automatically);
				if(!settings.orbit_automatically)
					ImGui::BeginDisabled();

				ImGui::DragFloat("Rotations/s", &settings.orbit_camera_rotations_per_second, 0.001f, -1.0f, 1.0f);
				
				if(!settings.orbit_automatically)
					ImGui::EndDisabled();

				if(settings.orbit_automatically)
					ImGui::BeginDisabled();

				internal.render_dirty |= ImGui::DragFloat("Rotation t", &internal.orbit_camera_t, 0.001f);
				internal.orbit_camera_t = wrap_number(internal.orbit_camera_t, 0.0f, 1.0f); 

				if(settings.orbit_automatically)
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
			ImGui::InputInt("##Framerate limit", &settings.fps_limit, 0, 0);

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

			f32 min = 1e34f;
			f32 max = -1e34f;
			f32 avg = 0.0f;

			for(auto& value : internal.performance.render_times_ms)
			{
				min = glm::fmin(min, value);
				max = glm::fmax(max, value);
				avg += value;
			}

			avg /= (f32)internal.performance.data_samples;

			ImGui::Text("Average: %.2fms", avg);
			ImGui::Text("Min: %.2fms", min);
			ImGui::Text("Max: %.2fms", max);
		}

		if(ImGui::BeginTabItem("Scene Settings"))
		{
			ImGui::Dummy({0, 20});
			ImGui::SeparatorText("EXR Settings");
			ImGui::Indent();

			if(ImGui::BeginCombo("EXRs", internal.current_exr.c_str()))
			{
				u32 idx = 0;
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

			ImGui::EndTabItem();

			internal.render_dirty |= ImGui::DragFloat("EXR Angle", &scene_data.exr_angle, 0.1f);
			
			scene_data.exr_angle = wrap_number(scene_data.exr_angle, 0.0f, 360.0f);

			static u32 selected_mesh_idx_to_instance = 0;

			if(ImGui::BeginCombo("Mesh Instances", std::to_string(selected_mesh_idx_to_instance).c_str()))
			{
				uint idx = 0;
				for(auto& mesh_header : AssetManager::get_mesh_headers())
				{
					(void)mesh_header;

					if (ImGui::Selectable(std::to_string(idx).c_str(), idx == selected_mesh_idx_to_instance))
					{
						selected_mesh_idx_to_instance = idx;
					}

					idx++;
				}

				ImGui::EndCombo();
			}

			if(ImGui::Button("Add Instance"))
			{
				internal.selected_instance_idx = WorldManager::add_instance_of_mesh(selected_mesh_idx_to_instance);
				internal.render_dirty = true;
			}

			ImGui::Dummy({0, 20});
			ImGui::Unindent();
			ImGui::SeparatorText("Selected Instance");
			ImGui::Indent();

			if(internal.selected_instance_idx != -1)
			{
				ImGui::SeparatorText("Transform");

				auto& instance = WorldManager::get_world_device_data().instances[internal.selected_instance_idx];
				bool transformed = false;

				glm::vec3 translation;
				glm::vec3 rotation;
				glm::vec3 scale;

				ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(instance.transform), glm::value_ptr(translation), glm::value_ptr(rotation), glm::value_ptr(scale));
				
				transformed |= ImGui::InputFloat3("Translation", glm::value_ptr(translation));
				transformed |= ImGui::InputFloat3("Rotation", glm::value_ptr(rotation));
				transformed |= ImGui::InputFloat3("Scale", glm::value_ptr(scale));
				
				ImGuizmo::RecomposeMatrixFromComponents(glm::value_ptr(translation), glm::value_ptr(rotation), glm::value_ptr(scale), glm::value_ptr(instance.transform));

				if(transformed)
				{
					instance.inverse_transform = glm::inverse(instance.transform);
					internal.render_dirty = true;
				}

				ImGui::Dummy({0, 20});
				ImGui::SeparatorText("Material");

				i32 mat_idx_proxy = (i32) instance.material_idx;
				internal.render_dirty |= ImGui::InputInt("Material", &mat_idx_proxy, 1);
				mat_idx_proxy = glm::clamp(mat_idx_proxy, 0, (i32)materials.size() - 1);
				instance.material_idx = (u32)mat_idx_proxy;

				ui_material_editor(materials[instance.material_idx]);
			}
			else
			{
				ImGui::Dummy({0, 100 + 170});
			}

			ImGui::Dummy({0, 20});
			ImGui::Unindent();
			ImGui::SeparatorText("Materials");
			ImGui::Indent();

			if(ImGui::Button("Add new"))
			{
				materials.push_back(Material());
			}

			u32 number_idx = 0;
			for(auto& current_material : materials)
			{
				if (ImGui::TreeNode(("## material list index" + std::to_string(number_idx)).c_str()))
				{
					ImGui::SameLine();
					ImGui::Text((material_types_as_strings[(u32)current_material.type].c_str()));

					ui_material_editor(current_material);

					ImGui::TreePop();
				}
				else
				{
					ImGui::SameLine();
					ImGui::Text((material_types_as_strings[(u32)current_material.type].c_str()));
				}
				number_idx++;
			}
		}

		ImGui::EndTabBar();
		
		ImGui::End();

		ImGui::SetNextWindowPos({});
		ImGui::Begin("Transform tools", 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

		f32 icon_font_size = 50.0f;	

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

		glm::vec3 forward = glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f) * glm::mat3(glm::eulerAngleXY(glm::radians(-host_camera.rotation.x), glm::radians(-host_camera.rotation.y))));;

		auto view = glm::lookAtRH(host_camera.position, host_camera.position + forward, glm::vec3(0.0f, 1.0f, 0.0f));

		if(internal.selected_instance_idx != -1)
		{
			i32 transform_idx = internal.selected_instance_idx;

			MeshInstanceHeader& instance = WorldManager::get_world_device_data().instances[transform_idx];
			glm::mat4& transform = instance.transform;

			glm::mat4 projection = glm::perspectiveRH(glm::radians(90.0f), (f32)internal.render_width_px / (f32)internal.render_height_px, 0.1f, 1000.0f);

			if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), (ImGuizmo::OPERATION)(internal.current_gizmo_operation & internal.axis_gizmo_bitmask), ImGuizmo::LOCAL, glm::value_ptr(transform)))
			{
				instance.inverse_transform = glm::inverse(transform);
				internal.render_dirty = true;
			}
		}

		if(settings.show_onscreen_log)
		{
			auto latest_msg = Log::get_latest_msg();
			auto message_color = (latest_msg.second == Log::MessageType::Error) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 255, 255);

			auto text_size = ImGui::CalcTextSize(latest_msg.first.data());

			ImGui::GetForegroundDrawList()->AddText(ImVec2(internal.render_width_px / 2 - text_size.x / 2, 10), message_color, latest_msg.first.data() ,latest_msg.first.data() + latest_msg.first.length());
		}
	}

	u32 Raytracer::get_target_fps()
	{
		return settings.fps_limit_enabled ? 
			settings.fps_limit :
			1000;
	}
}