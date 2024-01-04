#include "Raytracer.h"

#include <GLFW/glfw3.h>

#include "Compute.h"
#include "Assets.h"
#include "World.h"
#include "Math.h"
#include "BVH.h"
#include "Material.h"
#include "Camera.h"

#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include <IconsFontAwesome6.h>
#include <ImPlot.h>


// Implicit casting as error
#pragma warning(error:4244)

/*

	// TODO

	// Ask Jacco (or look at lighthouse) how he manages the different data for meshes (tris/idx/uv/normal). Does he pack it in one buffer?

	// Main Feature TODO

	// 1. Asset browser/manager
	//		- A system that automatically detects, sorts and shows the state of various assets (whether its loaded to CPU/GPU or still on disk, etc.)

	// 2. NEE for emissive objects

	// 3. A bunch of error checking, we work primarily with indices into arrays and such, and there is no checking when loading a scene for example

	// 4. Some sort of scene management? current work is sloppy
*/

namespace Raytracer
{	
	enum class ViewType : u32
	{
		Render,
		Albedo,
		Normal,
		BLAS,
		TLAS,
		AS,
		ViewTypeRange
	};

	struct PixelDetailInformation
	{
		u32 hit_object;
		u32 blas_hits;
		u32 tlas_hits;
		u32 pad;
		glm::vec4 hit_position;
	};

	struct
	{
		i32 accumulated_frame_limit		{ 32 };
		i32 fps_limit					{ 80 };

		bool show_onscreen_log			{ true };
		bool accumulate_frames			{ true };
		bool limit_accumulated_frames	{ false };
		bool fps_limit_enabled			{ false };
	} settings;

	struct SceneData
	{
		u32 resolution[2]				{ 0, 0 };
		u32 mouse_pos[2]				{ 0, 0 };
		i32 exr_size[2]					{ 0, 0 };
		u32 accumulated_frames			{ 0 };
		u32 reset_accumulator			{ false };
		glm::mat4 camera_transform{ glm::identity<glm::mat4>() };
		glm::mat4 old_camera_transform{ glm::identity<glm::mat4>() };
		glm::mat4 old_proj_camera_transform{ glm::identity<glm::mat4>() };
		glm::mat4 inv_old_camera_transform	{ glm::identity<glm::mat4>() };
		f32 exr_angle					{ 0.0f };
		u32 material_idx				{ 0 };
		u32 pad[2];
	} scene_data;

	struct
	{
		ViewType view_type				{ ViewType::Render };
		i32 selected_instance_idx		{ -1 }; // Signed so we can easily tell if they are valid or not (kind of a waste, also kind of not since its 1 bit who cares)
		i32 hovered_instance_idx		{ -1 };

		u32* buffer						{ nullptr };

		bool show_debug_ui				{ false };
		bool render_dirty				{ true };
		bool world_dirty				{ true };
		bool focus_on_clicked			{ false };

		f32 distance_to_hovered			{ 0.0f };

		u32 accumulated_frames			{ 0 };
		u32 render_width_px				{ 0 };
		u32 render_height_px			{ 0 };
		const u32 render_channel_count	{ 4 };
		
		ImGuizmo::OPERATION current_gizmo_operation { ImGuizmo::TRANSLATE };

		ComputeWriteBuffer* exr_buffer{ nullptr };
		ComputeGPUOnlyBuffer* gpu_accumulation_buffer { nullptr };
		ComputeGPUOnlyBuffer* gpu_detail_buffer { nullptr };

		ComputeGPUOnlyBuffer* gpu_primary_ray_buffer { nullptr };

		//std::vector<DiskAsset> exr_assets_on_disk;
		f32* loaded_exr_data { nullptr };
		std::string current_exr { "None" };

		u32 active_camera_idx { 0 };
		std::vector<Camera::Instance> cameras;

		Camera::Instance& get_active_camera_ref() { return cameras[active_camera_idx]; }

		struct
		{
			const usize array_size { 512 };
			Timer render_passes_timer;

