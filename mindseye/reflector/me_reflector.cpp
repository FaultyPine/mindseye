

#define ME_CORE_ONLY
#include "mindseye/me_unity.cpp"

#include "clang/AST/AST.h"
#include "clang/AST/Type.h"
#include "clang-c/Index.h"

#include "reflection_types.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

/*
Generate in-memory representation of the data i care about.
The pass that to a code generator to write out generated headers for mindseye to use.

just allocate and leak whatever you need... if i end up concerned about memory usage of this metaprogram, there's other problems...

Useful debugging tip when doing stuff with clang ast:
clang++ -Xclang -ast-dump -std=c++20 -fsyntax-only -IC:\Dev\mindseye -IC:\Dev\mindseye\mindseye scene/me_scene.h > me_scene.h.ast
can dump the actual clang ast, so you can see what cursor is which and the whole parent/child hierarchy

Inspiration for this type of reflection system came from Bobby Anguelov's Esoterica Engine
https://github.com/BobbyAnguelov/Esoterica/tree/main/Code/Applications/Reflector/TypeReflection
*/

struct ReflectedTypeIdentifier
{
	u32 headerID;
	u32 nameID;
	bool operator==(const ReflectedTypeIdentifier& other) const
	{
		return headerID == other.headerID && nameID == other.nameID;
	}
};

// can represent a number of things
// like structure types, and also field decls
// TODO: merge this with meTypeDescriptor
struct meReflectedType
{
	DynArray(meReflectedType*) children = {};
	StringView name = {};
	CXCursorKind kind = CXCursor_NoDeclFound;
	meReflectedType* innerType = nullptr; // for fields, this is their type
	StringView editorName = {};
	StringView tooltip = {};
	u32 size = 0;
	s32 offsetBits = 0; // offset of this type from it's parent type, in bits
	u32 align = 0;
	bool isExcluded = false;
	void Print() const;
	bool operator==(const meReflectedType& other) const
	{
		return name == other.name &&
			size == other.size &&
			offsetBits == other.offsetBits &&
			align == other.align;
	}

	meReflectedType() = default;
	// TODO: merge this with meTypeDescriptor, this is a temp stopgap
	meReflectedType(const meTypeDescriptor& other)
	{
		name = other.name;
		//kind = other.
		//innerType = other.underlyingType
		editorName = other.editorName;
		tooltip = other.tooltip;
		size = other.size;
		offsetBits = other.offset;
		align = other.align;
	}
};

struct meReflectedFile
{
	String fileName;
	// name hash -> type
	meMap<u32, meReflectedType> reflectedTypes = {};
};

struct ClangParsingContext
{
	Arena* allocator = nullptr;
	String projectRootDir = {};
	// header id -> file reflection info
	meMap<u32, meReflectedFile> reflectedFiles = {};
	CXTranslationUnit* tu = nullptr;

	static ClangParsingContext& GetSingleInstance()
	{
		static ClangParsingContext g_parsingContext = {};
		return g_parsingContext;
	}
};

void meReflectedType::Print() const
{
	LOG_INFO("%.*s\n\t[excluded = %i] [editorName = %.*s] [tooltip = %.*s] offset = %i size = %u align = %u", 
		STRING_VAARGS(name), isExcluded, STRING_VAARGS(editorName), STRING_VAARGS(tooltip), offsetBits, size, align);
	if (children)
	{
		s32 numChildren = DynArrayGetSize(children);
		for (s32 i = 0; i < numChildren; i++)
		{
			const meReflectedType* typeRefl = children[i];
			LOG_INFO("Child:");
			typeRefl->Print();
		}
	}
}

inline StringView GetCursorDisplayName(const CXCursor& cr, meAllocator* allocator )
{
	auto displayName = clang_getCursorDisplayName(cr);
	const char* strMem = clang_getCString(displayName);
	StringView result = StringView(strMem, CStringLength(strMem)); 
	//result.CopyOfCStr(strMem, allocator);
	//clang_disposeString(displayName);
	return result;
}

inline u32 GetLineNumberForCursor(const CXCursor& cr)
{
	uint32_t line, column, offset;
	CXSourceRange range = clang_getCursorExtent(cr);
	CXSourceLocation start = clang_getRangeStart(range);
	clang_getExpansionLocation( start, nullptr, &line, &column, &offset );
	return line;
}

