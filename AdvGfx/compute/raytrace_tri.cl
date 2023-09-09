struct Tri 
{ 
    float3 vertex0, vertex1, vertex2; 
    float3 center;
};

struct Ray 
{ 
    float3 O;
    float3 D; 
    float t; 
};

//struct AABB
//{
//    float3 min;
//    float3 max;
//};
//
//struct BVHNode
//{
//    struct AABB aabb;
//    uint left_first, tri_count;
//};

void IntersectTri( struct Ray* ray, const struct Tri tri)
{
	const float3 edge1 = tri.vertex1 - tri.vertex0;
	const float3 edge2 = tri.vertex2 - tri.vertex0;
	const float3 h = cross( ray->D, edge2 );
	const float a = dot( edge1, h );
	if (a > -0.0001f && a < 0.0001f) return; // ray parallel to triangle
	const float f = 1 / a;
	const float3 s = ray->O - tri.vertex0;
	const float u = f * dot( s, h );
	if (u < 0 || u > 1) return;
	const float3 q = cross( s, edge1 );
	const float v = f * dot( ray->D, q );
	if (v < 0 || u + v > 1) return;
	const float t = f * dot( edge2, q );
	if (t > 0.0001f) ray->t = min( ray->t, t );
}

//bool IntersectAABB( const struct Ray* ray, const float3 bmin, const float3 bmax )
//{
//	float tx1 = (bmin.x - ray->O.x) / ray->D.x, tx2 = (bmax.x - ray->O.x) / ray->D.x;
//	float tmin = min( tx1, tx2 ), tmax = fmax( tx1, tx2 );
//	float ty1 = (bmin.y - ray->O.y) / ray->D.y, ty2 = (bmax.y - ray->O.y) / ray->D.y;
//	tmin = max( tmin, min( ty1, ty2 ) ), tmax = min( tmax, max( ty1, ty2 ) );
//	float tz1 = (bmin.z - ray->O.z) / ray->D.z, tz2 = (bmax.z - ray->O.z) / ray->D.z;
//	tmin = max( tmin, min( tz1, tz2 ) ), tmax = min( tmax, max( tz1, tz2 ) );
//	return tmax >= tmin && tmin < ray->t && tmax > 0;
//}
//
//void intersect_bvh( struct Ray* ray, const uint nodeIdx, struct BVHNode* nodes, struct Tri* tris, uint* trisIdx)
//{
//	struct BVHNode node = nodes[nodeIdx];
//	if (!IntersectAABB( ray, node.aabb.min, node.aabb.max )) 
//		return;
//
//	if (node.tri_count > 0)
//	{
//		for (uint i = 0; i < node.tri_count; i++ )
//			IntersectTri( ray, tris[trisIdx[node.left_first + i]] );
//	}
//	else
//	{
//		intersect_bvh( ray, node.left_first, nodes, tris, trisIdx);
//		intersect_bvh( ray, node.left_first + 1, nodes, tris, trisIdx);
//	}
//}

//void kernel raytrace(global uint* buffer, global const struct BVHNode* nodes, global const struct Tri* tris, global const uint* trisIdx)
void kernel raytrace(global uint* buffer, global const struct Tri* tris)
{      
	float3 camPos = (float3)( 0.0f, 0.0f, -18.0f );
    float3 p0 = (float3)( -1.0f, 1.0f, -15.0f );
	float3 p1 = (float3)( 1.0f, 1.0f, -15.0f );
	float3 p2 = (float3)( -1.0f, -1.0f, -15.0f );
	
	int x = get_global_id(0);
	int y = get_global_id(1);
	
	int width = 640;
	int height = 640;


	float3 pixelPos = p0 + (p1 - p0) * (x / (float)width) + (p2 - p0) * (y / (float)height);

	struct Ray ray;
    ray.O = camPos;
    ray.D = normalize( pixelPos - ray.O );
    ray.t = 1e30f;

	//uint rootNodeIdx = 0;

	for(int i = 0; i < 2048; i++)
		IntersectTri(&ray, tris[i]);

    //intersect_bvh(&ray, rootNodeIdx, nodes, tris, trisIdx);

	bool hit_anything = ray.t < 1e30f;

	if(hit_anything)
	{
		buffer[x + y * width] = 0xffffffff;
	}
	else
	{
		buffer[x + y * width] = 0x00000000;
	}
}                                                                               