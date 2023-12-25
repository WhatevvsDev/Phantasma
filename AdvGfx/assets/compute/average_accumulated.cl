float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x + b)) / (x*(c*x + d) + e), 0.0f, 1.0f);
}

typedef struct AverageAccumulatedArgs
{
	float samples_reciprocal;
	uint width;
	uint height;
	ViewType view_type;
	uint selected_object_idx;
	uint pad[3];
} AverageAccumulatedArgs;

void kernel average_accumulated(
	global float* accumulation_buffer, 
	global uint* render_buffer, 
	global AverageAccumulatedArgs* args, 
	global PixelDetailInformation* detail_buffer
	)
{     
	int x = get_global_id(0);
	int y = get_global_id(1);

	uint pixel_idx = (x + y * args->width);
	
	int width = args->width;
	int height = args->height;

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

	float3 color = 0;
	
	switch(args->view_type)
	{
		case Render:
		{
			color = (float3)
			(	
				accumulation_buffer[pixel_idx * 4 + 0], 
				accumulation_buffer[pixel_idx * 4 + 1], 
				accumulation_buffer[pixel_idx * 4 + 2]
			);

			color *= args->samples_reciprocal;
			color = ACESFilm(color);
			color = sqrt(color);
			break;
		}
		case Albedo:
		{
			// Not implemented
			break;
		}
		case Normal:
		{
			// Not implemented
			break;
		}
		case BLAS:
		{
			color = detail_buffer[pixel_idx].blas_hits / 16.0f;
			break;
		}
		case TLAS:
		{
			color = detail_buffer[pixel_idx].tlas_hits / 16.0f;
			break;
		}
		case AS:
		{
			color = (detail_buffer[pixel_idx].tlas_hits + detail_buffer[pixel_idx].blas_hits) / 32.0f;
			break;
		}
		default:
		{
			color = (float3)(1.0f, 0.0f, 1.0f);
		}
	}
	
	int r = saturate(color.r) * 255.0f;
	int g = saturate(color.g) * 255.0f;
	int b = saturate(color.b) * 255.0f;

	bool outline_object = detail_buffer[pixel_idx].hit_object != UINT_MAX;
	bool is_pixel_of_selected_object = detail_buffer[pixel_idx].hit_object == args->selected_object_idx;

	if(outline_object && is_pixel_of_selected_object)
	{
		bool neighbor_is_different_object = false;

		for(int i = 0; i < 8; i++)
		neighbor_is_different_object |= detail_buffer[neighbors_idx[i]].hit_object - args->selected_object_idx;
		
		if(neighbor_is_different_object)
		{
			render_buffer[pixel_idx] = 0x2999F2;
			return;
		}
	}

	render_buffer[pixel_idx] = 0x00010000 * b + 0x00000100 * g + 0x00000001 * r;
}