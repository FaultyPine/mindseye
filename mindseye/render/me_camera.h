#pragma once

#include "core/me_defines.h"

struct meMouseInput;
struct MEREFLECT(type) meCamera 
{
    f32 speed = 10.0f;
    glm::vec3 cameraPos   = glm::vec3(0.0f, 5.0f, 0.0f);
    glm::vec3 cameraFront = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

    f32 FOV = 45.0f;
    f32 nearClip = 0.1f;
    f32 farClip = 1000.0f;

	// represents same thing as cameraFront/cameraUp
	// easier to compute with these than directions when taking in mouse inputs
	f32 yaw = -90.0f;
    f32 pitch = 0.0f;
    f32 sensitivity = 0.1f;

	bool isControlledByUserInput = false;

    enum MEREFLECT(type) Projection 
	{
        PERSPECTIVE,
        ORTHOGRAPHIC
    };
    Projection projection = PERSPECTIVE;

	MEAPI glm::vec3 GetNormalizedLookDir();
    MEAPI glm::mat4 GetProjectionMatrix() const;
    MEAPI glm::mat4 GetOrthographicProjection() const;
    MEAPI glm::mat4 GetPerspectiveProjection() const;
    MEAPI glm::mat4 GetViewMatrix() const;
    MEAPI void LookAt(glm::vec3 pos);

    MEAPI void UpdateCameraWithUserInput(OSStateView& osState);
};

