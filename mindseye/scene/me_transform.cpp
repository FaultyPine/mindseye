#pragma once

#include "me_transform.h"
#include "generatedtypes/me_transform.generated.cpp"



bool BoundingBox::Intersects(const BoundingBox& other) const
{
	return (min.x <= other.max.x && max.x >= other.min.x) &&
		(min.y <= other.max.y && max.y >= other.min.y) &&
		(min.z <= other.max.z && max.z >= other.min.z);
}

