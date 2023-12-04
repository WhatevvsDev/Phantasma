#include "stdafx.h"
#include "Camera.h"

float camera_speed_t_to_m_per_second(f32 camera_t)
{
	f32 adjusted_t = glm::fclamp(camera_t, 0.0f, 1.0f) * 2.0f - 1.0f;
	f32 value = pow(adjusted_t * 9.95f, 2.0f) * sgn(adjusted_t);

	if(camera_t > 0.5f)
	{
		return glm::clamp(value + 1.0f, 0.01f, 100.0f);
	}
	else
	{
		return glm::clamp(1.0f / fabsf(value - 1.0f), 0.01f, 100.0f);
	}
}

bool orbit_camera_behavior(f32 delta_time_ms, Camera::Instance& camera)
{
	bool state_changed = false;

	if(camera.orbit_automatically)
	{
		camera.orbit_camera_t += (delta_time_ms / 1000.0f) * camera.orbit_camera_rotations_per_second;
		camera.orbit_camera_t = wrap_number(camera.orbit_camera_t, 0.0f, 1.0f); 

		bool camera_is_orbiting = (camera.orbit_camera_rotations_per_second != 0.0f);

		state_changed |= camera_is_orbiting;
	}

	glm::vec3 position_offset = glm::vec3(0.0f, 0.0f, camera.orbit_camera_distance);

	camera.rotation = glm::vec3(camera.orbit_camera_angle, camera.orbit_camera_t * 360.0f, 0.0f);
	position_offset = position_offset * glm::mat3(glm::eulerAngleXY(glm::radians(-camera.rotation.x), glm::radians(-camera.rotation.y)));

	camera.position = camera.orbit_camera_position + position_offset;

	return state_changed;
}

bool freecam_camera_behavior(f32 delta_time_ms, Camera::Instance& camera)
{
	bool state_changed = false;

	i32 move_hor =	(ImGui::IsKeyDown(ImGuiKey_D))		- (ImGui::IsKeyDown(ImGuiKey_A));
	i32 move_ver =	(ImGui::IsKeyDown(ImGuiKey_Space))	- (ImGui::IsKeyDown(ImGuiKey_LeftCtrl));
	i32 move_ward =	(ImGui::IsKeyDown(ImGuiKey_W))		- (ImGui::IsKeyDown(ImGuiKey_S));
				
	glm::vec3 move_dir = glm::vec3(move_hor, move_ver, -move_ward);

	move_dir = glm::normalize(move_dir * glm::mat3(glm::eulerAngleXY(glm::radians(-camera.rotation.x), glm::radians(-camera.rotation.y))));

	bool camera_is_moving = (glm::dot(move_dir, move_dir) > 0.5f);

	if(camera_is_moving)
	{
		glm::vec3 camera_move_delta = glm::vec3(move_dir * delta_time_ms * 0.01f) * camera_speed_t_to_m_per_second(camera.camera_speed_t);
			
		state_changed |= true;
				
		camera.position += camera_move_delta;
	}

	ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
	glm::vec3 camera_delta = glm::vec3(-mouse_delta.y, -mouse_delta.x, 0);

	bool pan = ImGui::GetIO().MouseDown[2];

	bool camera_updating = (glm::dot(camera_delta, camera_delta) != 0.0f);

	if(pan && camera_updating)
	{
		glm::vec3 pan_vector = glm::vec3(camera_delta.y, -camera_delta.x, 0.0f) * 0.001f;

		pan_vector = pan_vector * glm::mat3(glm::eulerAngleXY(glm::radians(-camera.rotation.x), glm::radians(-camera.rotation.y)));
		
		camera.position += pan_vector;

		state_changed |= true;
	}
	else if(camera_updating)
	{
		camera.rotation += camera_delta * 0.1f;

		bool pitch_exceeds_limit = (fabs(camera.rotation.x) > 89.9f);

		if(pitch_exceeds_limit)
			camera.rotation.x = 89.9f * sgn(camera.rotation.x);

		state_changed |= true;
	}

	return state_changed;
}

bool Camera::update_instance(f32 delta_time_ms, Camera::Instance& instance)
{
	switch (instance.movement_type)
	{
	case MovementType::Orbit:
		return orbit_camera_behavior(delta_time_ms, instance);
	case MovementType::Freecam:
		return freecam_camera_behavior(delta_time_ms, instance);
	}

	return false;
}