String GetHeaderPathForCursor(CXCursor cr, meAllocator* allocator)
{
	CXFile file;
	CXSourceRange const cursorRange = clang_getCursorExtent( cr );
	clang_getExpansionLocation( clang_getRangeStart( cursorRange ), &file, nullptr, nullptr, nullptr );
	String HeaderFilePath = {};
	if (file != nullptr)
	{
		CXString clangFilePath = clang_File_tryGetRealPathName(file);
		const char* filePathMem = clang_getCString(clangFilePath);
		HeaderFilePath.CopyOfCStr(filePathMem, allocator);
		meFsNormalizePathSeperators(HeaderFilePath);
		clang_disposeString(clangFilePath);
	}
	return HeaderFilePath;
}

void clang_checkDiagnostics(CXTranslationUnit& tu)
{
	size_t num_diagnostics = clang_getNumDiagnostics(tu);
	if (num_diagnostics > 0)
	{
		LOG_INFO("Clang diagnostics:");
	}
    for (size_t i = 0; i < num_diagnostics; ++i) 
	{
        CXFile file;
        unsigned line;
        unsigned column;
        unsigned offset;
        CXDiagnostic diagnostic = clang_getDiagnostic(tu, i);
        CXSourceLocation location = clang_getDiagnosticLocation(diagnostic);
        clang_getExpansionLocation(location, &file, &line, &column, &offset);
        LOG_WARN("line %u:%u %s",line, column, clang_getCString(clang_getDiagnosticSpelling(diagnostic)));
        clang_disposeDiagnostic(diagnostic);
    }
}

// exclude stuff that isn't in our project tree, and 3rd party libs
bool IsHeaderWeCareAbout(StringView headerPath, StringView projectRootDir)
{
	if (!headerPath || !projectRootDir)
	{
		return false;
	}
	// assumed these are both absolute paths for simplicity
	bool inProjDir = FindInString(headerPath, projectRootDir) != -1;
	s32 is3rdPartyLib = FindInString(headerPath, STRING_LIT("/external/")) != -1;
	return inProjDir && !is3rdPartyLib;
}

void GetReflectedTypeHashes(
	CXCursor cr, 
	meAllocator* allocator, 
	ClangParsingContext& ctx,
	u32& headerID,
	u32& nameID)
{
	String headerPath = GetHeaderPathForCursor(cr, allocator);
	headerID = HashBytes((u8*)headerPath.data, headerPath.len);
	CXType type = clang_getCursorType(cr);
	// for struct fields, the cursor will be the attribute, parent will be the fielddecl, and parentparent will be the structdecl
	const char* typeStr = clang_getCString(clang_getTypeSpelling(type));
	nameID = HashBytes((u8*)typeStr, CStringLength(typeStr));
}

meReflectedType* TryGetReflectedType(CXCursor cr, meAllocator* allocator, ClangParsingContext& ctx)
{
	u32 headerID = 0;
	u32 nameID = 0;
	GetReflectedTypeHashes(cr, allocator, ctx, headerID, nameID);
	if (!ctx.reflectedFiles.count(headerID) || !ctx.reflectedFiles[headerID].reflectedTypes.count(nameID))
	{
		return nullptr;
	}
	meReflectedType& reflType = ctx.reflectedFiles[headerID].reflectedTypes[nameID];
	if (!reflType.children)
	{
		reflType.children = DynArrayCreate<meReflectedType*>(allocator);
	}
	String headerPath = GetHeaderPathForCursor(cr, allocator);
	ctx.reflectedFiles[headerID].fileName = headerPath;
	return &reflType;
}

bool IsPrimitiveType(CXTypeKind kind)
{
	return kind >= CXType_FirstBuiltin && kind <= CXType_LastBuiltin;
}

bool IsPrimitiveType(CXCursor cr)
{
	CXType type = clang_getCursorType(cr);
	CXCursor typeDecl = clang_getTypeDeclaration(type);
	CXType underlyingtype = clang_getCursorType(typeDecl);
	
	CXTypeKind kind = underlyingtype.kind;

	if (kind == CXType_Typedef)
	{
		kind = clang_getTypedefDeclUnderlyingType(typeDecl).kind;
	}
	return IsPrimitiveType(kind) || IsPrimitiveType(type.kind);
}

