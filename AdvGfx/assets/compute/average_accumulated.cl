void kernel average_accumulated(
	global float* accumulation_buffer, 
	global uint* render_buffer, 
	global float* sample_count_reciprocal, 
	global struct SceneData* scene_data,
	global PixelDetailInformation* detail_buffer
	)
{     
	int x = get_global_id(0);
	int y = get_global_id(1);

	uint pixel_idx = (x + y * scene_data->resolution.x);
	
	int width = scene_data->resolution.x;
	int height = scene_data->resolution.y;

	uint neighbors_idx[8] = 
	{
		clamp(x - 1, 0, width - 1) + clamp(y - 1, 0, height - 1) * width,
		clamp(x - 1, 0, width - 1) + clamp(y + 1, 0, height - 1) * width,
		clamp(x + 1, 0, width - 1) + clamp(y - 1, 0, height - 1) * width,
		clamp(x + 1, 0, width - 1) + clamp(y + 1, 0, height - 1) * width,
		clamp(x - 2, 0, width - 1) + clamp(y - 2, 0, height - 1) * width,
		clamp(x - 2, 0, width - 1) + clamp(y + 2, 0, height - 1) * width,
		clamp(x + 2, 0, width - 1) + clamp(y - 2, 0, height - 1) * width,
		clamp(x + 2, 0, width - 1) + clamp(y + 2, 0, height - 1) * width
	};

	float3 color = (float3)(
		accumulation_buffer[pixel_idx * 4 + 0], 
		accumulation_buffer[pixel_idx * 4 + 1], 
		accumulation_buffer[pixel_idx * 4 + 2]);

	color *= *sample_count_reciprocal;

	color = sqrt(color);

	int r = clamp((int)(color.x * 255.0f), 0, 255);
	int g = clamp((int)(color.y * 255.0f), 0, 255);
	int b = clamp((int)(color.z * 255.0f), 0, 255);

	bool outline_object = detail_buffer[pixel_idx].hit_object != UINT_MAX;
	bool is_pixel_of_selected_object = detail_buffer[pixel_idx].hit_object == scene_data->selected_object_idx;

	if(outline_object && is_pixel_of_selected_object)
	{
		bool neighbor_is_different_object = false;

		for(int i = 0; i < 8; i++)
		neighbor_is_different_object |= detail_buffer[neighbors_idx[i]].hit_object - scene_data->selected_object_idx;
		
		if(neighbor_is_different_object)
		{
			r = 0xF2;
			g = 0x99;
			b = 0x29;
		}
	}
	render_buffer[pixel_idx] = 0x00010000 * b + 0x00000100 * g + 0x00000001 * r;
}