#include <bgfx_shader.sh>

#ifndef __EDITOR_SH__
#define __EDITOR_SH__

uniform vec4 editorData;

bool IsObjectSelected()
{
    return editorData.x > 0;
}

#endif
