// Constants
#define EULER 2.71828f
#define PI 3.14159265359f
#define EPSILON 0.00001f

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

// Math

float4 sample_exr(float* exr, uint idx)
{
	return (float4)(exr[idx + 0], exr[idx + 1], exr[idx + 2], exr[idx + 3]) * 1.0f;
};

float wrap_float(float val, float min, float max) 
{
    return val + (max - min) * sign((float)(val < min) - (float)(val > max));
}

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

float fresnel_schlick(float f0, float3 view, float3 halfway)
{
	return f0 + ((1.0f - f0) * pow(1.0f - dot(view, halfway), 5.0f));
}

// Blinn
// Beckmann
// GGX <- We use this one
float normal_distribution_ggx(float3 view, float3 halfway, float roughness)
{
	float a = roughness * roughness;

	float n_dot_h = dot(view, halfway);

	float d = ((n_dot_h * a - n_dot_h) * n_dot_h + -1.0f);

	return clamp(a / (PI * d * d), 0.0f, 1.0f);
}

// Beckmann
// GGX
// Schlick-GGX
float geometry_one_term(float3 direction, float3 normal, float roughness)
{
	float k = (roughness * roughness) * 0.5f;

	float n_dot_v = fabs(dot(normal, direction));

	return n_dot_v / ((n_dot_v * (1.0f - k)) + k);
}

float geometry_term(float3 view, float3 normal, float3 light, float roughness)
{
	return geometry_one_term(light, normal, roughness) * geometry_one_term(view, normal, roughness);
}

//      pdf = D * NdotH / (4 * HdotV)
/*
float3 get_ggx_microfacet(uint* rand_seed, float roughness)
{
	// Get our uniform random numbers
	float2 randVal = (float2)(RandomFloat(rand_seed), RandomFloat(rand_seed));

	// GGX NDF sampling
	float a2 = roughness * roughness;
	float cosThetaH = sqrt(max(0.0f, (1.0f-randVal.x)/((a2-1.0f)*randVal.x+1)));
	float sinThetaH = sqrt(max(0.0f, 1.0f - cosThetaH * cosThetaH));
	float phiH = randVal.y * M_PI * 2.0f;

	// Get our GGX NDF sample (i.e., the half vector)
	return (float3)((sinThetaH * cos(phiH)), (sinThetaH * sin(phiH)), cosThetaH);
}*/

float3 get_ggx_microfacet(uint* randSeed, float roughness, float3 hitNorm)
{
	float2 randVal = (float2)(RandomFloat(randSeed), RandomFloat(randSeed));

	// Get an orthonormal basis from the normal
	float3 B = cross(hitNorm, (fabs(hitNorm.y) == 1.0f) ? (float3)(0.0f, 0.0f, 1.0f) : (float3)(0.0f, sign(hitNorm.y), 0.0f));
	float3 T = cross(B, hitNorm);

	// GGX NDF sampling
	float a2 = roughness * roughness;
	float cosThetaH = sqrt(max(0.0f, (1.0f -randVal.x)/((a2-1.0f )*randVal.x+1) ));
	float sinThetaH = sqrt(max(0.0f, 1.0f - cosThetaH * cosThetaH));
	float phiH = randVal.y * M_PI * 2.0f;

	// Get our GGX NDF sample (i.e., the half vector)
	return T * (sinThetaH * cos(phiH)) +
           B * (sinThetaH * sin(phiH)) +
           hitNorm * cosThetaH;
}

float2 sample_uniform_disk(uint* rand_seed)
{
	float azimuth = RandomFloat(rand_seed) * PI * 2;
	float len = sqrt(RandomFloat(rand_seed));

	float x = cos(azimuth) * len;
	float z = sin(azimuth) * len;

	return (float2)(x, z);
}

float3 random_unit_vector(uint* rand_seed, float3 normal)
{
	float3 result = (float3)(RandomFloat(rand_seed), RandomFloat(rand_seed), RandomFloat(rand_seed));

	result *= 2;
	result -= 1;

	int failsafe = 8;
	while(failsafe--)
	{
		if(dot(result,result) < 1.0f)
		{
			return normalize(result);
		}
		result = (float3)(RandomFloat(rand_seed), RandomFloat(rand_seed), RandomFloat(rand_seed));
		result *= 2;
		result -= 1;
	}

	return normal;
}

float saturate(float value)
{
	return clamp(value, 0.0f, 1.0f);
}

float float3_color_to_uint(float3 color)
{
	int r = saturate(color.r) * 255.0f;
	int g = saturate(color.g) * 255.0f;
	int b = saturate(color.b) * 255.0f;

	return 0x00010000 * b + 0x00000100 * g + 0x00000001 * r;
}