meTypeDescriptor* MapClangPrimitiveTypeToTypeDescriptor(CXCursor cr)
{
	CXType type = clang_getCursorType(cr);
	CXTypeKind kind = type.kind;
	if (kind == CXType_Typedef)
	{
		kind = clang_getTypedefDeclUnderlyingType(cr).kind;
	}
	static std::unordered_map<CXTypeKind, meTypeDescriptor*> clangToMePrimitiveType =
	{
		{ CXType_Bool, &TD_BOOL },
		{ CXType_Char_U, &TD_UNSIGNED_CHAR},
		{ CXType_UChar, &TD_UNSIGNED_CHAR},
		{ CXType_Char16, &TD_SHORT},
		{ CXType_Char32, &TD_INT},
		{ CXType_UShort, &TD_UNSIGNED_SHORT},
		{ CXType_UInt, &TD_UNSIGNED_INT},
		{ CXType_ULong, &TD_UNSIGNED_LONG},
		{ CXType_ULongLong, &TD_UNSIGNED_LONGLONG},
		{ CXType_Char_S, &TD_CHAR},
		{ CXType_SChar, &TD_CHAR},
		{ CXType_WChar, &TD_WCHAR},
		{ CXType_Short, &TD_SHORT},
		{ CXType_Int, &TD_INT},
		{ CXType_Long, &TD_LONG},
		{ CXType_LongLong, &TD_LONGLONG},
		{ CXType_Float, &TD_FLOAT},
		{ CXType_Double, &TD_DOUBLE},
	};
	meTypeDescriptor* result = clangToMePrimitiveType.at(kind);
	return result;
}

meReflectedType& GetReflectedType(CXCursor cr, meAllocator* allocator, ClangParsingContext& ctx)
{
	u32 headerID = 0;
	u32 nameID = 0;
	GetReflectedTypeHashes(cr, allocator, ctx, headerID, nameID);
	meReflectedType& reflType = ctx.reflectedFiles[headerID].reflectedTypes[nameID];
	if (!reflType.children)
	{
		reflType.children = DynArrayCreate<meReflectedType*>(allocator);
	}
	String headerPath = GetHeaderPathForCursor(cr, allocator);
	ctx.reflectedFiles[headerID].fileName = headerPath;
	if (IsPrimitiveType(cr)) // if we are registering a primitive type for the first time
	{
		meTypeDescriptor* typeDesc = MapClangPrimitiveTypeToTypeDescriptor(cr);
		reflType = *typeDesc;
	}
	return reflType;
}

// when we encounter a MEREFLECT macro, the entire content inside it is passed in here
// I.E. MEREFLECT(something, another)     "something, another" would be passed in
// In that case ^ the cursor points to either a fielddecl or structdecl that has been annotated
// This also processes fielddecls that won't have the macro on them. For those,
// the macroContent is empty and the cursor points to the fielddecl.
void StoreReflectedTypeInfo(
	CXCursor cr, 
	ClangParsingContext& ctx, 
	StringView macroContent)
{
	meAllocator* allocator = ctx.allocator;

	bool excluded = false;
	if (macroContent)
	{
		if (FindInString(macroContent, STRING_LIT("exclude")) > -1)
		{
			excluded = true;
		}
	}
	if (excluded) return;

	CXCursorKind crKind = clang_getCursorKind(cr);
	CXCursor parentCr = clang_getCursorLexicalParent(cr);
	StringView cursorName = GetCursorDisplayName(cr, allocator);
	CXType crType = clang_getCursorType(cr);

	meReflectedType* reflTypePtr = nullptr;
	// for fields, fill out the inner type, and add this type to the parent struct's children
	if (crKind == CXCursor_FieldDecl)
	{
		CXCursor fieldTypeCr = clang_getTypeDeclaration(crType);
		// annotated fields have their parentCr as the fielddecl. Unannotated fields have their parentCr as the struct decl
		if (fieldTypeCr.kind == CXCursor_NoDeclFound && IsPrimitiveType(crType.kind))
		{
			fieldTypeCr = cr;
		}
		else if (!IsPrimitiveType(fieldTypeCr))
		{
			StoreReflectedTypeInfo(fieldTypeCr, ctx, {});
		}
		meReflectedType& fieldTypeRefl = GetReflectedType(fieldTypeCr, allocator, ctx);
		meReflectedType& fieldMemberRefl = *MENEW(ctx.allocator, meReflectedType); // this will contain the field's type info
		
		meReflectedType& parentReflType = GetReflectedType(parentCr, allocator, ctx);
		DynArrayPush(parentReflType.children, &fieldMemberRefl);
		fieldMemberRefl.innerType = &fieldTypeRefl;
		reflTypePtr = &fieldMemberRefl;
	}
	else
	{
		reflTypePtr = &GetReflectedType(cr, allocator, ctx);
	}
	ME_ASSERT(reflTypePtr);
	meReflectedType& reflType = *reflTypePtr;

	if (reflType.kind != CXCursor_NoDeclFound) // already parsed this type
	{
		return;
	}
	CXType parentType = clang_getCursorType(parentCr);

	s64 typeSize = clang_Type_getSizeOf(crType);
	s64 typeAlign = clang_Type_getAlignOf(crType);
	const char* fieldName = CStringFromString(cursorName, allocator);
	s64 offset = clang_Type_getOffsetOf(parentType, fieldName);
	if (offset < 0)
	{
		// invalid offset, happens when reflecting on a struct type, since the struct decl itself has no offset
		// so, this is only valid when we're reflecting on a *field inside* a struct, which only happens
		// when we add additional reflection markup on a field, like a tooltip, description, exclusion, etc
		// not a fatal error, just documenting this quirk
	}
	reflType.name = cursorName;
	reflType.size = typeSize;
	reflType.offsetBits = offset;
	reflType.align = typeAlign;
	reflType.kind = crKind;

	// if there's an annotation, parse the content
	auto GetStringParam = []
		(StringView key, StringView macroContent) -> StringView
	{
		s32 paramContent = FindInString(macroContent, key, 0, StringOpFlags_IdxAfterNeedle | StringOpFlags_CaseInsensitive);
		if (paramContent < 0)
		{
			return {};
		}
		StringView param = macroContent.OffsetView(paramContent);
		param = EatChars(param, ' ');
		param = EatChars(param, '=');
		param = EatChars(param, ' ');
		ME_ASSERT(param[0] == '"');
		param = EatChars(param, '"');
		s32 endParamStrContent = EatCharsOffset(param, '"', true);
		StringView paramStrContent = param.OffsetView(0, endParamStrContent);
		return paramStrContent;
	};

	if (macroContent)
	{
		StringView descriptionParam = GetStringParam(STRING_LIT("Description"), macroContent);
		reflType.editorName = descriptionParam;

		StringView tooltipParam = GetStringParam(STRING_LIT("Tooltip"), macroContent);
		reflType.tooltip = tooltipParam;

		reflType.isExcluded = excluded;
	}
}

