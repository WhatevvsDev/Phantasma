#define COSINE_WEIGHTED_BIAS 1
#define STRATIFIED_4x4 1
#define TLAS 1

#define DEPTH 16

typedef struct Tri 
{ 
    float3 vertex0;
	float3 vertex1;
	float3 vertex2;
} Tri;

typedef struct RayIntersection
{
	// Barycentrics, we can reconstruct w
	float u;
	float v;
	int tri_hit;
	int header_tri_count;
	float3 geo_normal;
} RayIntersection;

typedef struct Ray
{ 
    float3 O;
    float3 D; 
    float t;
	int depth;
	RayIntersection intersection;
} Ray;

typedef struct BVHNode
{
    float minx, miny, minz;
    int left_first;
    float maxx, maxy, maxz;
	int tri_count;
} BVHNode;

typedef struct MeshHeader
{
	uint tris_offset;
	uint tris_count;

	uint normals_offset;
	uint normals_count; // Is in theory always 3x tris_count;

	uint root_bvh_node_idx;
	uint bvh_node_count; // Technically could be unnecessary

	uint tri_idx_offset;
	uint tri_idx_count;
} MeshHeader;

typedef struct MeshInstanceHeader
{
	float transform[16];
	float inverse_transform[16];
		
	uint mesh_idx;
	uint material_idx;
} MeshInstanceHeader;

typedef struct WorldManagerDeviceData
{
	uint mesh_count;
	MeshInstanceHeader instances[256];
} WorldManagerDeviceData;

float3 get_exr_color(float3 direction, float* exr, uint exr_width, uint exr_height, float exr_angle, uint max_luma_idx, bool include_sun)
{	
	float theta = acos(direction.y);
	float phi = atan2(direction.z, direction.x) + PI;

	float v = theta / PI;
	float u = phi / (2 * PI);
	
	u = wrap_float(u + exr_angle / 360.0f, 0.0f, 1.0f);

	uint i = floor(u * exr_width);
	uint j = floor(v * exr_height);

	i = clamp(i, 0u, exr_width - 1u);
	j = clamp(j, 0u, exr_height - 1u);

	uint idx = (i + j * exr_width);

	float4 color = sample_exr(exr, idx * 4u);

	return color.xyz * color.w;
}

void intersect_tri(Ray* ray, Tri* tris, uint triIdx, MeshHeader* header)
{
	const float3 edge1 = tris[triIdx].vertex1 - tris[triIdx].vertex0;
	const float3 edge2 = tris[triIdx].vertex2 - tris[triIdx].vertex0;
	const float3 h = cross( ray->D, edge2 );
	const float a = dot( edge1, h );
	if (fabs(a) < EPSILON) return; // ray parallel to triangle
	const float f = 1 / a;
	const float3 s = ray->O - tris[triIdx].vertex0;
	const float u = f * dot( s, h );
	if (u < 0 || u > 1) return;
	const float3 q = cross( s, edge1 );
	const float v = f * dot( ray->D, q );
	if (v < 0 || u + v > 1) return;
	const float t = f * dot( edge2, q );
	if (t > 0.0001f) 
	if(ray->t > t)
	{
		ray->t = t;
		ray->intersection.tri_hit = triIdx;
		ray->intersection.u = u;
		ray->intersection.v = v;
		ray->intersection.header_tri_count = header[0].tris_count;
		ray->intersection.geo_normal = cross(edge1, edge2);
	}
}

float intersect_aabb(Ray* ray, BVHNode* node )
{
	float tx1 = (node->minx - ray->O.x) / ray->D.x, tx2 = (node->maxx - ray->O.x) / ray->D.x;
	float tmin = min( tx1, tx2 ), tmax = max( tx1, tx2 );
	float ty1 = (node->miny - ray->O.y) / ray->D.y, ty2 = (node->maxy - ray->O.y) / ray->D.y;
	tmin = max( tmin, min( ty1, ty2 ) ), tmax = min( tmax, max( ty1, ty2 ) );
	float tz1 = (node->minz - ray->O.z) / ray->D.z, tz2 = (node->maxz - ray->O.z) / ray->D.z;
	tmin = max( tmin, min( tz1, tz2 ) ), tmax = min( tmax, max( tz1, tz2 ) );
	if (tmax >= tmin && tmin < ray->t && tmax > 0) 
		return tmin; 
	else 
		return 1e30f;
}

