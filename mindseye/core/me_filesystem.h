#pragma once

#include "mindseye/core/me_defines.h"
#include "mindseye/core/me_memory.h"


// TODO: if the reflector respected inherited types somehow, we wouldn't need to manually specify the string serializer/deserializer
struct MEREFLECT(type, EditorRender=meFsPathEditorRender, Serializer=stringSerializer, Deserializer=stringDeserializer, Equals=sizedBufferEquals, Destroy=stringDestroy, DeepCopy=stringDeepCopy)
meFsPath : public String
{

};

// populates `out` with result of OS path picker window
bool FilesystemPathPicker(const char* title, String& out);

bool meFsPathEditorRender(EditorRenderContext& ctx);

MEAPI StringView meFsGetDirectorySeperator();

// NOTE: will NormalizePathSeperators on the passed in path
MEAPI StringView meFsGetFileFromFullPath(StringView path);

MEAPI StringView msFsGetDirFromPath(StringView path);

MEAPI void meFsNormalizePathSeperators(StringView str);

// looks for the given file
// first, tries the file string itself
// if the file string isn't a relative path, tries looking in current working dir
// then tries looking in all parent dirs to the current working dir
MEAPI String meFsScanOutForFile(StringView file);

// walks a directory tree, collecting the results into the given list
MEAPI bool meFsRecursiveDirectoryWalk(
	const OSFileReference& directory,
	DynArray<OSFileReference>& result);
