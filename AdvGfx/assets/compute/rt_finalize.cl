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

double gaussian_weight(int i, int j, float var) 
{
    return 1 / (2 * M_PI) * exp(-0.5 * (i * i + j * j) / var / var);
}

void kernel rt_finalize(
	global float* accumulation_buffer, 
	global uint* render_buffer, 
	global AverageAccumulatedArgs* args, 
	global PerPixelData* detail_buffer
	)
{     
	int x = get_global_id(0);
	int y = get_global_id(1);	
	int width = args->width;
	int height = args->height;

	uint pixel_idx = (x + y * width);

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
			color = clamp(color, 0.0f, 1.0f);
			break;
		}
		case Albedo:
		{
			color = detail_buffer[pixel_idx].albedo.xyz;
			break;
		}
		case Normal:
		{
			color = (detail_buffer[pixel_idx].normal.xyz + 1.0f) * 0.5f;
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

	bool outline_object = detail_buffer[pixel_idx].hit_object != UINT_MAX;
	bool is_pixel_of_selected_object = detail_buffer[pixel_idx].hit_object == args->selected_object_idx;

	if(outline_object && is_pixel_of_selected_object)
	{
		bool neighbor_is_different_object = false;

		for(int lx = -2; lx < 3; lx++)
		{
			for(int ly = -2; ly < 3; ly++)
			{
				if(x == 0 && y == 0)
					continue;

				int new_x = lx + x;
				int new_y = ly + y;

				if(!is_within_bounds(new_x, new_y, width, height))
					continue;

				int i = (new_x + new_y * width);

				if(detail_buffer[i].hit_object - args->selected_object_idx)
				{
					neighbor_is_different_object = true;
					goto draw_outline;
				}
			}
		}
		draw_outline:;
		
		if(neighbor_is_different_object)
		{
			render_buffer[pixel_idx] = 0x2999F2;
			return;
		}
	}

	render_buffer[pixel_idx] = float3_color_to_uint(color);
}