StringView ParseReflectionMacroContent(CXCursor cr, CXTranslationUnit& tu)
{
	CXSourceRange range = clang_getCursorExtent(cr);
	CXSourceLocation start = clang_getRangeStart(range);
		
	CXFile file; unsigned line; unsigned col; unsigned offset;
	clang_getExpansionLocation(start, &file, &line, &col, &offset);

	u64 filesize = 0;
	const char* filecontentCStr = clang_getFileContents(tu, file, &filesize);
	StringView filecontent = { filecontentCStr, filesize };

	StringView startContent = filecontent.OffsetView(offset);

	s32 macroContentStartIdx = FindInString(startContent, STRING_LIT(ME_MACRO_STRINGIZE(MEREFLECT) "("), 0, StringOpFlags_IdxAfterNeedle);
	if (macroContentStartIdx < 0)
	{
		// fields with no annotations...
		return {};
	}
	StringView macroContentStart = startContent.OffsetView(macroContentStartIdx);
	s32 macroContentEndIdx = FindInString(macroContentStart, STRING_LIT(")"));
	ME_ASSERT(macroContentEndIdx > -1);
	StringView macroContents = StringView(macroContentStart.data, macroContentEndIdx);
	// A reasonable limitation??? If we want this, would be simple to count parenthesis...
	ME_ASSERT(FindInString(macroContents, STRING_LIT("(")) == -1 && "you cannot use parenthesis inside a reflection macro");
	
	return macroContents;
}

CXVisitorResult VisitStructureFields(CXCursor cr, CXClientData clientData);

// a decl we care about parsing/storing in the reflection data
void OnFindInterestingDecl(CXCursor cr, CXCursor parent, CXClientData clientData)
{
	#if !defined(ME_REFLECT_ATTR_STR) || !defined(MEREFLECT)
	#error Undefined MEREFLECT attribute str/macro... include me_defines.h
	#endif
	ClangParsingContext& ctx = *(ClangParsingContext*)clientData;
	StringView cursorName = GetCursorDisplayName(cr, ctx.allocator);
	// when the cursor is the reflection attribute, the parent cursor is the one with the actual decl we care about
	if (cursorName == STRING_LIT(ME_REFLECT_ATTR_STR))
	{
		StringView macroContents = ParseReflectionMacroContent(parent, *ctx.tu);
		StoreReflectedTypeInfo(parent, ctx, macroContents);

		// since this is called for a structure decl, OR on a fielddecl...
		CXCursorKind parentKind = clang_getCursorKind(parent);
		if (parentKind == CXCursor_StructDecl || parentKind == CXCursor_ClassDecl)
		{
			CXType parentType = clang_getCursorType(parent);
			clang_Type_visitFields(parentType, VisitStructureFields, clientData);
		}
	}
	else
	{
		// otherwise, cr is the cursor we care about
		StoreReflectedTypeInfo(cr, ctx, {});
	}
}

