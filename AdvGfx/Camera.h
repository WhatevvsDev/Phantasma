#pragma once

namespace Camera
{
	enum class MovementType
	{
		Freecam,
		Orbit
	};

	struct Instance
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
		Camera::MovementType movement_type { Camera::MovementType::Freecam };
		f32 camera_speed_t { 0.5f };
		bool orbit_automatically				{ true };

		// Blur
		f32 focal_distance { 1.0f };
		f32 blur_radius { 0.0f };

		// Needed to save/load vector of this
		NLOHMANN_DEFINE_TYPE_INTRUSIVE(Camera::Instance, 
			orbit_camera_position, 
			orbit_camera_distance, 
			orbit_camera_angle,
			orbit_camera_rotations_per_second,
			orbit_camera_t,
			camera_speed_t,
			position,
			rotation,
			movement_type,
			orbit_automatically,
			focal_distance,
			blur_radius);
	};

	// Returns whether camera transform or state has changed
	bool update_instance(f32 delta_time_ms, Camera::Instance& instance);

	glm::mat4 get_instance_matrix(Camera::Instance& instance);
}