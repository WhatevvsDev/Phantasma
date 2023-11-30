#pragma once
#include "Math.h"

struct MeshInstanceHeader
{
	glm::mat4 transform {glm::identity<glm::mat4>()};
	glm::mat4 inverse_transform {glm::identity<glm::mat4>()};
		
	u32 mesh_idx { 0 };
	u32 material_idx { 0 };

	// Needed to save/load vector of this
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(MeshInstanceHeader, 
		transform, 
		inverse_transform, 
		mesh_idx,
		material_idx);
};

enum class CameraMovementType
{
	Freecam,
	Orbit
};

struct CameraInstance
{
	// Orbit
	glm::vec3 orbit_camera_position			{ 0.0f };
	f32 orbit_camera_distance				{ 10.0f };
	f32 orbit_camera_angle					{ 0.0f };
	f32 orbit_camera_rotations_per_second	{ 0.1f };
	f32 orbit_camera_t						{ 0.0f };
	f32 pad_0[1];
	
	// Transform
	glm::vec3 position						{ 0.0f };
	f32 pad_1[1];
	glm::vec3 rotation						{ 0.0f };
	f32 pad_2[1];

	// Movement
	CameraMovementType camera_movement_type { CameraMovementType::Freecam };
	bool orbit_automatically				{ true };

	// Blur
	f32 focal_distance { 1.0f };
	f32 blur_radius { 0.0f };

	// Needed to save/load vector of this
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(CameraInstance, 
		orbit_camera_position, 
		orbit_camera_distance, 
		orbit_camera_angle,
		orbit_camera_rotations_per_second,
		orbit_camera_t,
		position,
		rotation,
		camera_movement_type,
		orbit_automatically,
		focal_distance,
		blur_radius);
};

struct WorldManagerDeviceData
{
	u32 mesh_instance_count;
	u32 pad_0[3];
	MeshInstanceHeader mesh_instances[4096]; // Size of array pointed at should be 4096
};

namespace WorldManager
{
	// Returns index of object
	int add_instance_of_mesh(u32 mesh_idx);
	void remove_mesh_instance(i32 instance_idx);
	MeshInstanceHeader& get_mesh_device_data(usize instance_idx);

	WorldManagerDeviceData& get_world_device_data();

	void serialize_scene();
	void deserialize_scene();
}