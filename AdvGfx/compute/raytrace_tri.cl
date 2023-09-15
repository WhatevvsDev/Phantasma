float3 lerp(float3 a, float3 b, float t)
{
    return a + t*(b-a);
}

float3 reflected(float3 normal, float3 direction)
{
	float projected = dot(-normal, direction);
	return (direction + normal * projected * 2);
}

struct Tri 
{ 
    float3 vertex0;
	float3 vertex1;
	float3 vertex2; 
    float3 center;
};

struct Ray 
{ 
    float3 O;
    float3 D; 
    float t;
	int bvh_hits;
	int tri_hit;
};

struct BVHNode
{
    float minx, miny, minz;
    int left_first;
    float maxx, maxy, maxz;
	int tri_count;
};

void IntersectTri( struct Ray* ray, struct Tri* tris, uint triIdx)
{
	const float3 edge1 = tris[triIdx].vertex1 - tris[triIdx].vertex0;
	const float3 edge2 = tris[triIdx].vertex2 - tris[triIdx].vertex0;
	const float3 h = cross( ray->D, edge2 );
	const float a = dot( edge1, h );
	if (a > -0.0001f && a < 0.0001f) return; // ray parallel to triangle
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

float IntersectAABB( struct Ray* ray, struct BVHNode* node )
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

	if (IntersectAABB( ray, node ) == 1e30f)
		return;

	while(1)
	{
		if(node->tri_count > 0)
		{
			for (uint i = 0; i < node->tri_count; i++ )
				IntersectTri( ray, tris, trisIdx[node->left_first + i]);

			if(stack_ptr == 0)
				break;

			node = traversal_stack[--stack_ptr];
		}
		else
		{
			ray->bvh_hits++;
			// Optimization potential: move these variables outside the while loop?
			// Optimization potential: we create children next to each other, maybe delete right_child?
			struct BVHNode* left_child = &nodes[node->left_first];
			struct BVHNode* right_child = &nodes[node->left_first + 1];
			float left_dist = IntersectAABB(ray, left_child);
			float right_dist = IntersectAABB(ray, right_child);

			if(left_dist > right_dist)
			{
				// Swap around dist and node
				float d = left_dist; left_dist = right_dist; right_dist = d;
				
				// Optimization potential, we might not need current node anymore, so this is might be extra?
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

float3 Sky(struct Ray* ray, float3* sun_dir)
{
	float3 sky_1 = (float3)(0.60f, 0.76f, 1.0f);
	float3 sky_2 = (float3)(0.1f, 0.12f, 0.2f);

	float3 sky = lerp(sky_2, sky_1, (ray->D.y + 1.0f) * 0.5f);

	float sunp = min(dot(ray->D, -*sun_dir), 0.0f);
	float sun = smoothstep(0.99f, 1.0f, sunp * sunp * sunp * sunp);

	return lerp(sky, (float3)(1.0f, 1.0f, 1.0f), sun);
}

float3 trace(struct Ray* ray, uint nodeIdx, struct BVHNode* nodes, struct Tri* tris, uint* trisIdx)
{
	float3 sun_dir = normalize((float3)(0.0f, 1.0f, 0.7f));

	intersect_bvh(ray, 0, nodes, tris, trisIdx);
	
	bool hit_anything = ray->t < 1e30;

	if(hit_anything)
	{
		float3 hit_pos = ray->O + (ray->D * ray->t) * 0.999999f;
		
		struct Ray shadow_ray;
		shadow_ray.O = hit_pos;
		shadow_ray.D = sun_dir;
		shadow_ray.t = 1e30f;
		shadow_ray.bvh_hits = 0;

		intersect_bvh(&shadow_ray, 0, nodes, tris, trisIdx);

		bool shadow = shadow_ray.t < 1e30;
		float3 normal = cross(tris[ray->tri_hit].vertex0 - tris[ray->tri_hit].vertex1,tris[ray->tri_hit].vertex0 - tris[ray->tri_hit].vertex2);
		normal = normalize(normal);

		float3 diffuse = (dot(normal, sun_dir) + 1.0f) * 0.5f * (1.0f - shadow);

		return diffuse;
	}
	else // Sky
	{
		return Sky(ray, &sun_dir);
	}
}



struct SceneData
{
	uint resolution_x;
	uint resolution_y;
	uint tri_count;
	uint dummy;
	float4 cam_pos;
	float4 cam_forward;
	float4 cam_right;
	float4 cam_up;
};

void kernel raytrace(global uint* buffer, global struct Tri* tris, global struct BVHNode* nodes, global uint* trisIdx, global struct SceneData* sceneData)
{     
	float3 cpos = (float3)(sceneData->cam_pos.x, sceneData->cam_pos.y, sceneData->cam_pos.z);

	float3 cright = (float3)(sceneData->cam_right.x, sceneData->cam_right.y, sceneData->cam_right.z);	
	float3 cforward = (float3)(sceneData->cam_forward.x, sceneData->cam_forward.y, sceneData->cam_forward.z);
	float3 cup = (float3)(sceneData->cam_up.x, sceneData->cam_up.y, sceneData->cam_up.z);
	
	int width = sceneData->resolution_x;
	int height = sceneData->resolution_y;

	int x = get_global_id(0);
	int y = height - get_global_id(1);
	uint pixel_dest = (x + y * width);

	float x_t = ((x / (float)width) - 0.5f) * 2.0f;
	float y_t = ((y / (float)height)- 0.5f) * 2.0f;

	float aspect_ratio = (float)(sceneData->resolution_x) / (float)(sceneData->resolution_y);

	float3 pixelPos = cforward + cright * x_t * aspect_ratio + cup * y_t;

	// Actual raytracing

	struct Ray ray;
	ray.O = cpos;
    ray.D = normalize( pixelPos );
    ray.t = 1e30f;
	ray.bvh_hits = 0;

	float3 color = trace(&ray, 0, nodes, tris, trisIdx);

	int r = clamp((int)(color.x * 255.0f), 0, 255);
	int g = clamp((int)(color.y * 255.0f), 0, 255);
	int b = clamp((int)(color.z * 255.0f), 0, 255);

	buffer[pixel_dest] = 0x00010000 * b + 0x00000100 * g + 0x00000001 * r;
}