#include "Math.h"
#include <cstdint>

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