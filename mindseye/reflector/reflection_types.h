#pragma once

#include "mindseye/core/me_defines.h"
#include "mindseye/core/containers/dynarray.h"

typedef s32 meTypeID;

struct meTypeDescriptor
{
	String name = {};
	String editorName = {};
	String tooltip = {};
	DynArray(meTypeDescriptor) fields = nullptr;
	s32 value = 0;
	s32 flags = 0;
	
	u32 size = 0;
	u32 align = 0;
	s32 offset = 0;

	meTypeDescriptor* underlyingType = nullptr;

	bool operator==(const meTypeDescriptor& other) const
	{
		return name == other.name &&
			size == other.size &&
			offset == other.offset &&
			align == other.align && 
			flags == other.flags && 
			underlyingType == other.underlyingType;
	}
};


extern meTypeDescriptor TD_UNSIGNED_INT;
extern meTypeDescriptor TD_INT;
extern meTypeDescriptor TD_UNSIGNED_SHORT;
extern meTypeDescriptor TD_SHORT;
extern meTypeDescriptor TD_UNSIGNED_LONG;
extern meTypeDescriptor TD_LONG;
extern meTypeDescriptor TD_LONGLONG;
extern meTypeDescriptor TD_UNSIGNED_LONGLONG;
extern meTypeDescriptor TD_FLOAT;
extern meTypeDescriptor TD_DOUBLE;
extern meTypeDescriptor TD_BOOL;
extern meTypeDescriptor TD_CHAR;
extern meTypeDescriptor TD_UNSIGNED_CHAR;
extern meTypeDescriptor TD_WCHAR;
extern meTypeDescriptor TD_POINTER;
