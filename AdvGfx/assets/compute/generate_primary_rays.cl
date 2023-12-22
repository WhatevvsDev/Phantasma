
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

	uint pixel_index = (x + y * width);

	uint rand_seed = WangHash(pixel_index + scene_data->accumulated_frames * width * height);

	float camera_fov = 110.0f;
	float tan_half_angle = tan(radians(camera_fov * 0.5f));
	float aspectScale = width * 0.5f;
	
	float2 pixel = (float2)(x, y) + (float2)(0.5f) - ((float2)(width, height) * 0.5f);
	float3 pixel_dir_tangent = normalize((float3)((float2)(pixel.x, -pixel.y) * tan_half_angle / aspectScale, -1));
	
	float3 disk_pos_tangent = (float3)(sample_uniform_disk(&rand_seed) * scene_data->blur_radius, 0.0f);

	pixel_dir_tangent -= disk_pos_tangent / scene_data->focal_distance;
	float3 pixel_dir_world = transform((float4)(pixel_dir_tangent, 0.0f), scene_data->camera_transform).xyz;;
	float3 disk_pos_world = transform((float4)(disk_pos_tangent, 0.0f), scene_data->camera_transform).xyz;
	
	// Get position from matrix
	float3 cam_pos = (float3)(scene_data->inverse_camera_transform[12], scene_data->inverse_camera_transform[13], scene_data->inverse_camera_transform[14]);
	
	// Actual raytracing
	struct Ray ray;
	ray.O = cam_pos + disk_pos_world;
    ray.D = normalize(pixel_dir_world);
    ray.t = 1e30f;

	primary_rays[pixel_index] = ray;
}