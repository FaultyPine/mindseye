#include "me_camera.h"
#include "platform/me_input.h"

// TODO: once i have input
const s32 TAB_OUT_OF_WINDOW_KEY = 0; //TINY_KEY_TAB;


glm::vec3 meCamera::GetNormalizedLookDir() 
{
	glm::vec3 direction = glm::vec3(0);
	direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	direction.y = sin(glm::radians(pitch));
	direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	return glm::normalize(direction);
}


glm::mat4 meCamera::GetProjectionMatrix() const 
{
    if (projection == PERSPECTIVE) 
	{
        return GetPerspectiveProjection();
    }
    else 
	{
        return GetOrthographicProjection();
    }
}

static u32 GetScreenWidth()
{
	return GetEngineCtx()->osData->windowWidth;
}
static u32 GetScreenHeight()
{
	return GetEngineCtx()->osData->windowHeight;
}

glm::mat4 meCamera::GetOrthographicProjection() const 
{
	u32 screenWidth = GetScreenWidth();
	u32 screenHeight = GetScreenHeight();
    if (projection == PERSPECTIVE) 
	{
        return glm::ortho(0.0f, (f32)screenWidth, (f32)screenHeight, 0.0f, -1.0f, 1.0f); 
    }
    else 
	{
        return glm::ortho(-(f32)screenWidth / 2.0f, (f32)screenWidth / 2.0f, (f32)screenHeight / 2.0f, -(f32)screenHeight / 2.0f, -1.0f, 1.0f);
    }
}

glm::mat4 meCamera::GetPerspectiveProjection() const 
{
	u32 screenWidth = GetScreenWidth();
	u32 screenHeight = GetScreenHeight();
    f32 aspect = (f32)screenWidth / (f32)screenHeight;
    if (screenHeight == 0)
    {
        aspect = 0.0f;
    } 
    return glm::perspective(glm::radians(FOV), aspect, nearClip, farClip);
}

glm::mat4 meCamera::GetViewMatrix() const 
{
    return glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
}

void meCamera::UpdateCameraWithUserInput(OSStateView& osState) 
{
	if (!isControlledByUserInput)
	{
		return;
	}
	meMouseInput& mouseInput = osState.mouseState;
	f32 xoffset = mouseInput.mouseDelta.x;
    f32 yoffset = -mouseInput.mouseDelta.y;

	// after applying this frame's delta, reset
	mouseInput.mouseDelta = glm::vec2(0);
	
	xoffset *= sensitivity;
	yoffset *= sensitivity;

	yaw   += xoffset;
	pitch += yoffset;

	// clamp looking up/down
	if (pitch > 89.0f)
	{
		pitch = 89.0f;
	}
	if (pitch < -89.0f)
	{
		pitch = -89.0f;
	}
	cameraFront = GetNormalizedLookDir();
    
	meCamera& cam = *this;
	meKeyboardInput& keyboardState = osState.keyboardState;
	f32 cameraSpeed = cam.speed * GetDeltaTime();
    if (keyboardState.IsKeyDown(VK_CONTROL))
    {
        cameraSpeed *= 15.0f;
    }
    if (keyboardState.IsKeyDown('W'))
	{
        cam.cameraPos += cameraSpeed * glm::vec3(cam.cameraFront.x, 0.0, cam.cameraFront.z);
    }
    if (keyboardState.IsKeyDown('S')) 
	{
        cam.cameraPos -= cameraSpeed * glm::vec3(cam.cameraFront.x, 0.0, cam.cameraFront.z);
    }
	glm::vec3 cameraRight = glm::normalize(glm::cross(cam.cameraFront, cam.cameraUp));
    if (keyboardState.IsKeyDown('A')) 
	{
        cam.cameraPos -= cameraRight * cameraSpeed;
    }
    if (keyboardState.IsKeyDown('D')) 
	{
        cam.cameraPos += cameraRight * cameraSpeed;
    }
    if (keyboardState.IsKeyDown(VK_SPACE)) 
	{
        cam.cameraPos.y += cameraSpeed;
    }
    if (keyboardState.IsKeyDown(VK_SHIFT)) 
	{
        cam.cameraPos.y -= cameraSpeed;
    }
}

void meCamera::LookAt(glm::vec3 pos) 
{
    glm::vec3 forward = glm::normalize(pos - cameraPos);
    glm::vec3 side = glm::normalize(glm::cross(cameraUp, forward));
    glm::vec3 newUp = glm::cross(forward, side);
    cameraFront = forward;
    cameraUp = newUp;
}