typedef struct BVHArgs
{
	Ray* ray;

	BVHNode* blas_nodes;
	BVHNode* tlas_nodes;

	WorldManagerDeviceData* world_data;
	MeshHeader* mesh_headers;

	Tri* tris;
	uint* trisIdx;

	MeshHeader* mesh_header;
	float* inverse_transform;

	uint* tlas_idx;

	uint blas_hits;
	uint tlas_hits;

} BVHArgs;

void intersect_bvh(BVHArgs* args)
{
	// Keeping track of current node in the stack
	uint node_offset = args->mesh_header->root_bvh_node_idx;
	BVHNode* node = &args->blas_nodes[node_offset];
	BVHNode* traversal_stack[32];
	uint stack_ptr = 0; 

	float3 org_dir = args->ray->D;
	float3 org_pos = args->ray->O;
	args->ray->D = transform((float4)(args->ray->D, 0), args->inverse_transform).xyz;
	args->ray->O = transform((float4)(args->ray->O, 1), args->inverse_transform).xyz;

	if (intersect_aabb( args->ray, node ) == 1e30f)
	{
		args->ray->D = org_dir;
		args->ray->O = org_pos;
		return;
	}

	while(1)
	{
		args->blas_hits++;

		if(node->tri_count > 0)
		{
			for (uint i = 0; i < node->tri_count; i++ )
				intersect_tri( args->ray, &args->tris[args->mesh_header->tris_offset], args->trisIdx[node->left_first + i + args->mesh_header->tri_idx_offset], args->mesh_header);

			if(stack_ptr == 0)
				break;

			node = traversal_stack[--stack_ptr];
		}
		else
		{
			BVHNode* left_child = &args->blas_nodes[node->left_first + node_offset];
			BVHNode* right_child = &args->blas_nodes[node->left_first + node_offset + 1];
			float left_dist = intersect_aabb(args->ray, left_child);
			float right_dist = intersect_aabb(args->ray, right_child);

			if(left_dist > right_dist)
			{
				// Swap around dist and node
				float d = left_dist; left_dist = right_dist; right_dist = d;
				
				BVHNode* n = left_child; left_child = right_child; right_child = n;
			}

			if(left_dist == 1e30f)
			{
				if(stack_ptr == 0)
					break;

				node = traversal_stack[--stack_ptr];
			}
			else
			{
				node = left_child;

            	if (right_dist != 1e30f) 
					traversal_stack[stack_ptr++] = right_child;
			}
		}
	}

	args->ray->D = org_dir;
	args->ray->O = org_pos;
}

float3 tri_normal(Tri* tri)
{
	float3 a = (*tri).vertex0;
	float3 b = (*tri).vertex1;
	float3 c = (*tri).vertex2;

	return normalize(cross(a - b, a - c));
}

float beers_law(float thickness, float absorbtion_coefficient)
{
	return pow(EULER, -absorbtion_coefficient * thickness);
}

float3 interpolate_tri_normal(float4* normals, struct Ray* ray)
{
	float u = ray->intersection.u;
	float v = ray->intersection.v;
	float w = 1.0f - u - v;

	float3 v0normal = normals[ray->intersection.tri_hit * 3 + 0].xyz;
	float3 v1normal = normals[ray->intersection.tri_hit * 3 + 1].xyz;
	float3 v2normal = normals[ray->intersection.tri_hit * 3 + 2].xyz;

	return normalize(v0normal * w + v1normal * u + v2normal * v);
}

typedef enum MaterialType
{
	Diffuse 	= 0,
	Metal		= 1,
	Dielectric	= 2
} MaterialType;

typedef struct Material
{
	float4 albedo;
	float ior;
	float absorbtion_coefficient;
	MaterialType type;
	float specularity;
} Material;

typedef struct TraceArgs
{
	Ray* primary_ray;
	BVHNode* blas_nodes;
	float4* normals;
	Tri* tris;
	uint* trisIdx;
	uint* rand_seed;
	MeshHeader* mesh_headers;
	uint mesh_idx;
	float* exr;
	uint exr_width;
	uint exr_height;
	float exr_angle;
	WorldManagerDeviceData* world_data;
	Material* materials;
	uint material_idx;
	float focal_distance;
	float blur_radius;
	uint max_luma_idx;
	BVHNode* tlas_nodes;
	uint* tlas_idx;
} TraceArgs;

float4 malleys_method(uint* rand_seed)
{
	float2 disk_pos = sample_uniform_disk(rand_seed);

	float x = disk_pos.x;
	float z = disk_pos.y;

	float powx = x * x;
	float powz = z * z;

	float y = sqrt(max(1.0f - powx - powz, 0.0f));

	return (float4)(normalize((float3)(x, y, z)), y);
}

