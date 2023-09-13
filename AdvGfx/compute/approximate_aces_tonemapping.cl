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

float3 aces_approx(float3 v)
{
	// Krzysztof Narkowicz - https://knarkowicz.wordpress.com/
    v *= 0.6f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);
}

void kernel approximate_aces(global uint* buffer, global struct SceneData* sceneData)
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
	float tff_recip = 1.0f / 225.0f;

	float3 c = (float3)(r * tff_recip, g * tff_recip, b * tff_recip);
	c = aces_approx(c);

	uint adjusted_r = clamp(c.r * 255.0f, 0.0f, 255.0f);
	uint adjusted_g = clamp(c.g * 255.0f, 0.0f, 255.0f);
	uint adjusted_b = clamp(c.b * 255.0f, 0.0f, 255.0f);

	buffer[pixel_dest] = 0x00010000 * adjusted_b + 0x00000100 * adjusted_g + 0x00000001 * adjusted_r;
}