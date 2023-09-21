float3 lerp(float3 a, float3 b, float t)
{
    return a + t*(b-a);
}

float3 reflected(float3 direction, float3 normal)
{
	return direction - 2 * dot(direction, normal) * normal;
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
};

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

	/*
	//float cos_theta = fmin(dot(-in, n), 1.0f);
	float cos_theta = fmin(dot(-in, n), 1.0f);
    float3 r_out_perp =  ior * (in + cos_theta*n);
    float3 r_out_parallel = -sqrt(fabs(1.0f - dot(r_out_perp, r_out_perp))) * n;
    return r_out_perp + r_out_parallel;
	*/
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

struct BVHNode
{
    float minx, miny, minz;
    int left_first;
    float maxx, maxy, maxz;
	int tri_count;
};

void intersect_tri(struct Ray* ray, struct Tri* tris, uint triIdx)
{
	const float3 edge1 = tris[triIdx].vertex1 - tris[triIdx].vertex0;
	const float3 edge2 = tris[triIdx].vertex2 - tris[triIdx].vertex0;
	const float3 h = cross( ray->D, edge2 );
	const float a = dot( edge1, h );
	if (a > -0.000001f && a < 0.000001f) return; // ray parallel to triangle
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
	float tx1 = (node->minx - ray->O.x) / ray->D.x, tx2 = (node->maxx - ray->O.x) / ray->D.x;
	float tmin = min( tx1, tx2 ), tmax = max( tx1, tx2 );
	float ty1 = (node->miny - ray->O.y) / ray->D.y, ty2 = (node->maxy - ray->O.y) / ray->D.y;
	tmin = max( tmin, min( ty1, ty2 ) ), tmax = min( tmax, max( ty1, ty2 ) );
	float tz1 = (node->minz - ray->O.z) / ray->D.z, tz2 = (node->maxz - ray->O.z) / ray->D.z;
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
	return lerp(sky, (float3)(1.0f, 1.0f, 1.0f), sun);
}

#define DEPTH 32