			u32 current_index { 0 };
			f32 pre_render_time[512] { };
			f32 primary_ray_gen_time[512] { };
			f32 ray_trace_time[512] { };
			f32 average_samples_time[512] { };

		} performance;
	} internal;

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
			TryFromJSONVal(save_data, internal, cameras);
		}

		if(internal.cameras.empty())
			internal.cameras.push_back(Camera::Instance());
	}

	void switch_skybox(u32 index)
	{
		auto& exr = Assets::get_exr_by_index(index);
		delete internal.exr_buffer;
		internal.exr_buffer = new ComputeWriteBuffer({ exr.data, (usize)(exr.width * exr.height * 4) });
		scene_data.exr_size[0] = exr.width;
		scene_data.exr_size[1] = exr.height;

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
		internal.gpu_primary_ray_buffer = new ComputeGPUOnlyBuffer((usize)(render_area_px * 44)); // TODO: This is hardcoded, it should not be!

		Assets::init();

		switch_skybox(0);

		World::deserialize_scene();

		internal.performance.render_passes_timer.start();
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
		ToJSONVal(save_data, internal, cameras);

		std::ofstream o("phantasma.data.json");
		o << save_data << std::endl;
	}

	void terminate()
	{
		World::serialize_scene();
		terminate_save_data();
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
					internal.get_active_camera_ref().focal_distance = internal.distance_to_hovered;
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

	void update_instance_transform_hotkeys()
	{
		bool translate = ImGui::IsKeyPressed(ImGuiKey_G);
		bool rotate = ImGui::IsKeyPressed(ImGuiKey_R);
		bool scale = ImGui::IsKeyPressed(ImGuiKey_E);

		if(translate)
			internal.current_gizmo_operation = ImGuizmo::TRANSLATE;

		if(rotate)
			internal.current_gizmo_operation = ImGuizmo::ROTATE;

		if(scale)
			internal.current_gizmo_operation = ImGuizmo::SCALE;
	}

	void update_input()
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			Raytracer::terminate();
			exit(0);
		}

		if (ImGui::IsKeyPressed(ImGuiKey_F1))
		{
			internal.show_debug_ui = !internal.show_debug_ui;
		}			
	}

	void update(const f32 delta_time_ms)
	{		
		update_input();

		auto& active_camera = internal.get_active_camera_ref();

		bool pressed_any_move_key =	
			(ImGui::IsKeyDown(ImGuiKey_W) || 
			ImGui::IsKeyDown(ImGuiKey_A) || 
			ImGui::IsKeyDown(ImGuiKey_S) || 
			ImGui::IsKeyDown(ImGuiKey_D)) &&
			!ImGui::IsAnyItemFocused();

		bool ui_closed = !internal.show_debug_ui;

		if(ui_closed && pressed_any_move_key)
			active_camera.movement_type = Camera::MovementType::Freecam;

		if(ui_closed || active_camera.movement_type == Camera::MovementType::Orbit)
			internal.render_dirty |= Camera::update_instance(delta_time_ms, active_camera);

		update_instance_selection_behavior();
		update_instance_transform_hotkeys();

		bool recompiled_any_shaders = Compute::recompile_kernels(ComputeKernelRecompilationCondition::SourceChanged);
		internal.render_dirty |= recompiled_any_shaders;
	}

	// TODO: figure out a better way to do this
	std::vector<BVHNode> tlas{ BVHNode() };
	std::vector<u32> tlas_idx{ };

	void raytrace_save_render_to_file()
	{
		if (!ImGui::IsKeyReleased(ImGuiKey_P))
			return;

		stbi_write_jpg("render.jpg", internal.render_width_px, internal.render_height_px, internal.render_channel_count, internal.buffer, 100);
		LOGDEBUG("Saved screenshot.");
	}

	// Averages out acquired samples, and renders them to the screen
	void raytrace_trace_rays(const ComputeReadWriteBuffer& screen_buffer)
	{
		ComputeOperation("raytrace.cl")
			.read_write(*internal.gpu_accumulation_buffer)	
			.read_write(screen_buffer)
			.read(ComputeReadBuffer({&internal.hovered_instance_idx, 1}))
			.read(ComputeReadBuffer({&internal.distance_to_hovered, 1}))
			.write(Assets::get_normals_compute_buffer())
			.write(Assets::get_uvs_compute_buffer())
			.write(Assets::get_tris_compute_buffer())
			.write(Assets::get_bvh_compute_buffer())
			.write(Assets::get_tri_idx_compute_buffer())
			.write(Assets::get_mesh_header_buffer())
			.write(Assets::get_texture_compute_buffer())
			.write(Assets::get_texture_header_buffer())
			.write({&scene_data, 1})
			.write(*internal.exr_buffer)
			.write({&World::get_world_device_data(), 1})
			.write(World::get_material_vector())
			.write(tlas)
			.write(tlas_idx)
			.read_write(*internal.gpu_detail_buffer)
			.read_write((*internal.gpu_primary_ray_buffer))
			.global_dispatch({internal.render_width_px, internal.render_height_px, 1})
			.execute();

		internal.accumulated_frames++;

		// TOOD: make this cleaner (for all of them)
		internal.performance.ray_trace_time[internal.performance.current_index] = internal.performance.render_passes_timer.peek_delta();
	}

	void raytrace_average_samples(const ComputeReadWriteBuffer& screen_buffer)
	{
		struct GeneratePrimaryRaysArgs
		{
			f32 samples_reciprocal;
			u32 width;
			u32 height;
			ViewType view_type;
			u32 selected_object_idx;
			u32 pad[3];
		} args;

		args.samples_reciprocal = 1.0f / (f32)internal.accumulated_frames;
		args.width = internal.render_width_px;
		args.height = internal.render_height_px;
		args.view_type = internal.view_type;
		args.selected_object_idx = internal.selected_instance_idx;

		ComputeOperation("average_accumulated.cl")
			.read_write((*internal.gpu_accumulation_buffer))
			.read_write(screen_buffer)
			.write({&args, 1})
			.read_write(*internal.gpu_detail_buffer)
			.global_dispatch({internal.render_width_px, internal.render_height_px, 1})
			.execute();

		internal.performance.average_samples_time[internal.performance.current_index] = internal.performance.render_passes_timer.peek_delta();
	}

	void raytrace_generate_primary_rays()
	{
		auto& active_camera = internal.get_active_camera_ref();

		struct GeneratePrimaryRaysArgs
		{
			u32 width;
			u32 height;
			u32 accumulated_frames;
			f32 blur_radius;
			f32 focal_distance;
			f32 camera_fov;
			f32 pad[2];
			glm::mat4 camera_transform;
		} args;

		args.width = internal.render_width_px;
		args.height = internal.render_height_px;
		args.accumulated_frames = internal.accumulated_frames;
		args.blur_radius = active_camera.blur_radius;
		args.focal_distance = active_camera.focal_distance;
		args.camera_fov = 110;
		args.camera_transform = Camera::get_instance_matrix(active_camera);

		ComputeOperation("generate_primary_rays.cl")
			.write({&args, 1})
			.read_write((*internal.gpu_primary_ray_buffer))
			.global_dispatch({internal.render_width_px, internal.render_height_px, 1})
			.execute();

		internal.performance.primary_ray_gen_time[internal.performance.current_index] = internal.performance.render_passes_timer.peek_delta();
	}

	void raytrace()
	{
		internal.performance.current_index++;
		internal.performance.current_index %= internal.performance.array_size;

		internal.performance.render_passes_timer.reset();

		if(internal.render_dirty || !settings.accumulate_frames || internal.world_dirty)
		{
			scene_data.reset_accumulator = true;
			internal.render_dirty = false;
			internal.accumulated_frames = 0;
		}

		auto& active_camera = internal.get_active_camera_ref();

		glm::mat4 projection = glm::perspectiveRH(glm::radians(90.0f), (f32)internal.render_width_px / (f32)internal.render_height_px, 0.1f, 1000.0f);
		scene_data.accumulated_frames = internal.accumulated_frames;

		scene_data.old_camera_transform = scene_data.camera_transform;
		scene_data.old_proj_camera_transform = projection * scene_data.camera_transform;
		scene_data.inv_old_camera_transform = glm::inverse(scene_data.camera_transform);
		scene_data.camera_transform = Camera::get_instance_matrix(active_camera);

		u32 render_area = internal.render_width_px * internal.render_height_px;
		ComputeReadWriteBuffer screen_buffer({internal.buffer, (usize)(render_area)});

		bool accumulate_frames = !(settings.limit_accumulated_frames && (internal.accumulated_frames > (u32)settings.accumulated_frame_limit));

		internal.performance.pre_render_time[internal.performance.current_index] = internal.performance.render_passes_timer.lap_delta();

		if(internal.world_dirty)
		{
			BVH built_tlas = BVH();
			BuildTLAS(built_tlas);

			tlas_idx = built_tlas.primitive_idx;
			tlas = built_tlas.nodes;
			internal.world_dirty = false;
		}


		if(accumulate_frames)
		{
			raytrace_generate_primary_rays();
			raytrace_trace_rays(screen_buffer);
			raytrace_average_samples(screen_buffer);

			scene_data.reset_accumulator = false;
		}

		raytrace_save_render_to_file();
	}

	constexpr std::string material_type_to_string(const MaterialType& type)
	{
		switch(type)
		{
			case MaterialType::Diffuse: return "Diffuse";
			case MaterialType::Metal: return "Metal";
			case MaterialType::Dielectric: return "Dielectric";
			case MaterialType::CookTorranceBRDF: return "Cook-Torrance";
		}
		return "";
	}

	constexpr std::string view_type_to_string(const ViewType& type)
	{
		switch(type)
		{
			case ViewType::Render: return "Render";
			case ViewType::Albedo: return "Albedo";
			case ViewType::Normal: return "Normal";
			case ViewType::BLAS: return "BLAS";
			case ViewType::TLAS: return "TLAS";
			case ViewType::AS: return "AS";
		}
		return "";
	}

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

		if(ImGui::BeginCombo("Material Type", material_type_to_string(material.type).c_str()))
		{
			for(i32 i = 0; i < (i32)MaterialType::MaterialTypeRange; i++)
			{
				MaterialType type = (MaterialType)i;
				bool is_type = type == material.type;

				if(ImGui::Selectable(material_type_to_string(type).c_str(), is_type))
					material.type = type;
			}

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

		MeshInstanceHeader& instance = World::get_mesh_device_data(internal.selected_instance_idx);
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

		internal.render_dirty |= ImGui::InputInt("Texture index", &instance.texture_idx, 1, 1);
		instance.texture_idx = glm::clamp(instance.texture_idx, -1, (i32)Assets::get_texture_count() - 1);

		i32 mat_idx_proxy = (i32) instance.material_idx;
		internal.render_dirty |= ImGui::InputInt("Material", &mat_idx_proxy, 1);
		if(mat_idx_proxy > (i32)World::get_material_count() - 1)
			World::add_material();
		mat_idx_proxy = glm::max(mat_idx_proxy, 0);
		instance.material_idx = (u32)mat_idx_proxy;

		ui_material_editor(World::get_material_ref(instance.material_idx));
	}

	void ui()
	{
		auto& active_camera = internal.get_active_camera_ref();

		if(!internal.show_debug_ui)
			return;

		ImGui::PushStyleColor(ImGuiCol_WindowBg, {0, 0, 0, 0});
		ImGui::DockSpaceOverViewport(0, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoDockingInCentralNode);
		ImGui::PopStyleColor();

		ImPlot::ShowDemoWindow();

		if(ImGui::IsKeyPressed(ImGuiKey_Backspace) && !ImGui::IsAnyItemActive())
		{
			bool any_instance_is_selected = internal.selected_instance_idx >= 0;

			if(any_instance_is_selected)
			{
				World::remove_mesh_instance(internal.selected_instance_idx);
				internal.world_dirty = true;
			}

			bool selected_index_out_of_range = (i32)World::get_world_device_data().mesh_instance_count <= internal.selected_instance_idx;

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

			if (ImGui::BeginCombo("View Type", view_type_to_string(internal.view_type).c_str()))
			{
				for(i32 i = 0; i < (i32)ViewType::ViewTypeRange; i++)
				{
					auto type = (ViewType)i;
					bool is_type = internal.view_type == type;

					if (ImGui::Selectable(view_type_to_string(type).c_str(), is_type))
						internal.view_type = type;
				}

				ImGui::EndCombo();
			}

			ImGui::Unindent();
			ImGui::Dummy({20, 20});
			ImGui::SeparatorText("Camera");
			ImGui::Indent();

			if (ImGui::BeginCombo("Type", active_camera.movement_type == Camera::MovementType::Orbit ? "Orbit" : "Free"))
			{
				if (ImGui::Selectable("Freecam", active_camera.movement_type == Camera::MovementType::Freecam))
					active_camera.movement_type = Camera::MovementType::Freecam;

				if (ImGui::Selectable("Orbit", active_camera.movement_type == Camera::MovementType::Orbit))
				{
					active_camera.movement_type = Camera::MovementType::Orbit;
					internal.render_dirty = true;
				}

				ImGui::EndCombo();
			}

			if(active_camera.movement_type == Camera::MovementType::Orbit)
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
			internal.render_dirty |= ImGui::DragFloat("Blur amount", &blur_radius_proxy, 1.0f, 0.0f, 1000.0f);
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
				internal.cameras.push_back(internal.get_active_camera_ref());
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
				for(auto exr : Assets::get_disk_files_by_extension("exr"))
				{
					if (ImGui::Selectable((exr.file_name + ".exr").c_str(), exr.file_name == internal.current_exr))
					{
						internal.current_exr = exr.file_name;
						
						// Load actual exr
						switch_skybox(idx);
					}

					idx++;
				}

				ImGui::EndCombo();
			}

			ImGui::EndTabItem();

			internal.render_dirty |= ImGui::DragFloat("EXR Angle", &scene_data.exr_angle, 0.1f);
			
			scene_data.exr_angle = wrap_number(scene_data.exr_angle, 0.0f, 360.0f);

			static std::string selected_mesh_name = Assets::get_disk_files_by_extension("gltf").begin()->file_name;
			static u32 selected_mesh_idx = 0;
			
			if(ImGui::BeginCombo("Mesh Instances", selected_mesh_name.c_str()))
			{
				u32 idx = 0;
				for(auto& mesh : Assets::get_disk_files_by_extension("gltf"))
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
				internal.selected_instance_idx = World::add_instance_of_mesh(selected_mesh_idx);
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
				World::add_material();
			}

			u32 number_idx = 0;
			for(u32 idx = 0; idx < World::get_material_count(); idx++)
			{
				auto current_material = World::get_material_ref(idx);

				if (ImGui::TreeNode(("## material list index" + std::to_string(number_idx)).c_str()))
				{
					ImGui::SameLine();
					ImGui::Text((material_type_to_string(current_material.type).c_str()));

					ui_material_editor(current_material);

					ImGui::TreePop();
				}
				else
				{
					ImGui::SameLine();
					ImGui::Text((material_type_to_string(current_material.type).c_str()));
				}
				number_idx++;
			}
		}

		if(ImGui::BeginTabItem("Performance"))
		{
			// TODO: make this a nice stacked graph
			ImGui::Text(std::format("pre_render_time time is {} ms", internal.performance.pre_render_time[internal.performance.current_index]).c_str());
			ImGui::Text(std::format("primary_ray_gen_time time is {} ms", internal.performance.primary_ray_gen_time[internal.performance.current_index]).c_str());
			ImGui::Text(std::format("ray_trace_time time is {} ms", internal.performance.ray_trace_time[internal.performance.current_index]).c_str());
			ImGui::Text(std::format("average_samples_time time is {} ms", internal.performance.average_samples_time[internal.performance.current_index]).c_str());
				
			if (ImPlot::BeginPlot("Raytracing performance"))
			{
				// TODO: make a nice performance logger, that can get cumulative time and regular time and plot it on its own
				ImPlot::PlotLine("average_samples_time",	internal.performance.pre_render_time,		internal.performance.array_size, 1.0f, 0.0f, ImPlotLineFlags_Shaded, internal.performance.current_index);
				ImPlot::PlotLine("ray_trace_time",			internal.performance.ray_trace_time,		internal.performance.array_size, 1.0f, 0.0f, ImPlotLineFlags_Shaded, internal.performance.current_index);
				ImPlot::PlotLine("primary_ray_gen_time",	internal.performance.primary_ray_gen_time,	internal.performance.array_size, 1.0f, 0.0f, ImPlotLineFlags_Shaded, internal.performance.current_index);
				ImPlot::PlotLine("pre_render_time",			internal.performance.pre_render_time,		internal.performance.array_size, 1.0f, 0.0f, ImPlotLineFlags_Shaded, internal.performance.current_index);
				ImPlot::EndPlot();
			}

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
		ImGui::End();

		glm::vec3 forward = glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f) * glm::mat3(glm::eulerAngleXY(glm::radians(-active_camera.rotation.x), glm::radians(-active_camera.rotation.y))));

		auto view = glm::lookAtRH(active_camera.position, active_camera.position + forward, glm::vec3(0.0f, 1.0f, 0.0f));

		if(internal.selected_instance_idx != -1)
		{
			i32 transform_idx = internal.selected_instance_idx;

			MeshInstanceHeader& instance = World::get_mesh_device_data(transform_idx);
			glm::mat4 transform = instance.transform;

			glm::mat4 projection = glm::perspectiveRH(glm::radians(90.0f), (f32)internal.render_width_px / (f32)internal.render_height_px, 0.1f, 1000.0f);

			if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), (ImGuizmo::OPERATION)(internal.current_gizmo_operation), ImGuizmo::LOCAL, glm::value_ptr(transform)))
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

	bool Raytracer::ui_is_visible()
	{
		return internal.show_debug_ui;
	}
}