float3 uniform_hemisphere_tangent(uint* rand_seed)
{
	float3 hemisphere_normal = random_unit_vector(rand_seed, (float3)(0.0f, 1.0f, 0.0f));

	if(dot(hemisphere_normal, (float3)(float3)(0.0f, 1.0f, 0.0f)) < 0.0f)
			hemisphere_normal = -hemisphere_normal;

	return hemisphere_normal;
}

float3 tangent_to_base_vector(float3 tangent_dir, float3 normal)
{
	normal = normalize(normal);

	if(fabs(normal.y) == 1.0f)
		return tangent_dir * sign(normal.y);

	float3 bitangent = normalize(cross(normal, (float3)(0.0f, 1.0f, 0.0f)));
	float3 tangent = normalize(cross(normal, bitangent));

	float3 new_tang = tangent_dir.x * tangent + tangent_dir.y * normal + tangent_dir.z * bitangent;

	return normalize(new_tang);
}

int intersect_tlas(BVHArgs* args)
{
	// Keeping track of current node in the stack
	BVHNode* node = &args->tlas_nodes[0];
	BVHNode* traversal_stack[32];
	uint stack_ptr = 0; 

	int hit = -1;

	if (intersect_aabb( args->ray, node ) == 1e30f)
	{
		return hit;
	}

	float dist = 1e30f;

	while(1)
	{
		args->tlas_hits++;

		if(node->tri_count > 0)
		{
			for (uint i = 0; i < node->tri_count; i++ )
			{
				MeshInstanceHeader* instance = &args->world_data->instances[node->left_first + i];

				args->mesh_header = &args->mesh_headers[instance->mesh_idx];
				args->inverse_transform = instance->inverse_transform;

				intersect_bvh(args);

				if(args->ray->t < dist)
				{
					hit = node->left_first + i;
					dist = args->ray->t;
				}
			}
			
			if(stack_ptr == 0)
				break;

			node = traversal_stack[--stack_ptr];
		}
		else
		{
			BVHNode* left_child = &args->tlas_nodes[node->left_first];
			BVHNode* right_child = &args->tlas_nodes[node->left_first + 1];
			float left_dist = intersect_aabb(args->ray, left_child);
			float right_dist = intersect_aabb(args->ray, right_child);

			if(left_dist > right_dist)
			{
				// Swap around dist and node
				float d = left_dist; left_dist = right_dist; right_dist = d;
				
				BVHNode* n = left_child; left_child = right_child; right_child = n;
			}

			if(left_dist == 1e30f)
			{
				if(stack_ptr == 0)
					break;

				node = traversal_stack[--stack_ptr];
			}
			else
			{
				node = left_child;

            	if (right_dist != 1e30f) 
					traversal_stack[stack_ptr++] = right_child;
			}
		}
	}

	return hit;
}

