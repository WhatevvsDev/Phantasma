// Force a specific SIMD set
// #define GLM_FORCE_SSE2
// #define GLM_FORCE_AVX.

// Disable use of SIMD for GLM
// #define GLM_FORCE_PURE 

// Include all GLM core / GLSL features
#include <glm/glm.hpp> // vec2, vec3, mat4, radians

// Include all GLM extensions
#include <glm/ext.hpp> // perspective, translate, rotate

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