#pragma once

enum class MaterialType : u32
{
	Diffuse,
	Metal,
	Dielectric
};

struct Material
{
	glm::vec4 albedo { 1.0f, 1.0f, 1.0f, 0.0f };
	float ior { 1.33f };
	float absorbtion_coefficient { 0.0f };
	MaterialType type { MaterialType::Diffuse };
	float specularity { 0.0f };
};