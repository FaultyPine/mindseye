#include "me_serialize.h"

#include "external/inicpp.hpp"
#include "reflector/reflection_types.h"

void SerializeToIniBlocking(
	const meTypeDescriptor& typeDesc, 
	const void* data,
	StringView outFilename)
{
	inicpp::IniManager iniObj(outFilename.data);
	iniObj.set("type", (const char*)typeDesc.name.data);
	for (u64 i = 0; i < typeDesc.fields.size; i++)
	{
		const meTypeDescriptor& field = typeDesc.fields[i];
		if (field.editorName)
		{
			iniObj[field.name.data]["editorName"] = field.editorName.data;
		}
		if (field.tooltip)
		{
			iniObj[field.name.data]["tooltip"] = field.tooltip.data;
		}
		iniObj[field.name.data]["size"] = field.size;
		iniObj[field.name.data]["align"] = field.align;
		iniObj[field.name.data]["offsetBits"] = field.offsetBits;
	}
}