CXVisitorResult VisitStructureFields(CXCursor cr, CXClientData clientData)
{
	//ClangParsingContext& ctx = *(ClangParsingContext*)clientData;
	CXCursorKind kind = clang_getCursorKind(cr);
	CXCursor parent = clang_getCursorLexicalParent(cr);

	switch (kind)
	{
		case CXCursor_FieldDecl:
		{
			u32 result = clang_visitChildren(cr, +[](CXCursor cr, CXCursor parent, CXClientData clientData)
			{
				CXCursorKind kind = clang_getCursorKind(cr);
				switch (kind)
				{
					case CXCursor_AnnotateAttr:
					{
						OnFindInterestingDecl(cr, parent, clientData);
						return CXChildVisit_Break; // indicates to our code just below that we found an attribute, and already parsed this decl
					}
					break;
					default: break;
				}
				return CXChildVisit_Continue;
			}, clientData);
			if (result == 0)
			{
				// this means we didn't find an annotation on the field, and have yet to parse it
				OnFindInterestingDecl(cr, parent, clientData);
			}
		}
		break;
		default: break;
	}

	return CXVisit_Continue;
}

CXChildVisitResult visitTranslationUnit(CXCursor cr, CXCursor parent, CXClientData clientData)
{
	ClangParsingContext& ctx = *(ClangParsingContext*)clientData;
	Arena* allocator = ctx.allocator;
	CXCursorKind const kind = clang_getCursorKind(cr);
	String headerPath = GetHeaderPathForCursor(cr, allocator);
	if (!IsHeaderWeCareAbout(headerPath, ctx.projectRootDir))
	{
		return CXChildVisit_Continue;
	}
	StringView cursorName = GetCursorDisplayName(cr, ctx.allocator);
	if (cursorName.len == 0)
	{
		return CXChildVisit_Continue;
	}

	switch (kind)
	{
		case CXCursor_AnnotateAttr:
		{
			CXCursorKind parentKind = clang_getCursorKind(parent);
			// finding an annotated struct
			if (parentKind == CXCursor_StructDecl)
			{
				OnFindInterestingDecl(cr, parent, clientData);
			}
		}
		break;

		case CXCursor_EnumDecl:
		{
			//TODO
			return CXChildVisit_Continue;
		}
		break;

		// other potential TODOs to support...
		// - reflected structs inside namespaces!
		// - templated reflection???

		default:
		{
			return CXChildVisit_Recurse;
		}
	}
	return CXChildVisit_Recurse;
}

// each command object has info about it's working dir, 
// a list of cmd args (first arg being the compiler exe itself) or the full command string
// the file it was invoked on
// and the output file the command resulted in (optional)
struct CompileCommand
{
	StringView inFile = {};
	StringView workingDir = {};
	StringView arguments = {};
};

DynArray(CompileCommand) CompileDatabaseToCommandsList(
	meAllocator* allocator,
	StringView compileDatabasePath)
{
	CompileCommand* cmds = DynArrayCreate<CompileCommand>(allocator);
	OSFileReference compileCmdsFile = {};
	if (!meOSOpenFile(compileCmdsFile, (char*)compileDatabasePath, OnlyIfExists))
	{
		LOG_ERROR("Failed to open compile commands database file %s", compileDatabasePath.data);
		return cmds;
	}
	u64 fileSize = meOSGetFileSize(compileCmdsFile);
	char* compileCmdsContentsMem = (char*)MEALLOC(allocator, fileSize);
	StringView compileCmdsContentsStr = StringView(compileCmdsContentsMem, fileSize);
	if (!meOSReadFileContents(compileCmdsFile, compileCmdsContentsMem, fileSize))
	{
		LOG_ERROR("Failed to read compile commands database file %s", compileDatabasePath.data);
		return cmds;
	}
	CompileCommand cmd;
	s32 idx = FindInString(compileCmdsContentsStr, STRING_LIT(" "));
	cmd.arguments = compileCmdsContentsStr.OffsetView(idx + 1);
	s32 endFileIdx = FindInString(compileCmdsContentsStr, STRING_LIT(".cpp")) + 4;
	s32 startFileIdx = FindInStringRev(compileCmdsContentsStr, STRING_LIT(" "), compileCmdsContentsStr.len - endFileIdx)+1;
	cmd.inFile = compileCmdsContentsStr.OffsetView(startFileIdx, endFileIdx - startFileIdx);
	DynArrayPush(cmds, cmd);
	return cmds;
}

void GeneratedReflectionHeaders(ClangParsingContext& ctx, const char* headerOutputFolder);

// ============================================================================
// ABOVE: Parsing the clang translation unit for reflection-annotated types & gathering the data
// ================================================================================

