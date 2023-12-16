
typedef struct __attribute__ ((packed)) Ray
{ 
    float3 O;
    float t;
    float3 D;
	uint* intersection;
} Ray;

typedef struct GeneratePrimaryRaysDesc
{
	uint2 size; // Width x Height
} GeneratePrimaryRaysDesc;

void kernel generate_primary_rays(
	global GeneratePrimaryRaysDesc* desc,
	global SceneData* scene_data,
	global Ray* primary_rays
	)
{     
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = desc->size[0];
	int height = desc->size[1];
	float aspect_ratio = (float)(width) / (float)(height);

	uint pixel_index = (x + y * width);

	uint rand_seed = WangHash(pixel_index + scene_data->accumulated_frames * width * height);

	// Getting ray direction (+ DoF)
	float x_t = ((((float)x + RandomFloat(&rand_seed)) / (float)width) - 0.5f) * 2.0f;
	float y_t = ((((float)y + RandomFloat(&rand_seed)) / (float)height)- 0.5f) * 2.0f;

	float3 pixel_dir_tangent = 
	(float3)(0.0f, 0.0f, -1.0f) + 
	(float3)(1.0f, 0.0f, 0.0f) * x_t * aspect_ratio -
	(float3)(0.0f, 1.0f, 0.0f) * y_t;

	float3 disk_pos_tangent = (float3)(sample_uniform_disk(&rand_seed) * scene_data->blur_radius, 0.0f);

	pixel_dir_tangent -= disk_pos_tangent / scene_data->focal_distance;
	float3 pixel_dir_world = transform((float4)(pixel_dir_tangent, 0.0f), scene_data->camera_transform).xyz;;
	float3 disk_pos_world = transform((float4)(disk_pos_tangent, 0.0f), scene_data->camera_transform).xyz;
	
	// TODO: this is kinda dumb innit?
	float3 cam_pos = transform((float4)(0.0f, 0.0f, 0.0f, 1.0f), scene_data->camera_transform).xyz;

	// Actual raytracing
	struct Ray ray;
	ray.O = cam_pos + disk_pos_world;
    ray.D = normalize(pixel_dir_world);
    ray.t = 1e30f;

	primary_rays[pixel_index] = ray;
}