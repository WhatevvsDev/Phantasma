#include "Math.h"
// RNG - Marsaglia's xor32
static uint seed = 0x12345678;
uint WangHash( uint s )
{
	s = (s ^ 61) ^ (s >> 16);
	s *= 9, s = s ^ (s >> 4);
	s *= 0x27d4eb2d;
	s = s ^ (s >> 15);
	return s;
}
uint InitSeed( uint seedBase )
{
	return WangHash( (seedBase + 1) * 17 );
}
uint RandomUInt()
{
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return seed;
}
float RandomFloat() { return RandomUInt() * 2.3283064365387e-10f; }
float Rand( float range ) { return RandomFloat() * range; }
// local seed
uint RandomUInt( uint& customSeed )
{
	customSeed ^= customSeed << 13;
	customSeed ^= customSeed >> 17;
	customSeed ^= customSeed << 5;
	return customSeed;
}
float RandomFloat( uint& customSeed ) { return RandomUInt( customSeed ) * 2.3283064365387e-10f; }

namespace glm
{
	void to_json(json& j, const vec3& v)
	{
		j["x"] = v.x;
		j["y"] = v.y;
		j["z"] = v.z;
	}

	void from_json(const json& j, vec3& v)
	{
		j.at("x").get_to(v.x);
		j.at("y").get_to(v.y);
		j.at("z").get_to(v.z);
	}
	
	void to_json(json& j, const vec4& v)
	{
		j["x"] = v.x;
		j["y"] = v.y;
		j["z"] = v.z;
		j["w"] = v.w;
	}

	void from_json(const json& j, vec4& v)
	{
		j.at("x").get_to(v.x);
		j.at("y").get_to(v.y);
		j.at("z").get_to(v.z);
		j.at("w").get_to(v.w);
	}

	void glm::to_json(json& j, const mat4& v)
	{
		f32 matrix_values[16];
		memcpy(matrix_values, glm::value_ptr(v), sizeof(f32) * 16);

		j["mat4"] = matrix_values;
	}

	void from_json(const json& j, mat4& v)
	{
		f32 matrix_values[16];

		j.at("mat4").get_to(matrix_values);

		memcpy(glm::value_ptr(v), matrix_values, sizeof(f32) * 16);
	}
}