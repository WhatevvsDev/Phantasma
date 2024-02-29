#define DEPTH 16

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

	ExtendOutput* extend_output;

} BVHArgs;

void intersect_tri(Ray* ray, Tri* tris, uint triIdx, MeshHeader* header, ExtendOutput* extend_output)
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
		extend_output->tri_hit = triIdx;
		extend_output->u = u;
		extend_output->v = v;
		extend_output->geo_normal = (float4)(cross(edge1, edge2), 0.0f);
	}
}

float intersect_aabb(Ray* ray, BVHNode* node )
{
	float3 inv_D = 1.0f / ray->D;

	float tx1 = (node->minx - ray->O.x) * inv_D.x, tx2 = (node->maxx - ray->O.x) * inv_D.x;
	float tmin = min( tx1, tx2 ), tmax = max( tx1, tx2 );
	float ty1 = (node->miny - ray->O.y) * inv_D.y, ty2 = (node->maxy - ray->O.y) * inv_D.y;
	tmin = max( tmin, min( ty1, ty2 ) ), tmax = min( tmax, max( ty1, ty2 ) );
	float tz1 = (node->minz - ray->O.z) * inv_D.z, tz2 = (node->maxz - ray->O.z) * inv_D.z;
	tmin = max( tmin, min( tz1, tz2 ) ), tmax = min( tmax, max( tz1, tz2 ) );
	if (tmax >= tmin && tmin < ray->t && tmax > 0) 
		return tmin; 
	else 
		return 1e30f;
}

void intersect_blas(BVHArgs* args)
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

		if(node->primitive_count > 0)
		{
			for (uint i = 0; i < node->primitive_count; i++ )
				intersect_tri( args->ray, &args->tris[args->mesh_header->tris_offset], args->trisIdx[node->left_first + i + args->mesh_header->tri_idx_offset], args->mesh_header, args->extend_output);

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

		if(node->primitive_count > 0)
		{
			for (uint i = 0; i < node->primitive_count; i++ )
			{
				MeshInstanceHeader* instance = &args->world_data->instances[args->tlas_idx[node->left_first + i]];

				args->mesh_header = &args->mesh_headers[instance->mesh_idx];
				args->inverse_transform = instance->inverse_transform;
				
				intersect_blas(args);

				if(args->ray->t < dist)
				{
					hit = args->tlas_idx[node->left_first + i];
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

typedef struct ExtendArgs
{
	BVHNode* blas_nodes;
	Tri* tris;
	uint* trisIdx;
	MeshHeader* mesh_headers;
	WorldManagerDeviceData* world_data;
	BVHNode* tlas_nodes;
	uint* tlas_idx;
	PerPixelData* detail_buffer;
	Ray* ray_buffer;
	Ray* thread_ray;
	ExtendOutput* output;
} ExtendArgs;

float3 extend(ExtendArgs* args)
{
	// Keeping track of current ray in the stack
	Ray current_ray = *args->thread_ray;
	
	uint depth = DEPTH;
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

    int hit_mesh_header_idx = UINT_MAX;

    bvh_args.ray = &current_ray;
	bvh_args.extend_output = args->output;

    if(args->world_data->mesh_count > 0)
    {
        args->output->hit_mesh_header_idx = intersect_tlas(&bvh_args);
    }

    bool is_primary_ray = (depth == DEPTH);
    if(is_primary_ray)
    {
        args->thread_ray->t = current_ray.t;
        args->detail_buffer->tlas_hits = bvh_args.tlas_hits;
        args->detail_buffer->blas_hits = bvh_args.blas_hits;
        args->detail_buffer->hit_object = hit_mesh_header_idx;
        args->detail_buffer->hit_position = (float4)(current_ray.O + current_ray.D * current_ray.t, 0.0f);
        args->detail_buffer->normal = (float4)(-current_ray.D, 0.0f);
        // detail buffer
    }
    
    bool hit_anything = current_ray.t < 1e30;
	*args->thread_ray = current_ray;
    //args->thread_ray->hit_mesh_header_idx = hit_mesh_header_idx;

    if(hit_anything)
    {
        args->detail_buffer->normal = (float4)(current_ray.O + current_ray.D * current_ray.t, 0.0f);
    }
	return 0;
}

void kernel rt_extend(
	global struct Tri* tris, 
	global struct BVHNode* blas_nodes, 
	global uint* trisIdx, 
	global struct MeshHeader* mesh_headers, 
	global struct SceneData* scene_data, 
	global struct WorldManagerDeviceData* world_manager_data, 
	global BVHNode* tlas_nodes,
	global uint* tlas_idx,
	global PerPixelData* detail_buffer,
	global Ray* primary_rays,
	global WavefrontData* wavefront_data,
	global ExtendOutput* extend_output
	)
{     
	uint width = scene_data->resolution.x;
	uint height = scene_data->resolution.y;
	float aspect_ratio = (float)(width) / (float)(height);

	int x = get_global_id(0);
	int y = get_global_id(1);

	uint pixel_index = get_global_id(0);
	
	// Reset value
	extend_output[pixel_index].hit_mesh_header_idx = UINT_MAX;

	// Actual raytracing
	struct ExtendArgs extend_args;
	extend_args.blas_nodes = blas_nodes;
	extend_args.tris = tris;
	extend_args.trisIdx = trisIdx;
	extend_args.mesh_headers = mesh_headers;
	extend_args.world_data = world_manager_data;
	extend_args.tlas_nodes = tlas_nodes;
	extend_args.tlas_idx = tlas_idx;
	extend_args.detail_buffer = &detail_buffer[pixel_index];
	extend_args.ray_buffer = primary_rays;
	extend_args.thread_ray = &primary_rays[pixel_index];
	extend_args.output = &extend_output[pixel_index];

	float3 color = extend(&extend_args);
}