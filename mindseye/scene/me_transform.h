#pragma once

#include "core/me_defines.h"
#include "core/me_math.h"

#include "generatedtypes/me_transform.generated.h"

struct MEREFLECT(type) meTransform 
{
    glm::vec3 position = glm::vec3(0);
    glm::vec3 scale = glm::vec3(1);
	glm::quat rotation = glm::identity<glm::quat>();

    meTransform(
        const glm::vec3& pos = glm::vec3(0), 
        const glm::vec3& scl = glm::vec3(1), 
		const glm::quat& rot = glm::identity<glm::quat>())
    {
        position = pos;
        scale = scl;
		rotation = rot;
    }
	meTransform(const glm::mat4x4& mat)
	{
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::decompose(mat, scale, rotation, position, skew, perspective);
	}

    glm::mat4 ToModelMatrix() const 
    {
        return Math::Position3DToModelMat(position, scale, rotation);
    }
};

struct MEREFLECT(type) BoundingBox 
{
    BoundingBox() = default;
    BoundingBox(glm::vec3 mn, glm::vec3 mx) {min = mn; max = mx;}
    static BoundingBox FromCenterHalfExtents(glm::vec3 center, glm::vec3 halfExtents)
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
	bool Intersects(const BoundingBox& other) const;
};