float3 uint_color_to_float3(uint color)
{
	float r = ((color & 0x00ff0000) >> 16) / 255.0f;
	float g = ((color & 0x0000ff00) >> 8)  / 255.0f;
	float b = ((color & 0x000000ff) >> 0)  / 255.0f;

	return (float3)(r, g, b);
}

bool is_within_bounds(int x, int y, int width, int height)
{
	return (x >= 0 && y >= 0 && x < width && y < height);
}

// Structs

#ifndef VIEW_TYPE_ENUM_DEFINED

#define VIEW_TYPE_ENUM_DEFINED

typedef enum ViewType
{
	Render = 0,
	Albedo = 1,
	Normal = 2,
	BLAS   = 3,
	TLAS   = 4,
	AS     = 5
} ViewType;

#endif

#ifndef SCENE_DATA_STRUCT_DEFINED

#define SCENE_DATA_STRUCT_DEFINED

typedef struct SceneData
{
	uint2 resolution;
	uint2 mouse_pos;
	int2 exr_size;
	int accumulated_frames;
	bool reset_accumulator;
	float camera_transform[16];
	float old_camera_transform[16];
	float old_proj_camera_transform[16];
	float inv_old_camera_transform[16];
	float exr_angle;
	uint material_idx;
	uint2 pad;
} SceneData;

#endif

#ifndef PER_PIXEL_DATA_STRUCT_DEFINED

#define PER_PIXEL_DATA_STRUCT_DEFINED

typedef struct PerPixelData
{
	uint hit_object;
	uint blas_hits;
	uint tlas_hits;
	uint pad;
	float4 albedo;
	float4 normal;
	float4 hit_position;
} PerPixelData;

#endif

#ifndef PER_VERTEX_DATA_STRUCT_DEFINED

#define PER_VERTEX_DATA_STRUCT_DEFINED

typedef struct  VertexData
{
	float3 normal;
	float2 uv;
} VertexData;

#endif

#ifndef TRI_DEFINED

#define TRI_DEFINED

typedef struct Tri 
{ 
    float3 vertex0;
	float3 vertex1;
	float3 vertex2;
} Tri;

#endif

#ifndef RAY_INTERSECTION_DEFINED

#define RAY_INTERSECTION_DEFINED

typedef struct RayIntersection
{
	// Barycentrics, we can reconstruct w
	float u;
	float v;
	int tri_hit;
	float3 geo_normal;
} RayIntersection;

#endif

#ifndef RAY_DEFINED

#define RAY_DEFINED

typedef struct __attribute__ ((packed)) Ray
{ 
    float3 O;
    float t;
    float3 D;
	RayIntersection* intersection;
} Ray;

#endif

#ifndef BVH_NODE_DEFINED

#define BVH_NODE_DEFINED

typedef struct BVHNode
{
    float minx, miny, minz;
    int left_first;
    float maxx, maxy, maxz;
	int primitive_count;
} BVHNode;

#endif

#ifndef MESH_HEADER_DEFINED

#define MESH_HEADER_DEFINED

typedef struct MeshHeader
{
	uint tris_offset;
	uint tris_count;

	uint vertex_data_offset;
	uint vertex_data_count; // Is in theory always 3x tris_count;

	uint root_bvh_node_idx;
	uint bvh_node_count; // Technically could be unnecessary

	uint tri_idx_offset;
	uint tri_idx_count;
} MeshHeader;

#endif

#ifndef MESH_INSTANCE_HEADER_DEFINED

#define MESH_INSTANCE_HEADER_DEFINED

typedef struct MeshInstanceHeader
{
	float transform[16];
	float inverse_transform[16];
		
	uint mesh_idx;
	uint material_idx;
	uint texture_idx;
	uint pad;
} MeshInstanceHeader;

#endif

#ifndef WORLD_MANAGER_DEVICE_DATA_DEFINED

#define WORLD_MANAGER_DEVICE_DATA_DEFINED

typedef struct WorldManagerDeviceData
{
	uint mesh_count;
	uint pad_0[3];
	MeshInstanceHeader instances[4096];
} WorldManagerDeviceData;

#endif

#ifndef EXTEND_PER_PIXEL_OUTPUT_DEFINED

#define EXTEND_PER_PIXEL_OUTPUT_DEFINED

typedef struct ExtendPerPixelOutput
{
	int hit_mesh_header_idx;
	Ray old_ray;
	RayIntersection intersection;
} ExtendPerPixelOutput;

#endif
