 struct Tri 
{ 
    float3 vertex0;
	float3 vertex1;
	float3 vertex2;
};

struct RayIntersection
{
	// Barycentrics, we can reconstruct w
	float u;
	float v;
	int tri_hit;
	int header_tri_count;
};

struct Ray 
{ 
    float3 O;
    float3 D; 
    float t;
	float3 light;
	int depth;
	float3 D_reciprocal;
	uint bvh_hits;
	uint ray_parent;
	struct RayIntersection intersection;
};

struct BVHNode
{
    float minx, miny, minz;
    int left_first;
    float maxx, maxy, maxz;
	int tri_count;
};

struct MeshHeader
{
	uint tris_offset;
	uint tris_count;

	uint normals_offset;
	uint normals_count; // Is in theory always 3x tris_count;

	uint root_bvh_node_idx;
	uint bvh_node_count; // Technically could be unnecessary

	uint tri_idx_offset;
	uint tri_idx_count;
};

struct MeshInstanceHeader
{
	float transform[16];
	float inverse_transform[16];
		
	uint mesh_idx;
};

struct WorldManagerDeviceData
{
	uint mesh_count;
	struct MeshInstanceHeader instances[256];
};

#define PI 3.14159265359f

float3 get_exr_color(float3 direction, float* exr, uint exr_width, uint exr_height)
{
	float theta = acos(direction.y);
	float phi = atan2(direction.z, direction.x) + PI;

	float u = phi / (2 * PI);
	float v = theta / PI;

	int i = (int)(u * exr_width);
	int j = (int)(v * exr_height);

	int idx = (i + j * exr_width) * 4;
	
	float3 color = (float3)(exr[idx + 0],exr[idx + 1],exr[idx + 2]);
	float intensity = exr[idx + 3];
	
	return color * intensity;
}

#define EPSILON 0.00001f

void intersect_tri(struct Ray* ray, struct Tri* tris, uint triIdx, struct MeshHeader* header)
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
	}
}

float intersect_aabb( struct Ray* ray, struct BVHNode* node )
{
	float tx1 = (node->minx - ray->O.x) * ray->D_reciprocal.x, tx2 = (node->maxx - ray->O.x)  * ray->D_reciprocal.x;
	float tmin = min( tx1, tx2 ), tmax = max( tx1, tx2 );
	float ty1 = (node->miny - ray->O.y)  * ray->D_reciprocal.y, ty2 = (node->maxy - ray->O.y)  * ray->D_reciprocal.y;
	tmin = max( tmin, min( ty1, ty2 ) ), tmax = min( tmax, max( ty1, ty2 ) );
	float tz1 = (node->minz - ray->O.z)  * ray->D_reciprocal.z, tz2 = (node->maxz - ray->O.z)  * ray->D_reciprocal.z;
	tmin = max( tmin, min( tz1, tz2 ) ), tmax = min( tmax, max( tz1, tz2 ) );
	if (tmax >= tmin && tmin < ray->t && tmax > 0) return tmin; else return 1e30f;
}