float3 trace(struct Ray* primary_ray, uint nodeIdx, struct BVHNode* nodes, struct Tri* tris, uint* trisIdx, uint depth)
{
	float3 sun_dir = normalize((float3)(0.5f, 1.0f, -0.7f));

	const int ray_stack_size = 32;

	// Keeping track of current ray in the stack
	struct Ray ray_stack[ray_stack_size];
	int ray_stack_idx = 1;

	ray_stack[0] = *primary_ray;

	float3 diffuse = (float3)(0.0f);

	while(ray_stack_idx > 0)
	{
		ray_stack_idx--;
		struct Ray current_ray = ray_stack[ray_stack_idx];
		if(ray_stack_idx < 0)
			return 1;

		if(dot(current_ray.light, current_ray.light) < 0.00001f || 
		( current_ray.depth <= 0 ))
		{
			continue;
		}

		intersect_bvh(&current_ray, 0, nodes, tris, trisIdx);

		//if(depth == DEPTH)
		//	ray->tri_hit = current_ray.tri_hit;

		bool hit_anything = current_ray.t < 1e30;

		if(!hit_anything)
		{
		 	diffuse += sky_color(&current_ray, &sun_dir) * current_ray.light;
			continue;
		}
		
		float3 hit_pos = current_ray.O + (current_ray.D * current_ray.t);
		float3 a = tris[current_ray.tri_hit].vertex0;
		float3 b = tris[current_ray.tri_hit].vertex1;
		float3 c = tris[current_ray.tri_hit].vertex2;

		float3 normal = normalize(cross(a - c, a - b));
		bool inner_normal = (dot(normal, current_ray.D) < 0.0f);
		
		float d = 1.0f;
		float s = 1.0f - d;

		if(false)//(current_ray.tri_hit > 20479)//if(current_ray.tri_hit % 2 == 0)
		{
			struct Ray shadow_ray;
			shadow_ray.O = hit_pos;
			shadow_ray.D = sun_dir;
			shadow_ray.t = 1e30f;
			
			// We force intersect for diffuse ray
			intersect_bvh(&shadow_ray, 0, nodes, tris, trisIdx);
				
			bool shadow = shadow_ray.t < 1e30;
			float shadowt = (clamp((1.0f - shadow), 0.0f, 1.0f));

			diffuse += (float3)(current_ray.light * d * shadowt) * dot(-normal, sun_dir);

			struct Ray specular_ray;
			specular_ray.O = hit_pos;
			specular_ray.D = reflected(current_ray.D, normal);
			specular_ray.t = 1e30f;
			specular_ray.light = current_ray.light * s * shadowt;
			specular_ray.depth = current_ray.depth - 1;

			ray_stack[ray_stack_idx++] = specular_ray;
		}
		else
		{
			float ior = 1.33f; // water is 1.33

			float refraction_ratio = (!inner_normal ? (1.0f / ior) : ior);

			float reflectance = fresnel(current_ray.D, -normal, ior);
			float transmittance = 1.0f - reflectance;
			float3 bias = normal * 0.01f * (inner_normal ? 1.0f : -1.0f);

			{
				struct Ray reflection_ray;
				reflection_ray.O = hit_pos + bias;
				reflection_ray.D = reflected(current_ray.D, normal);
				reflection_ray.t = 1e30f;
				reflection_ray.light = current_ray.light * reflectance;
				reflection_ray.depth = current_ray.depth - 2;

				ray_stack[ray_stack_idx++] = reflection_ray;
			}
			
			//return transmittance;
			//return reflectance;

			if(transmittance > 0.0f)
			{
				struct Ray transmittance_ray;
				transmittance_ray.O = hit_pos - bias;
				transmittance_ray.D = refracted(current_ray.D, normal, refraction_ratio);
				transmittance_ray.t = 1e30f;
				transmittance_ray.light = current_ray.light * transmittance;
				transmittance_ray.depth = current_ray.depth - 2;

				ray_stack[ray_stack_idx++] = transmittance_ray;
			}
		}	
	}
	return diffuse;
}

struct SceneData
{
	uint resolution_x;
	uint resolution_y;
	uint tri_count;
	uint dummy;
	float3 cam_pos;
	float3 cam_forward;
	float3 cam_right;
	float3 cam_up;
};

void kernel raytrace(global uint* buffer, global struct Tri* tris, global struct BVHNode* nodes, global uint* trisIdx, global struct SceneData* sceneData)
{     
	int width = sceneData->resolution_x;
	int height = sceneData->resolution_y;

	int x = get_global_id(0);
	int y = get_global_id(1);
	uint pixel_dest = (x + y * width);

	float x_t = ((x / (float)width) - 0.5f) * 2.0f;
	float y_t = ((y / (float)height)- 0.5f) * 2.0f;

	float aspect_ratio = (float)(sceneData->resolution_x) / (float)(sceneData->resolution_y);

	float3 pixelPos = sceneData->cam_forward + sceneData->cam_right * x_t * aspect_ratio - sceneData->cam_up * y_t;

	// Actual raytracing

	struct Ray ray;
	ray.O = sceneData->cam_pos;
    ray.D = normalize( pixelPos );
    ray.t = 1e30f;
	ray.tri_hit = 0;
	ray.light = 1.0f;
	ray.depth = DEPTH;

	float3 color = trace(&ray, 0, nodes, tris, trisIdx, DEPTH);

	int r = clamp((int)(color.x * 255.0f), 0, 255);
	int g = clamp((int)(color.y * 255.0f), 0, 255);
	int b = clamp((int)(color.z * 255.0f), 0, 255);

	int hit_tri_idx = clamp(ray.tri_hit, 0, 255);

	buffer[pixel_dest] = 0x00010000 * b + 0x00000100 * g + 0x00000001 * r + 0x01000000 * hit_tri_idx;
}