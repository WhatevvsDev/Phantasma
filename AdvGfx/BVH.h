#pragma once

#include "Math.h"
#include "Mesh.h"

#include <vector>

struct BVHNode
{
    glm::vec3 min;
    uint left_first;
    glm::vec3 max;
	uint tri_count;
};

struct BVH
{
    BVH(const std::vector<Tri>& tris);

    std::vector<uint>       triIdx;
    std::vector<BVHNode>    bvhNodes;
    std::vector<glm::vec3>  centroids;
    uint nodes_used = 1;
};

void update_node_bounds( uint nodeIdx, BVH& bvh, const std::vector<Tri>& tris);
void subdivide( uint nodeIdx, BVH& bvh, const std::vector<Tri>& tris);