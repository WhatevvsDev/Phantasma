typedef struct GeneratePrimaryRaysArgs
{
	uint width;
	uint height;
	uint accumulated_frames;
	float blur_radius;
	float focal_distance;
	float camera_fov;
	bool reset_accumulator;
	float pad[1];
	float camera_transform[16];
} GeneratePrimaryRaysArgs;

void kernel rt_generate_rays(
	global GeneratePrimaryRaysArgs* args,
	global Ray* ray_buffer,
	global WavefrontData* wavefront_data,
	global float4* accumulation_buffer
	)
{     


	
	int x = get_global_id(0);
	int y = get_global_id(1);
	int width = args->width;
	int height = args->height;

	uint pixel_index = (x + y * width);

	//if(pixel_index == 0)
	//	printf("Size of ray: %u \n", sizeof(Ray));

	uint rand_seed = WangHash(pixel_index + args->accumulated_frames * width * height);

	float camera_fov = 110.0f;
	float tan_half_angle = tan(radians(camera_fov * 0.5f));
	float aspectScale = width * 0.5f;
	
	float2 pixel = (float2)(x, y) + (float2)(0.5f) - ((float2)(width, height) * 0.5f);
	float3 pixel_dir_tangent = normalize((float3)((float2)(pixel.x, -pixel.y) * tan_half_angle / aspectScale, -1));
	
	float3 disk_pos_tangent = (float3)(sample_uniform_disk(&rand_seed) * args->blur_radius, 0.0f);

	pixel_dir_tangent -= disk_pos_tangent / args->focal_distance;
	float3 pixel_dir_world = transform((float4)(pixel_dir_tangent, 0.0f), args->camera_transform).xyz;;
	float3 disk_pos_world = transform((float4)(disk_pos_tangent, 0.0f), args->camera_transform).xyz;
	
	// Get position from matrix
	float3 cam_pos = (float3)(args->camera_transform[12], args->camera_transform[13], args->camera_transform[14]);
	
	if(args->reset_accumulator)
	{
		accumulation_buffer[pixel_index] = 0;
	}

	// Actual raytracing
	struct Ray ray;
	ray.O = cam_pos + disk_pos_world;
    ray.D = normalize(pixel_dir_world);
    ray.t = 1e30f;
	ray.screen_pos.x = x;
	ray.screen_pos.y = y;
	ray.e_energy = 0.0f;
	ray.t_energy = 1.0f;

	if(pixel_index == 0)
	{
		atomic_store(&wavefront_data->ray_count, width * height);
	}

	ray_buffer[pixel_index] = ray;
}