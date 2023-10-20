void kernel average_accumulated(global float* accumulation_buffer, global uint* render_buffer, global float* sample_count_reciprocal, global struct SceneData* scene_data)
{     
	int x = get_global_id(0);
	int y = get_global_id(1);

	uint pixel_dest = (x + y * scene_data->resolution_x);

	float3 color = (float3)(
		accumulation_buffer[pixel_dest * 4 + 0], 
		accumulation_buffer[pixel_dest * 4 + 1], 
		accumulation_buffer[pixel_dest * 4 + 2]);

	color *= *sample_count_reciprocal;

	color.x = sqrt(color.x);
	color.y = sqrt(color.y);
	color.z = sqrt(color.z);
	
	int r = clamp((int)(color.x * 255.0f), 0, 255);
	int g = clamp((int)(color.y * 255.0f), 0, 255);
	int b = clamp((int)(color.z * 255.0f), 0, 255);

	render_buffer[pixel_dest] = 0x00010000 * b + 0x00000100 * g + 0x00000001 * r;
}