void intersect_bvh( struct Ray* ray, uint nodeIdx, struct BVHNode* nodes, struct Tri* tris, uint* trisIdx, struct MeshHeader* mesh_header, float* inverse_transform)
{
	// Keeping track of current node in the stack
	uint node_offset = mesh_header->root_bvh_node_idx;
	struct BVHNode* node = &nodes[nodeIdx];
	struct BVHNode* traversal_stack[32];
	uint stack_ptr = 0; 

	float3 org_dir = ray->D;
	float3 org_pos = ray->O;
	ray->D = transform((float4)(ray->D, 0), inverse_transform).xyz;
	ray->O = transform((float4)(ray->O, 1), inverse_transform).xyz;
	ray->D_reciprocal = 1.0f / ray->D;

	if (intersect_aabb( ray, node ) == 1e30f)
	{
		ray->D = org_dir;
		ray->O = org_pos;
		return;
	}

	while(1)
	{
		ray->bvh_hits++;

		if(node->tri_count > 0)
		{
			for (uint i = 0; i < node->tri_count; i++ )
				intersect_tri( ray, &tris[mesh_header->tris_offset], trisIdx[node->left_first + i + mesh_header->tri_idx_offset], mesh_header);

			if(stack_ptr == 0)
				break;

			node = traversal_stack[--stack_ptr];
		}
		else
		{
			struct BVHNode* left_child = &nodes[node->left_first + node_offset];
			struct BVHNode* right_child = &nodes[node->left_first + node_offset + 1];
			float left_dist = intersect_aabb(ray, left_child);
			float right_dist = intersect_aabb(ray, right_child);

			if(left_dist > right_dist)
			{
				// Swap around dist and node
				float d = left_dist; left_dist = right_dist; right_dist = d;
				
				struct BVHNode* n = left_child; left_child = right_child; right_child = n;
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

	ray->D = org_dir;
	ray->O = org_pos;
}

#define DEPTH 8

float3 tri_normal(struct Tri* tri)
{
	float3 a = (*tri).vertex0;
	float3 b = (*tri).vertex1;
	float3 c = (*tri).vertex2;

	return normalize(cross(a - b, a - c));
}

float3 random_unit_vector( uint* rand_seed)
{
	float3 result = (float3)(RandomFloat(rand_seed), RandomFloat(rand_seed), RandomFloat(rand_seed));

	result *= 2;
	result -= 1;

	int failsafe = 100;
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

	return normalize(result);
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

struct TraceArgs
{
	struct Ray* primary_ray;
	struct BVHNode* nodes;
	float4* normals;
	struct Tri* tris;
	uint* trisIdx;
	uint* rand_seed;
	struct MeshHeader* mesh_headers;
	uint mesh_idx;
	float* exr;
	uint exr_width;
	uint exr_height;
	struct WorldManagerDeviceData* world_data;
};

float3 trace(struct TraceArgs* args)
{
	float3 sun_dir = normalize((float3)(1.0f, 0.8f, -0.7f));

	// Keeping track of current ray in the stack
	const int ray_stack_size = 32;
	struct Ray ray_stack[ray_stack_size];
	int ray_stack_idx = 1;

	ray_stack[0] = *args->primary_ray;

	float3 color = (float3)(0.0f);

	while(ray_stack_idx > 0)
	{
		// Go through ray stack, return if empty
		ray_stack_idx--;
		if(ray_stack_idx < 0)
			return color;

		struct Ray current_ray = ray_stack[ray_stack_idx];

		bool out_of_scope = (dot(current_ray.light, current_ray.light) < EPSILON || ( current_ray.depth <= 0 ));

		if(out_of_scope)
		{
			continue;
		}


		float oldt = current_ray.t;
		int hit_header_idx = -1;

		// Do raytracing bits
		for(uint i = 0; i < (*args->world_data).mesh_count; i++)
		{
			struct MeshInstanceHeader* instance = &(*args->world_data).instances[i];
			struct MeshHeader* mesh = &args->mesh_headers[instance->mesh_idx];

			intersect_bvh(&current_ray, mesh->root_bvh_node_idx, args->nodes, args->tris, args->trisIdx, &args->mesh_headers[instance->mesh_idx], instance->inverse_transform);

			if(current_ray.t < oldt)
			{
				hit_header_idx = (int)i;
				oldt = current_ray.t;
			}
		}

		if(current_ray.depth == DEPTH)
		{
			args->primary_ray->bvh_hits = current_ray.bvh_hits;			
			args->primary_ray->t = current_ray.t;
			args->primary_ray->intersection.header_tri_count = hit_header_idx;
		}
		
		bool hit_anything = current_ray.t < 1e30;

		if(!hit_anything)
		{
			color += current_ray.light * get_exr_color(current_ray.D, args->exr, args->exr_width, args->exr_height);
			continue;
		}
		else
		{
			struct MeshInstanceHeader* instance = &(*args->world_data).instances[hit_header_idx];
			struct MeshHeader* mesh = &args->mesh_headers[instance->mesh_idx];	

			float3 hit_pos = current_ray.O + (current_ray.D * current_ray.t);
			float3 normal = interpolate_tri_normal(&args->normals[mesh->normals_offset], &current_ray);
			
			// We have to apply transform so normals are world-space
			normal = transform((float4)(normal, 0.0f), instance->transform).xyz;
			normal = normalize(normal);

			bool inner_normal = dot(normal, current_ray.D) > 0.0f;

			if(inner_normal)
				normal = -normal;

			float3 hemisphere_normal = random_unit_vector(args->rand_seed);
			if(dot(hemisphere_normal, normal) < 0.0f)
				hemisphere_normal = -hemisphere_normal;

			struct Ray new_ray;
			new_ray.O = hit_pos + hemisphere_normal * EPSILON;
			new_ray.t = 1e30f;
			new_ray.depth = current_ray.depth - 1;
			new_ray.ray_parent = ray_stack_idx;

			bool is_mirror = false;
			bool is_dielectric = false;

			float3 albedo = (float3)(0.7f, 0.5f, 1.0f);
			float ior = 1.66f;
			float absorbtion_coefficient = 0.1f;

			if(is_mirror)
			{
				float mirror_absorption = 0.1f;

				new_ray.D = reflected(current_ray.D, normal);
				ray_stack[ray_stack_idx++] = new_ray;

				ray_stack[current_ray.ray_parent].light = current_ray.light * (1.0f - mirror_absorption);
			}
			else
			{
				if(is_dielectric)
				{
					float refraction_ratio = (inner_normal ? (1.0f / ior) : ior);// <- Only use with objects that are enclosed

					float reflectance = fresnel(current_ray.D, normal, ior);
					float transmittance = 1.0f - reflectance;
					float random = RandomFloat(args->rand_seed);
					
					if(reflectance > random)
					{
						new_ray.D = reflected(current_ray.D, normal);
						ray_stack[ray_stack_idx++] = new_ray;

						ray_stack[current_ray.ray_parent].light = current_ray.light * albedo;
					}
					else
					{
						new_ray.D = refracted(current_ray.D, normal, refraction_ratio);
						ray_stack[ray_stack_idx++] = new_ray;

						ray_stack[current_ray.ray_parent].light = current_ray.light * albedo;
					
						if(inner_normal)
						{
							ray_stack[current_ray.ray_parent].light *= beers_law(current_ray.t, absorbtion_coefficient);
						}
					}
				}
				else
				{
					new_ray.D = hemisphere_normal;
					ray_stack[ray_stack_idx++] = new_ray;

					float3 brdf = albedo / (float)M_PI;

					ray_stack[current_ray.ray_parent].light = (float)M_PI * 2.0f * brdf * current_ray.light * dot(normal, hemisphere_normal);
				}
			}
		}
	}
	return color;
}

void kernel raytrace(global float* accumulation_buffer, global int* mouse, global float4* normals, global struct Tri* tris, global struct BVHNode* nodes, global uint* trisIdx, global struct MeshHeader* mesh_headers, global struct SceneData* sceneData, global float* exr, global struct WorldManagerDeviceData* world_manager_data)
{     
	int width = sceneData->resolution_x;
	int height = sceneData->resolution_y;

	int x = get_global_id(0);
	int y = get_global_id(1);

	uint pixel_dest = (x + y * width);

	uint rand_seed = WangHash(pixel_dest + sceneData->frame_number * width * height);

	float x_t = ((x / (float)width) - 0.5f) * 2.0f;
	float y_t = ((y / (float)height)- 0.5f) * 2.0f;

	float aspect_ratio = (float)(sceneData->resolution_x) / (float)(sceneData->resolution_y);

	float3 pixelPos = sceneData->cam_forward + sceneData->cam_right * x_t * aspect_ratio - sceneData->cam_up * y_t;

	// Actual raytracing

	struct Ray ray;
	ray.O = (float3)(sceneData->cam_pos_x, sceneData->cam_pos_y, sceneData->cam_pos_z);
    ray.D = normalize( pixelPos );
    ray.t = 1e30f;
	ray.light = 1.0f;
	ray.depth = DEPTH;

	struct TraceArgs trace_args;
	trace_args.primary_ray = &ray;
	trace_args.nodes = nodes;
	trace_args.normals = normals;
	trace_args.tris = tris;
	trace_args.trisIdx = trisIdx;
	trace_args.rand_seed = &rand_seed;
	trace_args.mesh_headers = mesh_headers;
	trace_args.mesh_idx = sceneData->mesh_idx;
	trace_args.exr = exr;
	trace_args.exr_width = sceneData->exr_width;
	trace_args.exr_height = sceneData->exr_height;
	trace_args.world_data = world_manager_data;

	float3 color = trace(&trace_args);

	bool is_mouse_ray = (x == sceneData->mouse_x) && (y == sceneData->mouse_y);
	bool ray_hit_anything = ray.t < 1e30f;

	if(is_mouse_ray)
	{
		*mouse = trace_args.primary_ray->intersection.header_tri_count;

		if(!ray_hit_anything)
			*mouse = -1;
	}

	//color = ray.bvh_hits / 10.0f;
	
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
