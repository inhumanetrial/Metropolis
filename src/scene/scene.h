#pragma once
#include <glm/glm.hpp>
struct Object {
    glm::vec4 v0;
    glm::vec4 v1;
    glm::vec4 v2;
    glm::vec4 v3;
    glm::vec4 colorAndType;
    glm::vec4 emission;
};
struct Sphere {
    glm::vec4 posAndRadius;   // x, y, z = position, w = radius
    glm::vec4 colorAndPadding; // x, y, z = color, w = padding
};

