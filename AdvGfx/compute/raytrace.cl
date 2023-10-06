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

struct SceneData
{
	uint resolution_x;
	uint resolution_y;
	uint mouse_x;
	uint mouse_y;
	float cam_pos_x, cam_pos_y, cam_pos_z;
	uint tri_count;
	float3 cam_forward;
	float3 cam_right;
	float3 cam_up;
	float object_inverse_transform[16];
	int frame_number;
	bool reset_accumulator;
};

#define EPSILON 0.00001f

void intersect_tri(struct Ray* ray, struct Tri* tris, uint triIdx)
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

void intersect_bvh( struct Ray* ray, uint nodeIdx, struct BVHNode* nodes, struct Tri* tris, uint* trisIdx)
{
	// Keeping track of current node in the stack
	struct BVHNode* node = &nodes[0];
	struct BVHNode* traversal_stack[32];
	uint stack_ptr = 0; 

	if (intersect_aabb( ray, node ) == 1e30f)
		return;

	while(1)
	{
		ray->bvh_hits++;

		if(node->tri_count > 0)
		{
			for (uint i = 0; i < node->tri_count; i++ )
				intersect_tri( ray, tris, trisIdx[node->left_first + i]);

			if(stack_ptr == 0)
				break;

			node = traversal_stack[--stack_ptr];
		}
		else
		{
			struct BVHNode* left_child = &nodes[node->left_first];
			struct BVHNode* right_child = &nodes[node->left_first + 1];
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
}

float3 sky_color(struct Ray* ray, float3* sun_dir)
{
	float3 sky_color = (float3)(0.333f, 0.61f, 0.84f);
	float3 horizon_color = (float3)(0.9f, 0.9f, 0.92f);

	float rd = (ray->D.y + 1.0f) * 0.5f;
	float horizont = smoothstep(0.3f, 0.75f, 1.0f - rd);
	
	float3 sky = lerp(sky_color, horizon_color, horizont);

	float sunp = min(dot(ray->D, -*sun_dir), 0.0f);
	float sun = smoothstep(0.99f, 1.0f, sunp * sunp * sunp * sunp);
	float sun_intensity = 1.0f;
	return lerp(sky, (float3)(sun_intensity), sun);
}

#define DEPTH 32

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

float3 trace(struct Ray* primary_ray, uint nodeIdx, struct BVHNode* nodes, float4* normals, struct Tri* tris, uint* trisIdx, uint depth, float* inverse_transform, uint* rand_seed)
{
	float3 sun_dir = normalize((float3)(1.0f, 0.8f, -0.7f));

	// Keeping track of current ray in the stack
	const int ray_stack_size = 32;
	struct Ray ray_stack[ray_stack_size];
	int ray_stack_idx = 1;

	ray_stack[0] = *primary_ray;

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

		// Do raytracing bits
		current_ray.D_reciprocal = 1.0f / current_ray.D;
		intersect_bvh(&current_ray, 0, nodes, tris, trisIdx);

		if(current_ray.depth == DEPTH)
		{
			primary_ray->bvh_hits = current_ray.bvh_hits;			
		}
		
		bool hit_anything = current_ray.t < 1e30;

		if(!hit_anything)
		{
			color += current_ray.light * sky_color(&current_ray, &sun_dir);
			continue;
		}
		else
		{
			float3 hit_pos = current_ray.O + (current_ray.D * current_ray.t);
			float3 normal = interpolate_tri_normal(normals, &current_ray);
			bool inner_normal = (dot(normal, current_ray.D) > 0.5f);

			if(inner_normal)
				normal = -normal;

			float3 hemisphere_normal = random_unit_vector(rand_seed);
			if(dot(hemisphere_normal, normal) < 0.0f)
				hemisphere_normal = -hemisphere_normal;

			struct Ray new_ray;
			new_ray.O = hit_pos + hemisphere_normal * EPSILON;
			new_ray.t = 1e30f;
			new_ray.depth = current_ray.depth - 1;
			new_ray.ray_parent = ray_stack_idx;

			bool is_mirror = false;
			bool is_dielectric = true;

			float3 albedo = (float3)(1.0f, 0.9f, 1.0f);
			float ior = 1.88f;
			float absorbtion_coefficient = 0.9f;

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
					float random = RandomFloat(rand_seed);
					
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

void kernel raytrace(global float* accumulation_buffer, global uint* buffer, global int* mouse, global float4* normals, global struct Tri* tris, global struct BVHNode* nodes, global uint* trisIdx, global struct SceneData* sceneData)
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

	float3 color = trace(&ray, 0, nodes, normals, tris, trisIdx, DEPTH, sceneData->object_inverse_transform, &rand_seed);

	bool is_mouse_ray = (x == sceneData->mouse_x) && (y == sceneData->mouse_y);
	bool ray_hit_anything = ray.t < 1e30f;

	if(is_mouse_ray)
	{
		*mouse = ray_hit_anything
		 ? ray.intersection.tri_hit
		 : -1;
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