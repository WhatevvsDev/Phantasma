#pragma once
#include "JSONUtility.h"

enum class MaterialType : u32
{
	Diffuse,
	Metal,
	Dielectric,
	CookTorranceBRDF
};

struct Material
{
	glm::vec4 albedo { 1.0f, 1.0f, 1.0f, 0.0f };
	float ior { 1.33f };
	float absorbtion_coefficient { 0.0f };
	MaterialType type { MaterialType::Diffuse };
	float specularity { 0.0f };
	float metallic { 0.0f };
	float roughness { 0.0f };
	float pad[2];

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(Material, 
		albedo, 
		ior, 
		absorbtion_coefficient,
		type,
		specularity,
		metallic,
		roughness);
};