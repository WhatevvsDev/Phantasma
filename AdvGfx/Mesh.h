#pragma once
#include <vector>

#include "Math.h"

struct MeshTri 
{ 
    glm::vec3 vertex0;
	float pad_0;
	glm::vec3 vertex1;
	float pad_1;
	glm::vec3 vertex2;
	float pad_2;
    glm::vec3 centroid;
	float pad_3;
};

struct Mesh
{
	std::vector<MeshTri> create();
};