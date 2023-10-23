// Constants
#define EULER 2.71828f

// Math
float3 lerp(float3 a, float3 b, float t)
{
    return a + t*(b-a);
}

float3 reflected(float3 direction, float3 normal)
{
	return direction - 2 * dot(direction, normal) * normal;
}

float4 transform(float4 vector, float* transform)
{
	float4 result;

	result.x = 	transform[0] * vector.x + 
				transform[4] * vector.y + 
				transform[8] * vector.z +
				transform[12] * vector.w;
	result.y = 	transform[1] * vector.x + 
	 			transform[5] * vector.y + 
	 			transform[9] * vector.z +
	 			transform[13] * vector.w;
	result.z = 	transform[2] * vector.x + 
	 			transform[6] * vector.y + 
	 			transform[10] * vector.z +
	 			transform[14] * vector.w;
	result.w = 	transform[3] * vector.x + 
	 			transform[7] * vector.y + 
	 			transform[11] * vector.z +
	 			transform[15] * vector.w;

	return result;
}

void transpose4x4(float* matrix)
{
	float temp = 0.0f;

	temp = matrix[1], matrix[1] = matrix[4], matrix[4] = temp;
	temp = matrix[2], matrix[2] = matrix[8], matrix[8] = temp;
	temp = matrix[6], matrix[6] = matrix[9], matrix[9] = temp;
	temp = matrix[3], matrix[3] = matrix[12], matrix[12] = temp;
	temp = matrix[7], matrix[7] = matrix[13], matrix[13] = temp;
	temp = matrix[11], matrix[11] = matrix[14], matrix[14] = temp;
}

void copy4x4(float* src, float* dst)
{
	for(int i = 0; i < 16; i++)
		dst[i] = src[i];
}

// Taken from https://www.scratchapixel.com/lessons/3d-basic-rendering/introduction-to-shading/reflection-refraction-fresnel.html
float3 refracted(float3 in, float3 n, float ior) 
{
	float cosi = clamp(dot(in, n), -1.0f, 1.0f);
    float etai = 1, etat = ior;

    if (cosi < 0) 
	{ 
		cosi = -cosi; 
	} else 
	{ 
		float swap = etai; etai = etat; etat = swap;
		n = -n; 
		}
    float eta = etai / etat;
    float k = 1 - eta * eta * (1 - cosi * cosi);
    return k < 0 ? 0 : eta * in + (eta * cosi - sqrt(k)) * n;
}

// Taken from https://www.scratchapixel.com/lessons/3d-basic-rendering/introduction-to-shading/reflection-refraction-fresnel.html
// returns reflection
float fresnel(float3 ray_dir, float3 normal, float ior)
{
	float reflection;
    float cosi = clamp(dot(ray_dir, normal), -1.0f, 1.0f);
    float etai = 1;
	float etat = ior;
    if (cosi > 0) 
	{ 
		float swap = etai; etai = etat; etat = swap;
		normal = -normal;
	}
    // Compute sini using Snell's law
    float sint = etai / etat * sqrt(max(0.0f, 1.0f - cosi * cosi));
    // Total internal reflection
    if (sint >= 1) 
	{
        reflection = 1.0f;
    }
    else 
	{
        float cost = sqrt(max(0.0f, 1.0f - sint * sint));
        cosi = fabs(cosi);
        float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
        float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
        reflection = (Rs * Rs + Rp * Rp) / 2;
    }

	return reflection;
}

// Random numbers

uint WangHash( uint s ) 
{ 
	s = (s ^ 61) ^ (s >> 16);
	s *= 9, s = s ^ (s >> 4);
	s *= 0x27d4eb2d;
	s = s ^ (s >> 15); 
	return s; 
}
uint RandomInt( uint* s ) // Marsaglia's XOR32 RNG
{ 
	*s ^= *s << 13;
	*s ^= *s >> 17;
	* s ^= *s << 5; 
	return *s; 
}
float RandomFloat( uint* s ) 
{ 
	return RandomInt( s ) * 2.3283064365387e-10f; // = 1 / (2^32-1)
}

// Structs

#ifndef SCENE_DATA

#define SCENE_DATA

typedef struct SceneData
{
	uint resolution_x;
	uint resolution_y;
	uint mouse_x;
	uint mouse_y;
	int frame_number;
	uint exr_width;
	uint exr_height;
	bool reset_accumulator;
	float camera_transform[16];
	float exr_angle;
	uint material_idx;
} SceneData;

#endif
