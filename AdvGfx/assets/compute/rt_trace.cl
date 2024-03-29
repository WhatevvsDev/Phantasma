#define DEPTH 16

typedef struct TextureHeader
{
	uint start_offset;
	uint width;
	uint height;
	uint pad;
} TextureHeader;

float3 get_exr_color(float3 direction, float* exr, int2 exr_size, float exr_angle, bool include_sun)
{	
	float theta = acos(direction.y);
	float phi = atan2(direction.z, direction.x) + PI;

	float v = theta / PI;
	float u = phi / (2 * PI);
	
	u = wrap_float(u + exr_angle / 360.0f, 0.0f, 1.0f);

	uint i = floor(u * (exr_size.x - 1u));
	uint j = floor(v * (exr_size.y - 1u));

	uint idx = (i + j * exr_size.x);

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
		ray->tri_hit = triIdx;
		ray->u = u;
		ray->v = v;
		ray->geo_normal = cross(edge1, edge2);
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

		if(node->primitive_count > 0)
		{
			for (uint i = 0; i < node->primitive_count; i++ )
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
	float3 a = tri->vertex0;
	float3 b = tri->vertex1;
	float3 c = tri->vertex2;

	return normalize(cross(a - b, a - c));
}

float beers_law(float thickness, float absorption_coefficient)
{
	return pow(EULER, -absorption_coefficient * thickness);
}

float3 interpolate_tri_normal(VertexData* vertex_data, struct Ray* ray)
{
	float u = ray->u;
	float v = ray->v;
	float w = 1.0f - u - v;

	uint idx = ray->tri_hit * 3;

	float3 v0_normal = vertex_data[idx + 0].normal * w;
	float3 v1_normal = vertex_data[idx + 1].normal * u;
	float3 v2_normal = vertex_data[idx + 2].normal * v;

	return normalize(v0_normal + v1_normal + v2_normal);
}

float2 interpolate_tri_uvs(VertexData* vertex_data, struct Ray* ray)
{
	float u = ray->u;
	float v = ray->v;
	float w = 1.0f - u - v;

	uint idx = ray->tri_hit * 3;

	float2 v0_uv = vertex_data[idx + 0].uv * w;
	float2 v1_uv = vertex_data[idx + 1].uv * u;
	float2 v2_uv = vertex_data[idx + 2].uv * v;

	return (v0_uv + v1_uv + v2_uv);
}

typedef enum MaterialType
{
	Diffuse 	= 0,
	Metal		= 1,
	Dielectric	= 2,
	CookTorranceBRDF = 3
} MaterialType;

typedef struct Material
{
	float4 albedo;
	float ior;
	float absorption_coefficient;
	MaterialType type;
	float specularity;
	float metallic;
	float roughness;
} Material;

typedef struct TraceArgs
{
	Ray* primary_ray;
	BVHNode* blas_nodes;
	VertexData* vertex_data;
	Tri* tris;
	uint* trisIdx;
	uint* rand_seed;
	MeshHeader* mesh_headers;
	TextureHeader* texture_headers;
	float* exr;
	int2 exr_size;
	float exr_angle;
	WorldManagerDeviceData* world_data;
	Material* materials;
	BVHNode* tlas_nodes;
	uint* tlas_idx;
	PerPixelData* detail_buffer;
	unsigned char* textures;
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

		if(node->primitive_count > 0)
		{
			for (uint i = 0; i < node->primitive_count; i++ )
			{
				MeshInstanceHeader* instance = &args->world_data->instances[args->tlas_idx[node->left_first + i]];

				args->mesh_header = &args->mesh_headers[instance->mesh_idx];
				args->inverse_transform = instance->inverse_transform;
				
				intersect_bvh(args);

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

float3 trace(TraceArgs* args)
{
	// Keeping track of current ray
	Ray current_ray = *args->primary_ray;
	
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

	float3 e = 0;
	float3 t = 1;

	while(depth > 0)
	{
		// Go through ray stack, return if empty
		bool out_of_scope = (dot(t, t) < EPSILON);

		if(out_of_scope)
			break;

		float oldt = current_ray.t;
		int hit_mesh_header_idx = UINT_MAX;

		bvh_args.ray = &current_ray;

		if(args->world_data->mesh_count > 0)
		{
			hit_mesh_header_idx = intersect_tlas(&bvh_args);
		}

		bool is_primary_ray = (depth == DEPTH);

		if(is_primary_ray)
		{
			args->primary_ray->t = current_ray.t;
			args->detail_buffer->tlas_hits = bvh_args.tlas_hits;
			args->detail_buffer->blas_hits = bvh_args.blas_hits;
			args->detail_buffer->hit_object = hit_mesh_header_idx;
			args->detail_buffer->hit_position = (float4)(current_ray.O + current_ray.D * current_ray.t, 0.0f);
			args->detail_buffer->normal = (float4)(-current_ray.D, 0.0f);
		}
		
		bool hit_anything = current_ray.t < 1e30;

		if(!hit_anything)
		{
			float3 exr_color = get_exr_color(current_ray.D, args->exr, args->exr_size, args->exr_angle, is_primary_ray);

			// Slighly Biased way to get rid of fireflies
			float sqr_length = dot(exr_color, exr_color);
			float light_limit = 16.0f;
			exr_color = (sqr_length < light_limit ? exr_color : normalize(exr_color) * light_limit);


			if(is_primary_ray)
			{
				args->detail_buffer->albedo = (float4)(exr_color, 0.0f);
			}

			return t * exr_color;
		}
		else
		{
			MeshInstanceHeader* instance = &(*args->world_data).instances[hit_mesh_header_idx];
			MeshHeader* mesh = &args->mesh_headers[instance->mesh_idx];	
			Material mat = args->materials[instance->material_idx];

			VertexData* vertex_data = &args->vertex_data[mesh->vertex_data_offset];

			float3 hit_pos = current_ray.O + (current_ray.D * current_ray.t);
			float3 normal = interpolate_tri_normal(vertex_data, &current_ray);
			float3 geo_normal = current_ray.geo_normal;
			float2 uvs = interpolate_tri_uvs(vertex_data, &current_ray);


			// We have to apply transform so normals are world-space

			float local_inverse_transform_ver[16];
			copy4x4(instance->inverse_transform, &local_inverse_transform_ver);
			transpose4x4(&local_inverse_transform_ver);

			normal = transform((float4)(normal, 0.0f), &local_inverse_transform_ver).xyz;
			geo_normal = transform((float4)(geo_normal, 0.0f), &local_inverse_transform_ver).xyz;
			
			normal = normalize(normal);
			geo_normal = normalize(geo_normal);

			bool inner_normal = dot(geo_normal, current_ray.D) > 0.0f;

			float3 hemisphere_normal = malleys_method(args->rand_seed).xyz;

			if(inner_normal)
				normal = -normal;

			if(is_primary_ray)
			{
				args->detail_buffer->normal = (float4)(normal, 0.0f);
			}

			hemisphere_normal = tangent_to_base_vector(hemisphere_normal, normal);

			float old_ray_distance = current_ray.t;
			current_ray.O = hit_pos + hemisphere_normal * EPSILON;
			current_ray.t = 1e30f;
			depth--;

			float3 reflected_dir = reflected(current_ray.D, normal);
			float random = RandomFloat(args->rand_seed);

			float3 material_color = mat.albedo.xyz * (mat.albedo.a + 1.0f);

			bool is_emissive = mat.albedo.a != 0.0f;

			// <Texture Lookup>
			if(instance->texture_idx != -1)
			{
				TextureHeader header = args->texture_headers[instance->texture_idx];

				const uint channels = 4;

				// Get coordinates, float, uint, fractional
				float texel_xf = uvs.x * header.width;
				float texel_yf = uvs.y * header.height;
				uint texel_x_min = floor(texel_xf);
				uint texel_y_min = floor(texel_yf);
				float hor_fract = texel_xf - texel_x_min;
				float ver_fract = texel_yf - texel_y_min;

				// sample texture at 4 spots
				float3 texels[4];
				uint top_idx = (texel_x_min + texel_y_min * header.width) * channels;
				uint bot_idx = (texel_x_min + (texel_y_min + 1) * header.width) * channels;
				uchar *top_texels = &args->textures[top_idx + header.start_offset];
				uchar *bot_texels = &args->textures[bot_idx + header.start_offset];
				texels[0] = (float3)(top_texels[0], top_texels[1], top_texels[2]);
				texels[1] = (float3)(top_texels[4], top_texels[5], top_texels[6]);
				texels[2] = (float3)(bot_texels[0], bot_texels[1], bot_texels[2]);
				texels[3] = (float3)(bot_texels[4], bot_texels[5], bot_texels[6]);
				
				// interpolate
				float3 bilinear_color = lerp(lerp(texels[0], texels[1], hor_fract), lerp(texels[2], texels[3], hor_fract), ver_fract) / 255.0f;

				material_color = bilinear_color;

				if(is_primary_ray)
				{
					args->detail_buffer->albedo = (float4)(bilinear_color, 0.0f);
				}
			}
			// </Texture Lookup>

			if(is_primary_ray)
			{
				args->detail_buffer->albedo = (float4)(material_color, 0.0f);
			}
			// Add in emission late as to not taint the albedo buffer
			material_color *=  (mat.albedo.a + 1.0f);

			// <Russian roulette>
			{
				float die_chance = max(max((float)material_color.x, (float)material_color.y), (float)material_color.z);
				die_chance = clamp(die_chance, 0.0f, 1.0f);

				if(RandomFloat(args->rand_seed) > die_chance)
					break;
					
				material_color /= die_chance;
			}
			// </Russian roulette>

			switch(mat.type)
			{
				case Diffuse:
				{
					current_ray.D = (mat.specularity < random)
						 ? hemisphere_normal
						 : reflected_dir;
					
					float3 brdf = material_color / PI;

					float3 diffuse = brdf * 2.0f * dot(normal, current_ray.D);
					float3 specular = material_color * (1.0f - mat.specularity);

					float3 final_color = lerp(diffuse, specular, mat.specularity);


					e += t * final_color;
					t *= lerp(final_color, 1.0f, mat.specularity);

					if(is_emissive)
						break;

					continue;
				}
				case Metal:
				{
					float3 reflected_dir = reflected(current_ray.D, normal) * (1.0f + EPSILON);
					
					current_ray.D = normalize(reflected_dir + random_unit_vector(args->rand_seed, normal) * (1.0f - mat.specularity));
					
					e += t * material_color * 0.5f;
					t *= material_color * 0.5f;

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
						float transmission_factor = inner_normal ? beers_law(old_ray_distance, mat.absorption_coefficient) : 1.0f;

						e *= material_color * transmission_factor * t;
						t *= material_color * transmission_factor;
					}
					continue;
				}
				case CookTorranceBRDF:
				{

					bool ray_is_diffuse = RandomFloat(args->rand_seed) > mat.specularity;

					if(ray_is_diffuse)
					{
						current_ray.D = hemisphere_normal;

						float3 brdf = material_color / PI;
						float3 diffuse = brdf * 2.0f * dot(normal, current_ray.D) * (1.0f - mat.specularity);

						e += t * diffuse;
						t *= diffuse;
						continue;
					}
					else // ray_is_specular
					{
						float rough = mat.roughness;

						float3 V = current_ray.D;
						float3 N = normal;

						// Randomly sample the NDF to get a microfacet in our BRDF 
						float3 H = get_ggx_microfacet(args->rand_seed, rough, N);
						//H = tangent_to_base_vector(H, normal);

						// Compute outgoing direction based on this (perfectly reflective) facet
						float3 L = reflected(current_ray.D, normal);

						// Compute our color by tracing a ray in this direction
						float3 bounceColor = mat.albedo.xyz;//shootIndirectRay(hit, L, gMinT, 0, rndSeed, rayDepth);

						// Compute some dot products needed for shading
						float  NdotL = saturate(dot(N, L));
						float  NdotH = saturate(dot(N, H));
						float  LdotH = saturate(fabs(dot(L, H)));
						float  NdotV = saturate(fabs(dot(N, V)));

						// Evaluate our BRDF using a microfacet BRDF model
						float  D = 1.0f - (normal_distribution_ggx(N, H, rough));          
						float  G = fabs(geometry_term(V, N, L, rough)); 
						float3 F = 1.0f;//fresnel_schlick(mat.specularity, V, H);                 
						float3 ggxTerm = D * G * F / (4 * NdotL * NdotV); 

						// What's the probability of sampling vector H from getGGXMicrofacet()?
						float  ggxProb = D * NdotH / (4 * LdotH);

						// Accumulate color:  ggx-BRDF * lightIn * NdotL / probability-of-sampling
						//    -> Note: Should really cancel and simplify the math above
						float3 cccolor = NdotL * bounceColor * ggxTerm / (ggxProb);
					
						cccolor.x = saturate(cccolor.x);
						cccolor.y = saturate(cccolor.y);
						cccolor.z = saturate(cccolor.z);

						cccolor = cccolor * mat.specularity * 0.5f;

						current_ray.D = H;

						e += t * cccolor;
						t *= cccolor;
						continue;
					}
				}
			}
		}
	}
	return 0;
}

float3 to_float3(float* array)
{
	return (float3)(array[0], array[1], array[2]);
}

//returns xy->uv > [-1..1] (with aspect), z < 0 if invalid
float3 world_space_to_screen_space(float3 camera_position,float3 camera_target, float3 position_ws)
{
    float3 to_point = position_ws - camera_position;
    float3 to_point_nrm = normalize(to_point);
    
    float3 camera_dir = normalize(camera_target - camera_position);
	float3 view_right = normalize(cross((float3)(0.0,0.0,1.0),camera_dir));
    float3 view_up = (cross(camera_dir,view_right));
    
    float3 fwd = camera_dir * EPSILON;
    
    float d = dot(camera_dir,to_point_nrm);
    if(d < 0.01)
        return (float3)(0.0,0.0,-1.0);
    
    d = EPSILON / d;
    
    to_point = to_point_nrm * d - fwd;
    
    float x = dot(to_point,view_right);
    float y = dot(to_point,view_up);
    return (float3)(x,y,1.0);
}

void kernel rt_trace(
	global float* accumulation_buffer, 
	global uint* render_buffer, 
	global int* mouse, 
	global float* distance,
	global VertexData* vertex_data, 
	global struct Tri* tris, 
	global struct BVHNode* blas_nodes, 
	global uint* trisIdx, 
	global struct MeshHeader* mesh_headers, 
	global unsigned char* textures,
	global struct MeshHeader* texture_headers, 
	global struct SceneData* scene_data, 
	global float* exr, 
	global struct WorldManagerDeviceData* world_manager_data, 
	global struct Material* materials, 
	global BVHNode* tlas_nodes,
	global uint* tlas_idx,
	global PerPixelData* detail_buffer,
	global Ray* primary_rays
	)
{     
	uint width = scene_data->resolution.x;
	uint height = scene_data->resolution.y;
	float aspect_ratio = (float)(width) / (float)(height);

	int x = get_global_id(0);
	int y = get_global_id(1);

	uint pixel_index = (x + y * width);

	uint rand_seed = WangHash(pixel_index + scene_data->accumulated_frames * width * height);

	// Actual raytracing
	struct Ray ray = primary_rays[pixel_index];

	struct TraceArgs trace_args;
	trace_args.primary_ray = &ray;
	trace_args.blas_nodes = blas_nodes;
	trace_args.vertex_data = vertex_data;
	trace_args.tris = tris;
	trace_args.trisIdx = trisIdx;
	trace_args.rand_seed = &rand_seed;
	trace_args.mesh_headers = mesh_headers;
	trace_args.exr = exr;
	trace_args.exr_size = scene_data->exr_size;
	trace_args.exr_angle = scene_data->exr_angle;
	trace_args.world_data = world_manager_data;
	trace_args.materials = materials;
	trace_args.tlas_nodes = tlas_nodes;
	trace_args.tlas_idx = tlas_idx;
	trace_args.detail_buffer = &detail_buffer[pixel_index];
	trace_args.textures = textures;
	trace_args.texture_headers = texture_headers;

	float3 color = trace(&trace_args);

	bool is_mouse_ray = (x == scene_data->mouse_pos.x) && (y == scene_data->mouse_pos.y);
	bool ray_hit_anything = ray.t < 1e30f;

	if(is_mouse_ray)
	{
		*mouse = trace_args.detail_buffer->hit_object;
		*distance = trace_args.primary_ray->t;

		if(!ray_hit_anything)
		{
			*mouse = -1;
			*distance = -1.0f;
		}
	}

	// <Reprojection>
	{
		// TODO: finish this, stationary UVs don't match up with actual UVs for some reason.
		// Known information
		float2 uv = (float2)((float)x / (float)(width - 1), (float)y / (float)(height - 1));
		float4 pixel_ws_pos = (float4)(detail_buffer[pixel_index].hit_position.xyz, 1.0f);
		
		/*
		// Reproject using old camera matrix
		float4 reprojected = transform((float4)(pixel_ws_pos.xyz, 1.0f), scene_data->inv_old_camera_transform);
		reprojected /= reprojected.w;

		// Convert to screenspace
		float2 reproj_uv = (reprojected.xy / reprojected.z / (float2)(aspect_ratio, 1.0f)) * 0.5f + 0.5f;
		float2 reproj_uv01 = (float2)(1.0f - reproj_uv.x, reproj_uv.y);

		// If sampling offscreen
		bool offscreen = (reproj_uv01.x != clamp(reproj_uv01.x, 0.0f, 1.0f) || (reproj_uv01.y != clamp(reproj_uv01.y, 0.0f, 1.0f)));

		// Get buffer index
		uint rprj_x = reproj_uv01.x * width;
		uint rprj_y = reproj_uv01.y * height;
		uint reproj_pixel_index = (rprj_x + rprj_y * width);


		float4 prevClipPos = transform(pixel_ws_pos, scene_data->inv_old_camera_transform);
		prevClipPos /= prevClipPos.w;

		float2 prevUV = (prevClipPos.xy / prevClipPos.w) + 1.0f;

		//color = prevUV.xyy;

		// For testing
		if(true)
		{
			accumulation_buffer[pixel_index * 4 + 0] = color.x;
			accumulation_buffer[pixel_index * 4 + 1] = color.y;
			accumulation_buffer[pixel_index * 4 + 2] = color.z;
		}
		else
		{
			accumulation_buffer[pixel_index * 4 + 0] = accumulation_buffer[reproj_pixel_index * 4 + 0];
			accumulation_buffer[pixel_index * 4 + 1] = accumulation_buffer[reproj_pixel_index * 4 + 1];
			accumulation_buffer[pixel_index * 4 + 2] = accumulation_buffer[reproj_pixel_index * 4 + 2];
		}
		*/
	}
	// </Reprojection>

	color = max(color, 0.0f);

	//return;
	if(scene_data->reset_accumulator)
	{
		accumulation_buffer[pixel_index * 4 + 0] = color.x;
		accumulation_buffer[pixel_index * 4 + 1] = color.y;
		accumulation_buffer[pixel_index * 4 + 2] = color.z;
	}
	else
	{
		accumulation_buffer[pixel_index * 4 + 0] += color.x;
		accumulation_buffer[pixel_index * 4 + 1] += color.y;
		accumulation_buffer[pixel_index * 4 + 2] += color.z;
	}
}