float3 trace(TraceArgs* args)
{
	// Keeping track of current ray in the stack
	Ray current_ray = *args->primary_ray;

	float3 color = (float3)(0.0f);

	BVHArgs bvh_args;
	bvh_args.blas_nodes = args->blas_nodes;
	bvh_args.tlas_nodes = args->tlas_nodes;
	bvh_args.tris = args->tris;
	bvh_args.trisIdx = args->trisIdx;
	bvh_args.tlas_idx = args->tlas_idx;
	bvh_args.mesh_headers = args->mesh_headers;
	bvh_args.world_data = args->world_data;
	bvh_args.blas_hits = 0;
	bvh_args.tlas_hits = 0;

	float3 e = 0;
	float3 t = 1;

	while(current_ray.depth > 0)
	{
		// Go through ray stack, return if empty
		bool out_of_scope = (dot(t, t) < EPSILON);

		if(out_of_scope)
			break;

		float oldt = current_ray.t;
		int hit_header_idx = -1;

		bvh_args.ray = &current_ray;

		if(args->world_data->mesh_count > 0)
		{
			hit_header_idx = intersect_tlas(&bvh_args);
		}

		bool is_primary_ray = (current_ray.depth == DEPTH);

		if(is_primary_ray)
		{
			args->primary_ray->t = current_ray.t;
			args->primary_ray->intersection.header_tri_count = hit_header_idx;
		}
		
		bool hit_anything = current_ray.t < 1e30;

		if(!hit_anything)
		{
			float3 exr_color = get_exr_color(current_ray.D, args->exr, args->exr_width, args->exr_height, args->exr_angle, args->max_luma_idx, is_primary_ray);

			// Hacky way to get rid of fireflies
			exr_color = clamp(exr_color, 0.0f , 32.0f);

			e += t * exr_color;
			break;
		}
		else
		{
			MeshInstanceHeader* instance = &(*args->world_data).instances[hit_header_idx];
			MeshHeader* mesh = &args->mesh_headers[instance->mesh_idx];	
			Material mat = args->materials[instance->material_idx];

			float3 hit_pos = current_ray.O + (current_ray.D * current_ray.t);
			float3 normal = interpolate_tri_normal(&args->normals[mesh->normals_offset], &current_ray);
			float3 geo_normal = current_ray.intersection.geo_normal;

			// We have to apply transform so normals are world-space

			float local_inverse_transform_ver[16];
			copy4x4(instance->inverse_transform, &local_inverse_transform_ver);
			transpose4x4(&local_inverse_transform_ver);

			normal = transform((float4)(normal, 0.0f), &local_inverse_transform_ver).xyz;
			geo_normal = transform((float4)(geo_normal, 0.0f), &local_inverse_transform_ver).xyz;
			normal = normalize(normal);
			geo_normal = normalize(geo_normal);

			bool inner_normal = dot(geo_normal, current_ray.D) > 0.0f;

#if(COSINE_WEIGHTED_BIAS == 0)
			float3 hemisphere_normal = malleys_method(args->rand_seed).xyz;
#else
			float3 hemisphere_normal = uniform_hemisphere_tangent(args->rand_seed).xyz;
#endif

			hemisphere_normal = tangent_to_base_vector(hemisphere_normal, normal);

			if(inner_normal)
				normal = -normal;

			float old_ray_distance = current_ray.t;
			current_ray.O = hit_pos + hemisphere_normal * EPSILON;
			current_ray.t = 1e30f;
			current_ray.depth--;

			float3 reflected_dir = reflected(current_ray.D, normal);
			float random = RandomFloat(args->rand_seed);

			float3 material_color = mat.albedo.xyz * (mat.albedo.a + 1.0f);

			// <Russian roulette>
			float die_chance = max(max((float)material_color.x, (float)material_color.y), (float)material_color.z);
			die_chance = clamp(die_chance, 0.0f, 1.0f);

			if(RandomFloat(args->rand_seed) > die_chance)
				break;
				
			material_color /= die_chance;
			// </Russian roulette>

			switch(mat.type)
			{
				case Diffuse:
				{
					current_ray.D = (mat.specularity < random)
						 ? hemisphere_normal
						 : reflected_dir;
					
					float3 brdf = material_color / PI;

					float3 diffuse =  brdf * 2.0f * dot(normal, current_ray.D);
					float3 specular = material_color;

					float3 final_color = lerp(diffuse, specular, mat.specularity) * (1.0f - mat.absorbtion_coefficient);

					e += t * final_color;
					t *= dot(normal, current_ray.D) * brdf * 2.0f;

					continue;
				}
				case Metal:
				{
					float3 reflected_dir = reflected(current_ray.D, normal) * (1.0f + EPSILON);
					
					current_ray.D = normalize(reflected_dir + random_unit_vector(args->rand_seed, normal) * (1.0f - mat.specularity));
					continue;
				}
				case Dielectric:
				{
					float refraction_ratio = (inner_normal ? (1.0f / mat.ior) : mat.ior);// <- Only use with objects that are enclosed

					float reflectance = fresnel(current_ray.D, normal, refraction_ratio);
					float transmittance = 1.0f - reflectance;
				
					if(reflectance > random)
					{
						current_ray.D = reflected(current_ray.D, normal);
						e *= t * material_color;
						t *= material_color;

					}
					else
					{
						current_ray.D = refracted(current_ray.D, normal, refraction_ratio);
						float transmission_factor = inner_normal ? beers_law(old_ray_distance, mat.absorbtion_coefficient) : 1.0f;

						e *= material_color * transmission_factor * t;
						t *= material_color * transmission_factor;
					}
					continue;
				}
			}

			
		}
	}
	return e;
}

float3 to_float3(float* array)
{
	return (float3)(array[0], array[1], array[2]);
}

