#include "Raytracer.h"

#include <GLFW/glfw3.h>

#include "Compute.h"
#include "AssetManager.h"
#include "WorldManager.h"
#include "Math.h"
#include "BVH.h"
#include "Material.h"

#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include <IconsFontAwesome6.h>
#include <ImPlot.h>

#define TINYEXR_IMPLEMENTATION
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#define TINYEXR_USE_THREAD 1
#include "tinyexr.h"


// Implicit casting as error
#pragma warning(error:4244)

/*

	// Main TODO

	// 1. Asset browser/manager
	//		- A system that automatically detects, sorts and shows the state of various assets (whether its loaded to CPU/GPU or still on disk, etc.)

	// 2. NEE for emissive objects

	// 3. Texture support

*/

namespace Raytracer
{	
	struct PixelDetailInformation
	{
		u32 hit_object;
		u32 blas_hits;
		u32 tlas_hits;
		u32 pad;
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

	struct
	{
		i32 accumulated_frame_limit { 32 };
		i32 fps_limit				{ 80 };

		bool show_onscreen_log							{ true };
		bool accumulate_frames							{ true };
		bool limit_accumulated_frames					{ false };
		bool fps_limit_enabled							{ false };

		f32 camera_speed_t						{ 0.5f };
	} settings;

	struct SceneData
	{
		u32 resolution[2] { 0, 0 };
		u32 mouse_pos[2] { 0, 0 };
		u32 accumulated_frames { 0 };
		u32 exr_width { 0 };
		u32 exr_height { 0 };
		u32 reset_accumulator { false };
		glm::mat4 camera_transform {glm::identity<glm::mat4>()};
		f32 exr_angle { 0.0f };
		u32 material_idx { 0 };
		f32 focal_distance;
		f32 blur_radius;
		u32 max_luma_idx { 0 };
		u32 selected_object_idx { UINT_MAX };
		u32 pad[3];
	} scene_data;

	struct
	{
		i32 selected_instance_idx { -1 }; // Signed so we can easily tell if they are valid or not
		i32 hovered_instance_idx { -1 };

		u32* buffer { nullptr };

		bool show_debug_ui { false };
		bool save_render_to_file { false };
		bool render_dirty { true };
		bool world_dirty { true };
		bool focus_on_clicked { false };

		f32 distance_to_hovered { 0.0f };

		u32 accumulated_frames { 0 };
		u32 render_width_px { 0 };
		u32 render_height_px { 0 };
		const u32 render_channel_count { 4 };
		
		ImGuizmo::OPERATION current_gizmo_operation { ImGuizmo::TRANSLATE };
		ImGuizmo::OPERATION axis_gizmo_bitmask { all_axis_bits };

		ComputeGPUOnlyBuffer* gpu_accumulation_buffer { nullptr };
		ComputeGPUOnlyBuffer* gpu_detail_buffer { nullptr };

		//std::vector<DiskAsset> exr_assets_on_disk;
		f32* loaded_exr_data { nullptr };
		std::string current_exr { "None" };

		u32 active_camera_idx { 0 };
		std::vector<CameraInstance> cameras;
	} internal;

	namespace Input
	{
		void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			// Unused parameters
			(void)window; (void)scancode; (void)action; (void)mods;

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
			TryFromJSONVal(save_data, settings, accumulate_frames);
			TryFromJSONVal(save_data, settings, limit_accumulated_frames);
			TryFromJSONVal(save_data, settings, fps_limit_enabled);
			
			TryFromJSONVal(save_data, settings, camera_speed_t);

			TryFromJSONVal(save_data, internal, cameras);
		}

		if(internal.cameras.empty())
			internal.cameras.push_back(CameraInstance());
	}

	// TODO: This should probably not be in Raytracer.cpp
	void load_exr(u32 index = 0)
	{
		std::string exr_path = AssetManager::get_disk_files_by_extension("exr")[index].path.string();
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

		// Finding the brightest spot on the exr, assumed to be the sun for NEE
		f32 max_luma = -1e34f;
		for(u32 idx = 0; idx < (u32)width * (u32)height; idx++)
		{
			glm::vec4 value = *(((glm::vec4*)internal.loaded_exr_data) + idx);
			f32 luma = (value.x * 0.2126f + value.y * 0.7152f + value.z * 0.0722f) * value.w;
			max_luma = glm::max(luma, max_luma);

			if(luma == max_luma)
				scene_data.max_luma_idx = idx;
		}

		exr_buffer = new ComputeWriteBuffer({internal.loaded_exr_data, (usize)(width * height * 4)});
		internal.render_dirty = true;
	}

