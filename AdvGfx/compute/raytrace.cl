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

struct Tri 
{ 
    float3 vertex0;
	float3 vertex1;
	float3 vertex2;
};

struct Ray 
{ 
    float3 O;
    float3 D; 
    float t;
	int tri_hit;
	float3 light;
	int depth;
	float3 D_reciprocal;
	uint bvh_hits;
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

#define EPSILON 0.000001f

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
		ray->tri_hit = triIdx;
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
			ray->bvh_hits++;
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
	return lerp(sky, (float3)(sun_intensity), sun) * 2.0f;
}

#define DEPTH 32
#define EULER 2.71828f

float3 tri_normal(struct Tri* tri)
{
	float3 a = (*tri).vertex0;
	float3 b = (*tri).vertex1;
	float3 c = (*tri).vertex2;

	return normalize(cross(a - b, a - c));
}

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

float3 trace(struct Ray* primary_ray, uint nodeIdx, struct BVHNode* nodes, struct Tri* tris, uint* trisIdx, uint depth, float* inverse_transform, uint* rand_seed)
{
	float3 sun_dir = normalize((float3)(1.0f, 0.8f, -0.7f));

	// Keeping track of current ray in the stack
	const int ray_stack_size = 32;
	struct Ray ray_stack[ray_stack_size];
	int ray_stack_idx = 1;

	ray_stack[0] = *primary_ray;

	float3 color = (float3)(1.0f);
	int ray_count = 0;

	while(ray_stack_idx > 0)
	{
		// Go through ray stack, return if empty
		ray_stack_idx--;
		if(ray_stack_idx < 0)
			return color;

		struct Ray current_ray = ray_stack[ray_stack_idx];

		bool out_of_scope = (dot(current_ray.light, current_ray.light) < EPSILON || ( current_ray.depth <= 0 ));

		if(out_of_scope)
			continue;

		// Do raytracing bits
		current_ray.D_reciprocal = 1.0f / current_ray.D;
		intersect_bvh(&current_ray, 0, nodes, tris, trisIdx);
		
		bool hit_anything = current_ray.t < 1e30;

		if(!hit_anything)
		{
		 	color *= sky_color(&current_ray, &sun_dir) * current_ray.light;
			continue;
		}
		else
		{
			float3 hit_pos = current_ray.O + (current_ray.D * current_ray.t - EPSILON);
			float3 normal = tri_normal(&tris[current_ray.tri_hit]);
			bool inner_normal = (dot(normal, current_ray.D) < 0.0f);

			if(inner_normal)
			normal = -normal;

			float3 hemisphere_normal = random_unit_vector(rand_seed);
			if(dot(hemisphere_normal, normal) < 0.0f)
			{
				hemisphere_normal = -hemisphere_normal;
			}

			struct Ray new_ray;
			new_ray.O = hit_pos;
			new_ray.D = hemisphere_normal;
			new_ray.t = 1e30f;
			new_ray.tri_hit = 0;
			new_ray.light = current_ray.light;
			new_ray.depth = current_ray.depth - 1;
			ray_count++;

			ray_stack[ray_stack_idx++] = new_ray;
			
			float3 object_color;

			if(current_ray.tri_hit > 2)
			{
				object_color = (float3)(1.0f, 0.0f, 1.0f);
			}
			else
			{
				object_color = (float3)(1.0f, 1.0f, 1.0f);
			}

			color *= object_color * current_ray.light * 0.5f;
		}
	}
	return color;// light;// / (float)ray_count;
}

void kernel raytrace(global float* accumulation_buffer, global uint* buffer, global int* mouse, global struct Tri* tris, global struct BVHNode* nodes, global uint* trisIdx, global struct SceneData* sceneData)
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
	ray.tri_hit = 0;
	ray.light = 1.0f;
	ray.depth = DEPTH;

	float3 color = trace(&ray, 0, nodes, tris, trisIdx, DEPTH, sceneData->object_inverse_transform, &rand_seed);

	bool is_mouse_ray = (x == sceneData->mouse_x) && (y == sceneData->mouse_y);
	bool ray_hit_anything = ray.t < 1e30f;

	if(is_mouse_ray)
	{
		*mouse = ray_hit_anything
		 ? ray.tri_hit
		 : -1;
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