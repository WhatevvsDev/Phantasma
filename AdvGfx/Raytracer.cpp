#include "Raytracer.h"
#include "Math.h"
#include "Common.h"

struct Tri 
{ 
    glm::vec3 vertex0, vertex1, vertex2; 
    glm::vec3 centroid;
};

struct Ray 
{ 
    glm::vec3 O, D; float t = 1e30f; 
};

 #define N 64

Tri tris[N];


void intersect_tri( Ray& ray, const Tri& tri )
{
    const glm::vec3 edge1 = tri.vertex1 - tri.vertex0;
    const glm::vec3 edge2 = tri.vertex2 - tri.vertex0;
    const glm::vec3 h = cross( ray.D, edge2 );
    const float a = dot( edge1, h );
    if (a > -0.0001f && a < 0.0001f) return; // ray parallel to triangle
    const float f = 1 / a;
    const glm::vec3 s = ray.O - tri.vertex0;
    const float u = f * dot( s, h );
    if (u < 0 || u > 1) return;
    const glm::vec3 q = cross( s, edge1 );
    const float v = f * dot( ray.D, q );
    if (v < 0 || u + v > 1) return;
    const float t = f * dot( edge2, q );
    if (t > 0.0001f) ray.t = min( ray.t, t );
}

namespace Raytracer
{
    glm::vec3 camPos( 0, 0, -18 );
    glm::vec3 p0( -1, 1, -15 ), p1( 1, 1, -15 ), p2( -1, -1, -15 );
    Ray ray;

	void init()
	{
		for (int i = 0; i < N; i++)
        {
            glm::vec3 r0( RandomFloat(), RandomFloat(), RandomFloat() );
            glm::vec3 r1( RandomFloat(), RandomFloat(), RandomFloat() );
            glm::vec3 r2( RandomFloat(), RandomFloat(), RandomFloat() );
            tris[i].vertex0 = r0 * 9.0f - glm::vec3( 5 );
            tris[i].vertex1 = tris[i].vertex0 + r1;
            tris[i].vertex2 = tris[i].vertex0 + r2;

            tris[i].centroid = tris[i].vertex0 + tris[i].vertex1 + tris[i].vertex2;
            tris[i].centroid *= 1.0f / 3.0f;
        }
	}

	void raytrace(int width, int height, uint32_t* buffer)
	{
        for (int y = 0; y < height; y++) 
        {
            for (int x = 0; x < width; x++)
            {
                glm::vec3 pixelPos = p0 + (p1 - p0) * (x / (float)width) + (p2 - p0) * (y / (float)height);
                ray.O = camPos;
                ray.D = normalize( pixelPos - ray.O );
                ray.t = 1e30f;

                for( int i = 0; i < N; i++ ) intersect_tri( ray, tris[i] );

                bool hit_anything = ray.t != 1e30f;

                buffer[x + y * width] = hit_anything ? 0xffffffff : 0x00000000;
	
            }
        }
    }
}