	void init(const RaytracerInitDesc& desc)
	{
		Compute::init();

		init_internal(desc);
		init_load_saved_data();

		u32 render_area_px = internal.render_width_px * internal.render_height_px;

		internal.gpu_accumulation_buffer = new ComputeGPUOnlyBuffer((usize)(render_area_px * internal.render_channel_count * sizeof(float)));
		internal.gpu_detail_buffer = new ComputeGPUOnlyBuffer((usize)(render_area_px * sizeof(PixelDetailInformation)));

		AssetManager::init();
		
		load_exr();

		WorldManager::deserialize_scene();
	}

	// Saves data such as settings, or world state to phantasma.data.json
	void terminate_save_data()
	{
		json save_data;

		ToJSONVal(save_data, settings, accumulated_frame_limit);
		ToJSONVal(save_data, settings, fps_limit);

		ToJSONVal(save_data, settings, show_onscreen_log);
		ToJSONVal(save_data, settings, accumulate_frames);
		ToJSONVal(save_data, settings, limit_accumulated_frames);
		ToJSONVal(save_data, settings, fps_limit_enabled);

		ToJSONVal(save_data, settings, camera_speed_t);

		ToJSONVal(save_data, internal, cameras);

		std::ofstream o("phantasma.data.json");
		o << save_data << std::endl;
	}

	void terminate()
	{
		WorldManager::serialize_scene();
		terminate_save_data();
	}

	void update_orbit_camera_behavior(f32 delta_time_ms, CameraInstance& camera)
	{
		if(camera.orbit_automatically)
		{
			camera.orbit_camera_t += (delta_time_ms / 1000.0f) * camera.orbit_camera_rotations_per_second;
			camera.orbit_camera_t = wrap_number(camera.orbit_camera_t, 0.0f, 1.0f); 

			bool camera_is_orbiting = (camera.orbit_camera_rotations_per_second != 0.0f);

			internal.render_dirty |= camera_is_orbiting;
		}

		glm::vec3 position_offset = glm::vec3(0.0f, 0.0f, camera.orbit_camera_distance);

		camera.rotation = glm::vec3(camera.orbit_camera_angle, camera.orbit_camera_t * 360.0f, 0.0f);
		position_offset = position_offset * glm::mat3(glm::eulerAngleXY(glm::radians(-camera.rotation.x), glm::radians(-camera.rotation.y)));

		camera.position = camera.orbit_camera_position + position_offset;
	}

	void update_free_float_camera_behavior(float delta_time_ms, CameraInstance& camera)
	{
		i32 move_hor =	(ImGui::IsKeyDown(ImGuiKey_D))		- (ImGui::IsKeyDown(ImGuiKey_A));
		i32 move_ver =	(ImGui::IsKeyDown(ImGuiKey_Space))	- (ImGui::IsKeyDown(ImGuiKey_LeftCtrl));
		i32 move_ward =	(ImGui::IsKeyDown(ImGuiKey_W))		- (ImGui::IsKeyDown(ImGuiKey_S));
				
		glm::vec3 move_dir = glm::vec3(move_hor, move_ver, -move_ward);

		move_dir = glm::normalize(move_dir * glm::mat3(glm::eulerAngleXY(glm::radians(-camera.rotation.x), glm::radians(-camera.rotation.y))));

		bool camera_is_moving = (glm::dot(move_dir, move_dir) > 0.5f);

		if(camera_is_moving)
		{
			glm::vec3 camera_move_delta = glm::vec3(move_dir * delta_time_ms * 0.01f) * camera_speed_t_to_m_per_second();
			
			internal.render_dirty = true;
				
			camera.position += camera_move_delta;
		}

		ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
		glm::vec3 camera_delta = glm::vec3(-mouse_delta.y, -mouse_delta.x, 0);

		bool pan = ImGui::GetIO().MouseDown[2];

		bool camera_updating = (glm::dot(camera_delta, camera_delta) != 0.0f);
		bool allow_camera_rotation = !internal.show_debug_ui;

		if(pan && camera_updating)
		{
			glm::vec3 pan_vector = glm::vec3(camera_delta.y, -camera_delta.x, 0.0f) * 0.001f;

			pan_vector = pan_vector * glm::mat3(glm::eulerAngleXY(glm::radians(-camera.rotation.x), glm::radians(-camera.rotation.y)));
		
			camera.position += pan_vector;

			internal.render_dirty = true;
		}
		else if(allow_camera_rotation && camera_updating)
		{
			camera.rotation += camera_delta * 0.1f;

			bool pitch_exceeds_limit = (fabs(camera.rotation.x) > 89.9f);

			if(pitch_exceeds_limit)
				camera.rotation.x = 89.9f * sgn(camera.rotation.x);

			internal.render_dirty = true;
		}
	}

