#pragma once
#include <vector>

#include "Math.h"

struct BVH;

struct Tri 
{ 
    glm::vec3 vertex0;
	float pad_0;
	glm::vec3 vertex1;
	float pad_1;
	glm::vec3 vertex2;
	float pad_2;
};

struct Mesh
{
	Mesh(const std::string& path);
	std::vector<Tri> tris;
	BVH* bvh;
};