int main(int argc, char* argv[])
{
	InitializeLogger();
	if (argc < 4)
	{
		LOG_ERROR(
		"Not enough args...\n"
		"Arg1 should be path to compile_command.txt\n"
		"Arg2 should be the project's root directory\n"
		"Arg3 should be the output directory for generated header files"
		);
		return 1;
	}
	StringView compileCmdsDatabaseFilePath = StringFromCString(argv[1]);
	LOG_INFO("[Mindseye Reflector] Reflecting %.*s\n", STRING_VAARGS(compileCmdsDatabaseFilePath));
	const char* projectRootDir = argv[2];
	const char* headerOutputFolder = argv[3];

	meAllocator* systemAllocator = GetSystemAllocator();
	Arena& reflectorArena = *MENEW(systemAllocator, Arena);
	reflectorArena = ArenaInit(MEGABYTES_BYTES(100ull), "Main reflector arena", systemAllocator);
	// create a header file on disk that is a sort of "unity" build single file that includes all the files we want to run our reflection parser on
	DynArray(CompileCommand) compileCommands = CompileDatabaseToCommandsList(&reflectorArena, compileCmdsDatabaseFilePath);
	if (DynArrayGetSize(compileCommands) == 0)
	{
		LOG_WARN("[Reflector] No compile commands found");
		return 0;
	}

	const char* reflectorHeaderFilename = "Reflector.h";
	char* reflectorFilePath = DynArrayCreate<char>(&reflectorArena, 50);
	char* exePath = meOSGetExeFileFolder();
	u32 exePathLen = CStringLength(exePath);
	// we expect Reflector.h to be next to the reflector executable
	DynArrayPush(reflectorFilePath, exePath, exePathLen);
	if (exePath[exePathLen - 1] != '\\' && exePath[exePathLen - 1] != '/') DynArrayPush(reflectorFilePath, '\\');
	DynArrayPush(reflectorFilePath, (char*)reflectorHeaderFilename, CStringLength(reflectorHeaderFilename));
	LOG_INFO("Reflector file: %s", reflectorFilePath);
	auto idx = clang_createIndex(0, 1);
	u32 clangOptions = 0
		| CXTranslationUnit_DetailedPreprocessingRecord
		| CXTranslationUnit_SkipFunctionBodies
		| CXTranslationUnit_IncludeBriefCommentsInCodeCompletion
		//| CXTranslationUnit_KeepGoing
		//| CXTranslationUnit_SingleFileParse
		;

	DynArray(const char*) clangArgs = DynArrayCreate<const char*>(&reflectorArena, 20);
	u32 numCompileCommands = DynArrayGetSize(compileCommands);
	for (u32 i = 0; i < numCompileCommands; i++)
	{
		s32 stridx = 0; 
		StringView args = compileCommands[i].arguments;
		while (stridx < args.len && stridx >= 0)
		{
			StringView currentArgsView = args.OffsetView(stridx);
			stridx += EatCharsOffset(currentArgsView, ' ');
			s32 nextSpace = FindInString(args, STRING_LIT(" "), stridx);
			if (nextSpace < 0) break;
			s32 len = nextSpace - stridx;
			StringView subArg = args.OffsetView(stridx, len);

			bool isDefineFlag = FindInString(subArg, STRING_LIT("-D")) != -1;
			bool isIncludeDir = FindInString(subArg, STRING_LIT("-I")) != -1;
			bool isStdVer = FindInString(subArg, STRING_LIT("-std")) != -1;
			if (isDefineFlag || isIncludeDir || isStdVer)
			{
				void* strmem = MEALLOC(&reflectorArena, len + 1);
				ME_MEMCLEAR(strmem, len + 1);
				ME_MEMCPY(strmem, args.data + stridx, len);
				DynArrayPush(clangArgs, (const char*)strmem);
			}
			stridx = nextSpace;
		}
	}

	DynArrayPush(clangArgs, "-x");
	DynArrayPush(clangArgs, "c++");

	CXTranslationUnit tu;
	CXErrorCode result = CXError_Failure;
	{
		result = clang_parseTranslationUnit2(idx, reflectorFilePath, clangArgs, DynArrayGetSize(clangArgs), 0, 0, clangOptions, &tu);
	}
	ClangParsingContext& ctx = ClangParsingContext::GetSingleInstance();
	ctx.tu = &tu;
	ctx.allocator = &reflectorArena;
	String absProjectRootPath = meOSResolveRelativeToAbsPath(&reflectorArena, StringFromCString(projectRootDir));
	ctx.projectRootDir = absProjectRootPath;
	meFsNormalizePathSeperators(ctx.projectRootDir);
	//clang_checkDiagnostics(tu);
	if (result == CXError_Success)
	{
		auto cursor = clang_getTranslationUnitCursor(tu);
		// populates the parsingcontext with info about all reflected types
		clang_visitChildren(cursor, visitTranslationUnit, &ctx);
		GeneratedReflectionHeaders(ctx, headerOutputFolder);
	}
	else
	{
		switch (result)
		{
			case CXError_Failure:
                LOG_ERROR("Clang Unknown failure");
                break;

			case CXError_Crashed:
                LOG_ERROR("Clang crashed");
                break;

			case CXError_InvalidArguments:
                LOG_ERROR("Clang Invalid arguments");
                break;

			case CXError_ASTReadError:
                LOG_ERROR("Clang AST read error");
                break;
			default:
			{
				LOG_INFO("Clang AST read success");
			} 
			break;
		}
	}
	clang_disposeIndex(idx);

	return 0;
}