	void update_instance_selection_behavior()
	{
		if(!internal.show_debug_ui)
			return;
		
		bool clicked_on_non_gizmo = (ImGui::GetIO().MouseClicked[0] && (!ImGuizmo::IsOver() || internal.selected_instance_idx == -1) && !ImGui::IsWindowHovered() && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_AnyWindow));

		auto cursor_pos = ImGui::GetIO().MousePos;

		scene_data.mouse_pos[0] = glm::clamp((u32)cursor_pos.x, 0u, internal.render_width_px - 1);
		scene_data.mouse_pos[1] = glm::clamp((u32)cursor_pos.y, 0u, internal.render_height_px - 1);
		
		if(clicked_on_non_gizmo)
		{
			bool valid_focus_hover = internal.distance_to_hovered > 0.0f; 

			if(internal.focus_on_clicked)
			{
				if(valid_focus_hover)
				{
					internal.cameras[internal.active_camera_idx].focal_distance = internal.distance_to_hovered;
					internal.focus_on_clicked = false;
					internal.render_dirty = true;
				}
			}
			else
			{
				internal.selected_instance_idx = internal.hovered_instance_idx;
			}
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
		bool pressed_any_move_key =	
			(ImGui::IsKeyDown(ImGuiKey_W) || 
			ImGui::IsKeyDown(ImGuiKey_A) || 
			ImGui::IsKeyDown(ImGuiKey_S) || 
			ImGui::IsKeyDown(ImGuiKey_D)) &&
			!ImGui::IsAnyItemFocused();

		if(pressed_any_move_key)
			internal.cameras[internal.active_camera_idx].camera_movement_type = CameraMovementType::Freecam;

		internal.cameras[internal.active_camera_idx].camera_movement_type == CameraMovementType::Orbit
			? update_orbit_camera_behavior(delta_time_ms, internal.cameras[internal.active_camera_idx])
			: update_free_float_camera_behavior(delta_time_ms, internal.cameras[internal.active_camera_idx]);

		update_instance_selection_behavior();
		update_instance_transform_hotkeys();

		bool recompiled_any_shaders = Compute::recompile_kernels(ComputeKernelRecompilationCondition::SourceChanged);
		internal.render_dirty |= recompiled_any_shaders;

		settings.camera_speed_t += ImGui::GetIO().MouseWheel * 0.01f;
		settings.camera_speed_t = glm::fclamp(settings.camera_speed_t, 0.0f, 1.0f);
	}

