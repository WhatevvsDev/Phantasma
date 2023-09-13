struct SceneData
{
	uint resolution_x;
	uint resolution_y;
	uint tri_count;
	uint dummy;
	float4 cam_pos;
	float4 cam_forward;
	float4 cam_right;
	float4 cam_up;
};

float luminance(float3 v)
{
    return dot(v, (float3)(0.2126f, 0.7152f, 0.0722f));
}

void kernel reinhard(global uint* buffer, global struct SceneData* sceneData)
{     	
	int width = sceneData->resolution_x;
	int height = sceneData->resolution_y;

	int x = get_global_id(0);
	int y = height - get_global_id(1);
	uint pixel_dest = (x + y * width);

	// get RGB triplet	
	uint b = ((buffer[pixel_dest] & 0x00ff0000) >> 16);
	uint g = ((buffer[pixel_dest] & 0x0000ff00) >> 8);
	uint r = ((buffer[pixel_dest] & 0x000000ff) >> 0);

	// Apply reinhard
	float3 c = (float3)(r, g, b);
	float l_in = luminance(c) * 255.0f;
    c = c * (c / (l_in));

	uint adjusted_r = clamp(c.r * 255.0f, 0.0f, 255.0f);
	uint adjusted_g = clamp(c.g * 255.0f, 0.0f, 255.0f);
	uint adjusted_b = clamp(c.b * 255.0f, 0.0f, 255.0f);

	buffer[pixel_dest] = 0x00010000 * adjusted_b + 0x00000100 * adjusted_g + 0x00000001 * adjusted_r;
}