void kernel raytrace(global float* accumulation_buffer, global int* mouse, global float* distance, global float4* normals, global struct Tri* tris, global struct BVHNode* blas_nodes, global uint* trisIdx, global struct MeshHeader* mesh_headers, global struct SceneData* sceneData, global float* exr, global struct WorldManagerDeviceData* world_manager_data, global struct Material* materials, global BVHNode* tlas_nodes)
{     
	int width = sceneData->resolution_x;
	int height = sceneData->resolution_y;

	int x = get_global_id(0);
	int y = get_global_id(1);

	uint pixel_dest = (x + y * width);

	uint rand_seed = WangHash(pixel_dest + sceneData->frame_number * width * height);

	uint strata_idx = sceneData->frame_number % (16);
	uint strata_x_idx = strata_idx % 4;
	uint strata_y_idx = strata_idx / 4;

	float strata_u = (RandomFloat(&rand_seed) * 0.25f) + (0.25f * (float)strata_x_idx);
	float strata_v = (RandomFloat(&rand_seed) * 0.25f) + (0.25f * (float)strata_y_idx);

	float x_t;
	float y_t;

#if(STRATIFIED_4x4 == 0)
	x_t = ((((float)x + RandomFloat(&rand_seed)) / (float)width) - 0.5f) * 2.0f;
	y_t = ((((float)y + RandomFloat(&rand_seed)) / (float)height)- 0.5f) * 2.0f;
#else
	x_t = ((((float)x + strata_u) / (float)width) - 0.5f) * 2.0f;
	y_t = ((((float)y + strata_v) / (float)height) - 0.5f) * 2.0f;
#endif

	float aspect_ratio = (float)(sceneData->resolution_x) / (float)(sceneData->resolution_y);

	float3 pixel_dir_tangent = 
	(float3)(0.0f, 0.0f, -1.0f) + 
	(float3)(1.0f, 0.0f, 0.0f) * x_t * aspect_ratio -
	(float3)(0.0f, 1.0f, 0.0f) * y_t;

	float3 disk_pos_tangent = (float3)(sample_uniform_disk(&rand_seed) * sceneData->blur_radius, 0.0f);

	pixel_dir_tangent -= disk_pos_tangent / sceneData->focal_distance;
	float3 pixel_dir_world = transform((float4)(pixel_dir_tangent, 0.0f), sceneData->camera_transform).xyz;;
	float3 disk_pos_world = transform((float4)(disk_pos_tangent, 0.0f), sceneData->camera_transform).xyz;
	
	// TODO: this is kinda dumb innit?
	float3 cam_pos = transform((float4)(0.0f, 0.0f, 0.0f, 1.0f), sceneData->camera_transform).xyz;

	float3 ray_dir = (pixel_dir_world);

	// Actual raytracing
	struct Ray ray;
	ray.O = cam_pos + disk_pos_world;
    ray.D = normalize(ray_dir);
    ray.t = 1e30f;
	ray.depth = DEPTH;

	struct TraceArgs trace_args;
	trace_args.primary_ray = &ray;
	trace_args.blas_nodes = blas_nodes;
	trace_args.normals = normals;
	trace_args.tris = tris;
	trace_args.trisIdx = trisIdx;
	trace_args.rand_seed = &rand_seed;
	trace_args.mesh_headers = mesh_headers;
	trace_args.exr = exr;
	trace_args.exr_width = sceneData->exr_width;
	trace_args.exr_height = sceneData->exr_height;
	trace_args.exr_angle = sceneData->exr_angle;
	trace_args.world_data = world_manager_data;
	trace_args.material_idx = sceneData->material_idx;
	trace_args.materials = materials;
	trace_args.max_luma_idx = sceneData->max_luma_idx;
	trace_args.tlas_nodes = tlas_nodes;

	float3 color = trace(&trace_args);

	bool is_mouse_ray = (x == sceneData->mouse_x) && (y == sceneData->mouse_y);
	bool ray_hit_anything = ray.t < 1e30f;

	if(is_mouse_ray)
	{
		*mouse = trace_args.primary_ray->intersection.header_tri_count;
		*distance = trace_args.primary_ray->t;

		if(!ray_hit_anything)
		{
			*mouse = -1;
			*distance = -1.0f;
		}
	}

	if(sceneData->reset_accumulator)
	{
		accumulation_buffer[pixel_dest * 4 + 0] = color.x;
		accumulation_buffer[pixel_dest * 4 + 1] = color.y;
		accumulation_buffer[pixel_dest * 4 + 2] = color.z;
	}
	else
	{
		accumulation_buffer[pixel_dest * 4 + 0] += color.x;
		accumulation_buffer[pixel_dest * 4 + 1] += color.y;
		accumulation_buffer[pixel_dest * 4 + 2] += color.z;
	}
}

