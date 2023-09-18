// Force a specific SIMD set
// #define GLM_FORCE_SSE2
// #define GLM_FORCE_AVX.

// Disable use of SIMD for GLM
// #define GLM_FORCE_PURE 

// Include all GLM core / GLSL features
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp> // vec2, vec3, mat4, radians

// Include all GLM extensions
#include <glm/ext.hpp> // perspective, translate, rotate
#include <glm/gtc/matrix_transform.hpp>

// Taken from https://github.com/jbikker/tmpl8rt_IGAD
typedef unsigned int uint;

// RNG - Marsaglia's xor32
uint WangHash( uint s );
uint InitSeed( uint seedBase );
uint RandomUInt();
float RandomFloat();
float Rand( float range );
uint RandomUInt( uint& customSeed );
float RandomFloat( uint& customSeed );

#ifndef SGN_TEMPLATED_FUNC
// taken from https://stackoverflow.com/questions/1903954/is-there-a-standard-sign-function-signum-sgn-in-c-c
template <typename T> 
int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}
#define SGN_TEMPLATED_FUNC
#endif

#include "json.hpp"
using json = nlohmann::json;

namespace glm
{
    void to_json(json& j, const vec3& v);
    void from_json(const json& j, vec3& v);
}