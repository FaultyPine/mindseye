#pragma once

#include "core/me_defines.h"
#include "core/me_math.h"

#include "generatedtypes/me_transform.generated.h"

struct MEREFLECT(type) Transform 
{
    glm::vec3 position = glm::vec3(0);
    glm::vec3 scale = glm::vec3(1);
    f32 rotation = 0.0;
    glm::vec3 rotationAxis = {0,1,0};

    Transform(
        const glm::vec3& pos = glm::vec3(0), 
        const glm::vec3& scl = glm::vec3(1), 
        f32 rot = 0.0, 
        const glm::vec3& rotAxis = {0,1,0})
    {
        position = pos;
        scale = scl;
        rotation = rot;
        rotationAxis = rotAxis;
    }

    glm::mat4 ToModelMatrix() const 
    {
        return Math::Position3DToModelMat(position, scale, rotation, rotationAxis);
    }
};

struct BoundingBox 
{
    BoundingBox() = default;
    BoundingBox(glm::vec3 mn, glm::vec3 mx) {min = mn; max = mx;}
    static BoundingBox FromCenterHlfExtents(glm::vec3 center, glm::vec3 halfExtents)
    {
        return BoundingBox(center - halfExtents, center + halfExtents);
    }
    glm::vec3 min, max = glm::vec3(0);
    glm::vec3 center()
    {
        glm::vec3 center = min + ((max - min) / 2.0f);
        return center;
    }
    glm::vec3 halfExtents()
    {
        glm::vec3 halfExtents = max - center();
        return halfExtents;
    }
};