// ===================================================================
// BELOW: Outputting generated headers from the reflection data we captured
// ====================================================================

void ProcessReflectedFile(
	const meReflectedFile& fileRefl, 
	const char* headerOutputFolder,
	meAllocator* allocator);

// given the ClangParsingContext that is filled with all the relevant data, generate headers
void GeneratedReflectionHeaders(
	ClangParsingContext& ctx, 
	const char* headerOutputFolder)
{
	LOG_INFO("Generating reflection headers at %s", headerOutputFolder);
	u32 numReflectedFiles = ctx.reflectedFiles.size();
	for (const auto& [headerID, fileReflection] : ctx.reflectedFiles)
	{
		// TODO: this is ripe for super easy parallelism here
		// chunk up allocators for each thread, and have them all generate & write out each header
		Arena fileArena = ArenaInit(ArenaGetFreeSpace(ctx.allocator) / numReflectedFiles, "File Reflection Arena", ctx.allocator);
		ProcessReflectedFile(fileReflection, headerOutputFolder, &fileArena);
	}
}


void ProcessReflectedFile(
	const meReflectedFile& fileRefl,
	const char* headerOutputFolder,
	meAllocator* allocator)
{
	StringBuilder headerContentBuilder = StringBuilder(allocator, MEGABYTES_BYTES(1));

	StringView parsedHeaderExistingPath = fileRefl.fileName;
	if (!parsedHeaderExistingPath)
	{
		return;
	}
	StringView parsedHeaderFilename = meFsGetFilepathFromPath(parsedHeaderExistingPath);

	headerContentBuilder.Append(STRING_LIT("// ====== THIS FILE IS AUTOGENERATED =======\n"));
	// for simplicity, if anyone wants access to the reflection data for some type, they shouldn't be including
	// the actual header, not the generated one. The generated one should be included by the file it reflects
	headerContentBuilder.AppendFormat("// ====== THIS FILE SHOULD ONLY BE INCLUDED BY %.*s =======\n", STRING_VAARGS(parsedHeaderFilename));
	headerContentBuilder.Append(STRING_LIT("#pragma once\n"));
	headerContentBuilder.Append(STRING_LIT("#include \"reflector/reflection_types.h\"\n"));

	for (const auto& [nameID, typeRefl] : fileRefl.reflectedTypes)
	{
		if (typeRefl.isExcluded) continue;
		if (typeRefl.kind == CXCursor_StructDecl)
		{
			headerContentBuilder.AppendFormat("struct %.*s;\n", STRING_VAARGS(typeRefl.name));
			u32 numChildren = DynArrayGetSize(typeRefl.children);
			if (numChildren > 0)
			{
				headerContentBuilder.AppendFormat("extern meTypeDescriptor g_%.*s_fields[%i];\n", STRING_VAARGS(typeRefl.name), numChildren);
			}
			headerContentBuilder.AppendFormat("extern meTypeDescriptor g_%.*s_typedescriptor;\n", STRING_VAARGS(typeRefl.name));
		}
	}

	StringView fileContent = headerContentBuilder;
	StringView parsedHeaderFilenameNoExt;
	if (fileContent)
	{
		OSFileReference headerFile = {};
		s32 extensionIdx = FindInStringRev(parsedHeaderFilename, STRING_LIT("."));
		parsedHeaderFilenameNoExt = parsedHeaderFilename.OffsetView(0, extensionIdx);
		const char* dstHeaderFilePath = StringFormat("%s/%.*s.generated.h", headerOutputFolder, STRING_VAARGS(parsedHeaderFilenameNoExt));
		meOSEnsureDirectoriesExist(headerOutputFolder);
		if (!meOSOpenFile(headerFile, dstHeaderFilePath))
		{
			LOG_ERROR("Failed to open file %s while trying to generated reflected headers", dstHeaderFilePath);
			return;
		}
		// write to the file here
		meOSWriteFileContent(headerFile, fileContent.data, fileContent.len);
		meOSCloseFile(headerFile);
	}

	StringBuilder sourceContentBuilder = StringBuilder(allocator, MEGABYTES_BYTES(1));
	sourceContentBuilder.Append(STRING_LIT("// ====== THIS FILE IS AUTOGENERATED =======\n"));
	sourceContentBuilder.AppendFormat("#include \"%.*s.generated.h\"\n", STRING_VAARGS(parsedHeaderFilenameNoExt));

	for (const auto& [nameID, typeRefl] : fileRefl.reflectedTypes)
	{
		if (typeRefl.isExcluded) continue;
		if (typeRefl.kind == CXCursor_StructDecl)
		{
			u32 numChildren = DynArrayGetSize(typeRefl.children);
			if (numChildren > 0)
			{
				StringBuilder fieldsArrayContent = StringBuilder(allocator);
				for (s32 i = 0; i < numChildren; i++)
				{
					meReflectedType& childReflType = *typeRefl.children[i];
					fieldsArrayContent.Append(STRING_LIT("\t{"));
					fieldsArrayContent.AppendFormat(".name = STRING_LIT(\"%.*s\"), ", STRING_VAARGS(childReflType.name));
					if (childReflType.editorName) fieldsArrayContent.AppendFormat(".editorName = STRING_LIT(\"%.*s\"), ", STRING_VAARGS(childReflType.editorName));
					if (childReflType.tooltip) fieldsArrayContent.AppendFormat(".tooltip = STRING_LIT(\"%.*s\"), ", STRING_VAARGS(childReflType.tooltip));
					fieldsArrayContent.AppendFormat(".offset = %i, ", childReflType.offsetBits / 8);
					if (childReflType.innerType && childReflType.innerType->name)
					{
						String underlyingTD = String(childReflType.innerType->name, allocator);
						ToUpper(underlyingTD);
						StringReplace(underlyingTD, ' ', '_');
						fieldsArrayContent.AppendFormat(".underlyingType = &TD_%.*s, ", STRING_VAARGS(underlyingTD));
					}
					fieldsArrayContent.Append(STRING_LIT("},"));
					if (i != numChildren-1)
					{
						fieldsArrayContent.Append(STRING_LIT("\n"));
					}
				}				
				sourceContentBuilder.AppendFormat("meTypeDescriptor g_%.*s_fields[%i] = {\n%.*s\n};\n", STRING_VAARGS(typeRefl.name), numChildren, STRING_VAARGS(fieldsArrayContent));
			}
			StringBuilder mainTypeDescriptorContent = StringBuilder(allocator);
			mainTypeDescriptorContent.AppendFormat("\t.name = STRING_LIT(\"%.*s\"),\n", STRING_VAARGS(typeRefl.name));
			if (typeRefl.editorName) mainTypeDescriptorContent.AppendFormat("\t.editorName = STRING_LIT(\"%.*s\"),\n", STRING_VAARGS(typeRefl.editorName));
			if (typeRefl.tooltip) mainTypeDescriptorContent.AppendFormat("\t.tooltip = STRING_LIT(\"%.*s\"),\n", STRING_VAARGS(typeRefl.tooltip));
			
			mainTypeDescriptorContent.AppendFormat("\t.fields = g_%.*s_fields,\n", STRING_VAARGS(typeRefl.name));
			mainTypeDescriptorContent.AppendFormat("\t.size = %i,\n", typeRefl.size);
			mainTypeDescriptorContent.AppendFormat("\t.align = %i,", typeRefl.align);

			sourceContentBuilder.AppendFormat("meTypeDescriptor g_%.*s_typedescriptor = {\n%.*s\n};\n", STRING_VAARGS(typeRefl.name), STRING_VAARGS(mainTypeDescriptorContent));
		}
	}

	fileContent = sourceContentBuilder;
	if (fileContent)
	{
		OSFileReference sourceFile = {};
		s32 extensionIdx = FindInStringRev(parsedHeaderFilename, STRING_LIT("."));
		StringView parsedSourceFilenameNoExt = parsedHeaderFilename.OffsetView(0, extensionIdx);
		const char* dstFilePath = StringFormat("%s/%.*s.generated.cpp", headerOutputFolder, STRING_VAARGS(parsedSourceFilenameNoExt));
		if (!meOSOpenFile(sourceFile, dstFilePath))
		{
			LOG_ERROR("Failed to open file %s while trying to generated reflected headers", dstFilePath);
			return;
		}
		// write to the file here
		meOSWriteFileContent(sourceFile, fileContent.data, fileContent.len);
		meOSCloseFile(sourceFile);
	}
}

