#pragma once

#include "core/me_defines.h"
#include "reflector/reflection_types.h"

#include <external/glm/glm.hpp>
#include <external/glm/gtx/string_cast.hpp>
#include <external/glm/gtc/matrix_transform.hpp>
#include <external/glm/gtc/type_ptr.hpp>
#include <external/glm/gtx/quaternion.hpp>
#include <external/glm/gtx/matrix_decompose.hpp>


extern meTypeDescriptor TD_VEC3;
extern meTypeDescriptor TD_QUAT;

constexpr double PI   = 3.141592653589793238463;
constexpr float  PI_F = 3.14159265358979f;

inline f32 DegToRad(f32 deg)
{
    return deg * (PI / 180);
}
inline f32 RadToDeg(f32 rad)
{
    return rad * (180/PI);
}

struct Frustum
{
	union
	{
		glm::vec4 planes[6];
		struct
		{
			glm::vec4 left; glm::vec4 right;
			glm::vec4 bottom; glm::vec4 top;
			glm::vec4 nplane; glm::vec4 fplane;
		};
	};
	// Extracts frustum planes from a (projection * view) matrix (world-to-clip space)
	Frustum(
        const glm::mat4& projectionViewMatrix);

	bool ShouldCullSphere(glm::vec3 center, float radius);
};

namespace Math {



MEAPI bool isOverlappingRectSize2D(const glm::vec2& pos1, const glm::vec2& size1, const glm::vec2& pos2, const glm::vec2& size2);
MEAPI bool isOverlappingRect2D(const glm::vec2& startPos1, const glm::vec2& endPos1, const glm::vec2& startPos2, const glm::vec2& endPos2);
MEAPI bool isPointInRectangle(const glm::vec2& point, const glm::vec2& rectStart, const glm::vec2& rectEnd);
MEAPI bool isPositionNear(const glm::vec2& pos1, const glm::vec2& pos2, f32 dist);

MEAPI f32 Lerp(f32 a, f32 b, f32 t);
MEAPI f32 InvLerp(f32 a, f32 b, f32 v);
MEAPI f32 Remap(f32 val, f32 iMin, f32 iMax, f32 oMin, f32 oMax);

MEAPI glm::vec3 Lerp(glm::vec3 a, glm::vec3 b, f32 t);

MEAPI glm::vec2 RandomPointInCircle(f32 radius);
MEAPI glm::vec3 RandomPointInSphere(f32 radius);

template<typename T> inline T Max(T x, T y) { return x > y ? x : y; }
template<typename T> inline T Min(T x, T y) { return x < y ? x : y; }
template <typename T> inline T Clamp(const T& value, const T& low, const T& high) {
    return value < low ? low : (value > high ? high : value); 
}

template <typename T> int signof(T val) {
    return (T(0) < val) - (val < T(0));
}

template <typename T> bool isInRange(T x, T min, T max) {
    return min <= x && x <= max;
}

// https://www.youtube.com/watch?v=LSNQuFEDOyQ
inline f32 ExpoDecay(f32 a, f32 b, f32 decay, f32 deltaTimeSeconds)
{
    return b + (a - b) * exp(-decay * deltaTimeSeconds);
}

MEAPI uint32_t hash(const char* message, size_t message_length);

MEAPI u32 countLeadingZeroes(u32 n);

MEAPI glm::mat4 Position3DToModelMat(const glm::vec3& position, const glm::vec3& scale = glm::vec3(1), f32 rotation = 0.0, const glm::vec3& rotationAxis = {1,0,0});
MEAPI glm::mat4 Position3DToModelMat(const glm::vec3& position, const glm::vec3& scale = glm::vec3(1), const glm::quat& rotation = glm::identity<glm::quat>());
MEAPI glm::mat4 Position2DToModelMat(const glm::vec2& position, const glm::vec2& scale = glm::vec3(1), f32 rotation = 0.0, const glm::vec3& rotationAxis = {0,0,1});

template <typename T>
T PercentOf(T x, u32 percentOutOf100)
{
    percentOutOf100 = Math::Clamp(percentOutOf100, 0u, 100u);
    return (x * percentOutOf100) / 100;
}

}


