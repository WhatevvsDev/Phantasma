#define DEPTH 16

typedef struct TextureHeader
{
	uint start_offset;
	uint width;
	uint height;
	uint pad;
} TextureHeader;

float3 get_exr_color(float3 direction, float* exr, int2 exr_size, float exr_angle)
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

float3 interpolate_tri_normal(VertexData* vertex_data, struct ExtendOutput* extend_output)
{
	float u = extend_output->u;
	float v = extend_output->v;
	float w = 1.0f - u - v;

	uint idx = extend_output->tri_hit * 3;

	float3 v0_normal = vertex_data[idx + 0].normal * w;
	float3 v1_normal = vertex_data[idx + 1].normal * u;
	float3 v2_normal = vertex_data[idx + 2].normal * v;

	return normalize(v0_normal + v1_normal + v2_normal);
}

float2 interpolate_tri_uvs(VertexData* vertex_data, struct ExtendOutput* extend_output)
{
	float u = extend_output->u;
	float v = extend_output->v;
	float w = 1.0f - u - v;

	uint idx = extend_output->tri_hit * 3;

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

typedef struct ShadeArgs
{
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
	unsigned char* textures;
    Ray* shading_ray;
    Ray* ray_buffer;
    WavefrontData* wavefront_data;
    ExtendOutput* extend_output;
} ShadeArgs;

bool shade(ShadeArgs* args)
{
	// Keeping track of current ray
    Ray shading_ray = *args->shading_ray;
    Ray new_ray;

	uint depth = DEPTH;
	float3 color = (float3)(0.0f);

    int hit_mesh_header_idx = args->extend_output->hit_mesh_header_idx;

    bool is_primary_ray = false;
    
    bool hit_anything = hit_mesh_header_idx != UINT_MAX;

    if(!hit_anything)
    {
        float3 exr_color = get_exr_color(shading_ray.D, args->exr, args->exr_size, args->exr_angle);

        // Slighly Biased way to get rid of fireflies
        float sqr_length = dot(exr_color, exr_color);
        float light_limit = 16.0f;

        exr_color = (sqr_length < light_limit ? exr_color : normalize(exr_color) * light_limit);

        shading_ray.e_energy = shading_ray.t_energy * exr_color;
        *args->shading_ray = shading_ray;
        return false;
    }
    else
    {
        MeshInstanceHeader* instance = &(*args->world_data).instances[hit_mesh_header_idx];
        MeshHeader* mesh = &args->mesh_headers[instance->mesh_idx];	
        Material mat = args->materials[instance->material_idx];

        VertexData* vertex_data = &args->vertex_data[mesh->vertex_data_offset];

        float3 hit_pos = shading_ray.O + (shading_ray.D * shading_ray.t);
        float3 normal = interpolate_tri_normal(vertex_data, args->extend_output);
        float3 geo_normal = args->extend_output->geo_normal.xyz;
        float2 uvs = interpolate_tri_uvs(vertex_data, args->extend_output);

        // We have to apply transform so normals are world-space

        float local_inverse_transform_ver[16];
        copy4x4(instance->inverse_transform, &local_inverse_transform_ver);
        transpose4x4(&local_inverse_transform_ver);

        normal = transform((float4)(normal, 0.0f), &local_inverse_transform_ver).xyz;
        geo_normal = transform((float4)(geo_normal, 0.0f), &local_inverse_transform_ver).xyz;
        
        normal = normalize(normal);
        geo_normal = normalize(geo_normal);

        bool inner_normal = dot(geo_normal, shading_ray.D) > 0.0f;

        float3 hemisphere_normal = malleys_method(args->rand_seed).xyz;

        if(inner_normal)
            normal = -normal;

        hemisphere_normal = tangent_to_base_vector(hemisphere_normal, normal);

        float old_ray_distance = shading_ray.t;
        new_ray.O = hit_pos + hemisphere_normal * EPSILON;
        new_ray.t = 1e30f;
        depth--;

        float3 reflected_dir = reflected(shading_ray.D, normal);
        float random = RandomFloat(args->rand_seed);

        float3 material_color = mat.albedo.xyz * (mat.albedo.a + 1.0f);

        bool is_emissive = mat.albedo.a != 0.0f;

        // <Texture Lookup>
        if(instance->texture_idx != -1)
        {
            TextureHeader header = args->texture_headers[instance->texture_idx];

            const uint channels = 4;

            // Get coordinates, float, uint, fractional
            // TODO: check for sampling off screen
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

        }
        // </Texture Lookup>

        material_color *= (mat.albedo.a + 1.0f);

        // <Russian roulette>
        {
            float die_chance = max(max((float)material_color.x, (float)material_color.y), (float)material_color.z);
            die_chance = clamp(die_chance, 0.0f, 1.0f);

            if(RandomFloat(args->rand_seed) > die_chance)
                return false;
                
            material_color /= die_chance;
        }
        // </Russian roulette>

        switch(mat.type)
        {
            case Diffuse:
            {
                new_ray.D = (mat.specularity < random)
                        ? hemisphere_normal
                        : reflected_dir;
                
                float3 brdf = material_color / PI;

                float3 diffuse = brdf * 2.0f * dot(normal, new_ray.D);
                float3 specular = material_color * (1.0f - mat.specularity);

                float3 final_color = lerp(diffuse, specular, mat.specularity);


                shading_ray.e_energy += shading_ray.t_energy * final_color;
                shading_ray.t_energy *= lerp(final_color, 1.0f, mat.specularity);

                break;
            }
            case Metal:
            {
                float3 reflected_dir = reflected(shading_ray.D, normal) * (1.0f + EPSILON);
                
                new_ray.D = normalize(reflected_dir + random_unit_vector(args->rand_seed, normal) * (1.0f - mat.specularity));
                
                shading_ray.e_energy += shading_ray.t_energy * material_color * 0.5f;
                shading_ray.t_energy *= material_color * 0.5f;

                break;
            }
            case Dielectric:
            {
                float refraction_ratio = (inner_normal ? (1.0f / mat.ior) : mat.ior);// <- Only use with objects that are enclosed

                float reflectance = fresnel(shading_ray.D, normal, refraction_ratio);
                float transmittance = 1.0f - reflectance;
            
                if(reflectance > random)
                {
                    new_ray.D = reflected(shading_ray.D, normal);
                    shading_ray.e_energy *= shading_ray.t_energy * material_color;
                    shading_ray.t_energy *= material_color;
                }
                else
                {
                    new_ray.D = refracted(shading_ray.D, normal, refraction_ratio);
                    float transmission_factor = inner_normal ? beers_law(old_ray_distance, mat.absorption_coefficient) : 1.0f;

                    shading_ray.e_energy *= material_color * transmission_factor * shading_ray.t_energy;
                    shading_ray.t_energy *= material_color * transmission_factor;
                }
                break;
            }
        }
    }

    *args->shading_ray = shading_ray;
    int index = atomic_fetch_add(&args->wavefront_data->extended_ray_count, 1);
    new_ray.t = 1e30;
    new_ray.screen_pos = shading_ray.screen_pos;
    new_ray.e_energy = shading_ray.e_energy;
    new_ray.t_energy = shading_ray.t_energy;
    args->ray_buffer[index] = new_ray;
    
	return true;
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

void kernel rt_shade(
	global VertexData* vertex_data, 
	global struct Tri* tris, 
	global uint* trisIdx, 
	global struct MeshHeader* mesh_headers, 
	global unsigned char* textures,
	global struct MeshHeader* texture_headers, 
	global struct SceneData* scene_data, 
	global float* exr, 
	global struct WorldManagerDeviceData* world_manager_data, 
	global struct Material* materials, 
	global WavefrontData* wavefront_data,
    global Ray* ray_buffer,
    global float4* accumulation_buffer,
    global ExtendOutput* extend_output
    )
{     
	uint width = scene_data->resolution.x;
	uint height = scene_data->resolution.y;
    
    float aspect_ratio = (float)(width) / (float)(height);

	int x = get_global_id(0);
	int y = get_global_id(1);
	uint pixel_index = (x + y * width);

    if(pixel_index == 0)
    {
        atomic_store(&wavefront_data->extended_ray_count, 0);
    }


    int ray_limit = atomic_load(&wavefront_data->shaded_ray_count);

    if(pixel_index >= ray_limit)
        return;

	uint rand_seed = WangHash(pixel_index + scene_data->accumulated_frames * width * height);

    Ray shading_ray = ray_buffer[pixel_index];
    int ray_screen_index = (shading_ray.screen_pos.y * width) + shading_ray.screen_pos.x;

    barrier(CLK_GLOBAL_MEM_FENCE);

    struct ShadeArgs shade_args;
    shade_args.vertex_data = vertex_data;
    shade_args.tris = tris;
    shade_args.trisIdx = trisIdx;
    shade_args.rand_seed = &rand_seed;
    shade_args.mesh_headers = mesh_headers;
    shade_args.texture_headers = texture_headers;
    shade_args.exr = exr;
    shade_args.exr_size = scene_data->exr_size;
    shade_args.exr_angle = scene_data->exr_angle;
    shade_args.world_data = world_manager_data;
    shade_args.materials = materials;
    shade_args.textures = textures;
    shade_args.ray_buffer = ray_buffer;
    shade_args.wavefront_data = wavefront_data;
    shade_args.shading_ray = &shading_ray;
    shade_args.extend_output = &extend_output[pixel_index];
    
    bool generated_bounce_ray = shade(&shade_args);
    
    //color = max(color, 0.0f);

    if(!generated_bounce_ray)
    {
        float3 color = shading_ray.e_energy;
        accumulation_buffer[ray_screen_index] += (float4)(color, 0.0f);
    }
}