	// Averages out acquired samples, and renders them to the screen
	void raytrace_average_samples(const ComputeReadWriteBuffer& screen_buffer)
	{
		f32 samples_reciprocal = 1.0f / (f32)internal.accumulated_frames;

		// TODO: pass this a buffer of heatmap colors, instead of hardcoding it into the shader
		ComputeOperation("average_accumulated.cl")
			.read_write((*internal.gpu_accumulation_buffer))
			.read_write(screen_buffer)
			.write({&samples_reciprocal, 1})
			.write({&scene_data, 1})
			.read_write(*internal.gpu_detail_buffer)
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
		// TODO: we currently don't take into account world changes!
		if(internal.render_dirty || !settings.accumulate_frames || internal.world_dirty)
		{
			scene_data.reset_accumulator = true;
			internal.render_dirty = false;
			internal.accumulated_frames = 0;
		}

		CameraInstance& active_camera = internal.cameras[internal.active_camera_idx];

		scene_data.accumulated_frames = internal.accumulated_frames;
		scene_data.selected_object_idx = internal.selected_instance_idx;
		scene_data.focal_distance = active_camera.focal_distance;
		scene_data.blur_radius = active_camera.blur_radius;

		u32 render_area = internal.render_width_px * internal.render_height_px;
		ComputeReadWriteBuffer screen_buffer({internal.buffer, (usize)(render_area)});

		glm::mat4 new_transform = glm::identity<glm::mat4>();
			
		new_transform *= glm::translate(active_camera.position);
		new_transform *= glm::eulerAngleYX(glm::radians(active_camera.rotation.y), glm::radians(active_camera.rotation.x));

		scene_data.camera_transform = new_transform;

		bool stop_accumulating_frames = settings.limit_accumulated_frames && (internal.accumulated_frames > (u32)settings.accumulated_frame_limit);

		static std::vector<BVHNode> tlas { BVHNode() };

		if(internal.world_dirty)
		{
			auto built_tlas = TLASBuilder();

			tlas = built_tlas.nodes;
			internal.world_dirty = false;
		}

		if(stop_accumulating_frames)
			goto skip_rendering_goto;

		internal.accumulated_frames++;

		ComputeOperation("raytrace.cl")
			.read_write(*internal.gpu_accumulation_buffer)	
			.read(ComputeReadBuffer({&internal.hovered_instance_idx, 1}))
			.read(ComputeReadBuffer({&internal.distance_to_hovered, 1}))
			.write(AssetManager::get_normals_compute_buffer())
			.write(AssetManager::get_tris_compute_buffer())
			.write(AssetManager::get_bvh_compute_buffer())
			.write(AssetManager::get_tri_idx_compute_buffer())
			.write(AssetManager::get_mesh_header_buffer())
			.write({&scene_data, 1})
			.write(*exr_buffer)
			.write({&WorldManager::get_world_device_data(), 1})
			.write(WorldManager::get_material_vector())
			.write(tlas)
			.read_write(*internal.gpu_detail_buffer)
			.global_dispatch({internal.render_width_px, internal.render_height_px, 1})
			.execute();

		raytrace_average_samples(screen_buffer);

		scene_data.reset_accumulator = false;

		skip_rendering_goto:;

		raytrace_save_render_to_file();
	}

	std::string material_types_as_strings[] =
	{
		"Diffuse",
		"Metal",
		"Dielectric",
		"Cook-Torrance - NOT IMPLEMENTED"
	};

	void ui_material_editor(Material& material)
	{
		bool is_diffuse = material.type == MaterialType::Diffuse;
		bool is_metal = material.type == MaterialType::Metal;
		bool is_dielectric = material.type == MaterialType::Dielectric;
		bool is_cook_torrance = material.type == MaterialType::CookTorranceBRDF;

		
		internal.render_dirty |= ImGui::ColorPicker3("Albedo", glm::value_ptr(material.albedo), ImGuiColorEditFlags_NoInputs);
		internal.render_dirty |= ImGui::DragFloat("Emissiveness", &material.albedo.a, 0.01f, 0.0f, 100.0f);
		
		if(is_dielectric)
		internal.render_dirty |= ImGui::DragFloat("Absorbtion", &material.absorbtion_coefficient, 0.01f, 0.0f, 1.0f);

		if(is_dielectric)
		internal.render_dirty |= ImGui::DragFloat("IOR", &material.ior, 0.01f, 1.0f, 2.0f);
					
		if(is_diffuse || is_metal || is_cook_torrance)
		internal.render_dirty |= ImGui::DragFloat("Specularity", &material.specularity, 0.01f, 0.0f, 1.0f);
		
		if(is_cook_torrance)
		internal.render_dirty |= ImGui::DragFloat("Roughness", &material.roughness, 0.01f, 0.0f, 1.0f);

		if(is_cook_torrance)
		internal.render_dirty |= ImGui::DragFloat("Metallic", &material.metallic, 0.01f, 0.0f, 1.0f);

		MaterialType old_type = material.type;
		std::string material_name = material_types_as_strings[(u32)material.type];

		if(ImGui::BeginCombo("Material Type", material_name.c_str()))
		{
			if(ImGui::Selectable("Diffuse", is_diffuse))
				material.type = MaterialType::Diffuse;

			if(ImGui::Selectable("Metal", is_metal))
				material.type = MaterialType::Metal;

			if(ImGui::Selectable("Dielectric", is_dielectric))
				material.type = MaterialType::Dielectric;

			if(ImGui::Selectable("Cook-Torrance - NOT IMPLEMENTED", is_dielectric))
				material.type = MaterialType::CookTorranceBRDF;

			ImGui::EndCombo();
		}

		internal.render_dirty |= (material.type != old_type);
	}

