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

#ifndef WRAP_NUMBER_TEMPLATED_FUNC
template <typename T> 
T wrap_number(T val, T min, T max) {
    return val + (max - min) * ((int)(val < min) - (int)(val > max));
}
#define WRAP_NUMBER_TEMPLATED_FUNC
#endif

#include "JSONUtility.h"

namespace glm
{
    void to_json(json& j, const vec3& v);
    void from_json(const json& j, vec3& v);

    void to_json(json& j, const vec4& v);
    void from_json(const json& j, vec4& v);

    void to_json(json& j, const mat4& v);
    void from_json(const json& j, mat4& v);
}