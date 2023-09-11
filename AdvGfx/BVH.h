#pragma once

struct BVHNode
{
    glm::vec3 min;
    uint left_first;
    glm::vec3 max;
	uint tri_count;
};

void update_node_bounds( uint nodeIdx );
void subdivide( uint nodeIdx );