	void ui_selected_instance()
	{
		if(internal.selected_instance_idx == -1)
		{
			ImGui::Dummy({0, 100 + 170});
			return;
		}

		ImGui::SeparatorText("Transform");

		MeshInstanceHeader& instance = WorldManager::get_mesh_device_data(internal.selected_instance_idx);
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
			internal.world_dirty = true;
		}

		ImGui::Dummy({0, 20});
		ImGui::SeparatorText("Material");

		i32 mat_idx_proxy = (i32) instance.material_idx;
		internal.render_dirty |= ImGui::InputInt("Material", &mat_idx_proxy, 1);
		if(mat_idx_proxy > (i32)WorldManager::get_material_count() - 1)
			WorldManager::add_material();
		mat_idx_proxy = glm::max(mat_idx_proxy, 0);
		instance.material_idx = (u32)mat_idx_proxy;

		ui_material_editor(WorldManager::get_material_ref(instance.material_idx));
	}

	void ui()
	{
		CameraInstance& active_camera = internal.cameras[internal.active_camera_idx];

		if(!internal.show_debug_ui)
			return;

		ImGui::PushStyleColor(ImGuiCol_WindowBg, {0, 0, 0, 0});
		ImGui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoDockingInCentralNode);
		ImGui::PopStyleColor();

		if(ImGui::IsKeyPressed(ImGuiKey_Backspace))
		{
			bool any_instance_is_selected = internal.selected_instance_idx >= 0;

			if(any_instance_is_selected)
			{
				WorldManager::remove_mesh_instance(internal.selected_instance_idx);
				internal.world_dirty = true;
			}

			bool selected_index_out_of_range = (i32)WorldManager::get_world_device_data().mesh_instance_count <= internal.selected_instance_idx;

			if(selected_index_out_of_range)
			{
				internal.selected_instance_idx = -1;
			}
		}

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

			if (ImGui::BeginCombo("Type", active_camera.camera_movement_type == CameraMovementType::Orbit ? "Orbit" : "Free"))
			{
				if (ImGui::Selectable("Freecam", active_camera.camera_movement_type == CameraMovementType::Freecam))
					active_camera.camera_movement_type = CameraMovementType::Freecam;

				if (ImGui::Selectable("Orbit", active_camera.camera_movement_type == CameraMovementType::Orbit))
				{
					active_camera.camera_movement_type = CameraMovementType::Orbit;
					internal.render_dirty = true;
				}

				ImGui::EndCombo();
			}

			if(active_camera.camera_movement_type == CameraMovementType::Orbit)
			{
				internal.render_dirty |= ImGui::DragFloat3("Position", glm::value_ptr(active_camera.orbit_camera_position), 0.1f);
				internal.render_dirty |= ImGui::DragFloat("Distance", &active_camera.orbit_camera_distance, 0.1f);
				internal.render_dirty |= ImGui::DragFloat("Angle", &active_camera.orbit_camera_angle, 0.25f);
				active_camera.orbit_camera_angle = glm::clamp(active_camera.orbit_camera_angle, -89.9f, 89.9f);
				
				ImGui::Checkbox("Orbit automatically?", &active_camera.orbit_automatically);
				if(active_camera.orbit_automatically)
					ImGui::DragFloat("Rotations/s", &active_camera.orbit_camera_rotations_per_second, 0.001f, -1.0f, 1.0f);
				else
					internal.render_dirty |= ImGui::DragFloat("Rotated T", &active_camera.orbit_camera_t, 0.001f);
				
				active_camera.orbit_camera_t = wrap_number(active_camera.orbit_camera_t, 0.0f, 1.0f); 
			}

			static float blur_radius_proxy;
			internal.render_dirty |= ImGui::DragFloat("Blur amount", &blur_radius_proxy, 0.01f, 0.0f, 1.0f);
			active_camera.blur_radius = log(blur_radius_proxy * 0.2f + 1.0f);

			internal.render_dirty |= ImGui::DragFloat("Focal distance", &active_camera.focal_distance, 0.001f, 0.001f, 1e34f);

			if(!internal.focus_on_clicked)
			{
				if(ImGui::Button("Focus on cursor"))
				{
					internal.focus_on_clicked = true;
				}
			}
			else
			{
				ImGui::Button("Click to confirm");
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
			ImGui::SeparatorText("Camera Instances");
			ImGui::Indent();
			
			if (ImGui::Button("Add Camera"))
			{
				// Copy over current camera as a new camera
				internal.cameras.push_back(internal.cameras[internal.active_camera_idx]);
			}

			ImGui::Dummy({20, 20});

			int remove_idx = -1;

			for(u32 i = 0; i < internal.cameras.size(); i++)
			{
				std::string text = (i == internal.active_camera_idx)
					? std::format("[Camera {}]", i)
					: std::format("Camera {}", i);

				if(ImGui::Button(text.c_str()))
				{
					internal.active_camera_idx = i;
					internal.render_dirty = true;
				}

				ImGui::SameLine();


				bool last_camera = internal.cameras.size() == 1;

				if(!last_camera)
				{
					if(ImGui::Button(std::format("X ## {}", i).c_str()))
					{
						remove_idx = i;
					}
				}
			}

			if(remove_idx != -1)
			{
				internal.cameras.erase(internal.cameras.begin() + remove_idx);
				if(internal.active_camera_idx >= internal.cameras.size())
				{
					internal.active_camera_idx--;
				}
			}

			ImGui::EndTabItem();
		}

		if(ImGui::BeginTabItem("Scene Settings"))
		{
			ImGui::Dummy({0, 20});
			ImGui::SeparatorText("EXR Settings");
			ImGui::Indent();

			if(ImGui::BeginCombo("EXRs", internal.current_exr.c_str()))
			{
				u32 idx = 0;
				for(auto exr : AssetManager::get_disk_files_by_extension("exr"))
				{
					if (ImGui::Selectable((exr.file_name + ".exr").c_str(), exr.file_name == internal.current_exr))
					{
						internal.current_exr = exr.file_name;
						load_exr(idx);
					}

					idx++;
				}

				ImGui::EndCombo();
			}

			ImGui::EndTabItem();

			internal.render_dirty |= ImGui::DragFloat("EXR Angle", &scene_data.exr_angle, 0.1f);
			
			scene_data.exr_angle = wrap_number(scene_data.exr_angle, 0.0f, 360.0f);

			static std::string selected_mesh_name = AssetManager::get_disk_files_by_extension("gltf").begin()->file_name;
			static u32 selected_mesh_idx = 0;
			
			if(ImGui::BeginCombo("Mesh Instances", selected_mesh_name.c_str()))
			{
				u32 idx = 0;
				for(auto& mesh : AssetManager::get_disk_files_by_extension("gltf"))
				{
					if (ImGui::Selectable(mesh.file_name.c_str(), mesh.file_name == selected_mesh_name))
					{
						selected_mesh_name = mesh.file_name;
						selected_mesh_idx = idx;
					}
					idx++;
				}

				ImGui::EndCombo();
			}

			if(ImGui::Button("Add Instance"))
			{
				internal.selected_instance_idx = WorldManager::add_instance_of_mesh(selected_mesh_idx);
				internal.world_dirty = true;
			}

			ImGui::Dummy({0, 20});
			ImGui::Unindent();
			ImGui::SeparatorText("Selected Instance");
			ImGui::Indent();

			ui_selected_instance();

			ImGui::Dummy({0, 20});
			ImGui::Unindent();
			ImGui::SeparatorText("Materials");
			ImGui::Indent();

			if(ImGui::Button("Add new"))
			{
				WorldManager::add_material();
			}

			u32 number_idx = 0;
			for(u32 idx = 0; idx < WorldManager::get_material_count(); idx++)
			{
				auto current_material = WorldManager::get_material_ref(idx);

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

		glm::vec3 forward = glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f) * glm::mat3(glm::eulerAngleXY(glm::radians(-active_camera.rotation.x), glm::radians(-active_camera.rotation.y))));

		auto view = glm::lookAtRH(active_camera.position, active_camera.position + forward, glm::vec3(0.0f, 1.0f, 0.0f));

		if(internal.selected_instance_idx != -1)
		{
			i32 transform_idx = internal.selected_instance_idx;

			MeshInstanceHeader& instance = WorldManager::get_mesh_device_data(transform_idx);
			glm::mat4 transform = instance.transform;

			glm::mat4 projection = glm::perspectiveRH(glm::radians(90.0f), (f32)internal.render_width_px / (f32)internal.render_height_px, 0.1f, 1000.0f);

			if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), (ImGuizmo::OPERATION)(internal.current_gizmo_operation & internal.axis_gizmo_bitmask), ImGuizmo::LOCAL, glm::value_ptr(transform)))
			{
				instance.transform = transform;
				instance.inverse_transform = glm::inverse(transform);
				internal.world_dirty = true;
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