

#define ME_CORE_ONLY
#include "mindseye/me_unity.cpp"

#include "clang/AST/AST.h"
#include "clang/AST/Type.h"
#include "clang-c/Index.h"

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

struct meReflectedType
{
	DynArray(meReflectedType*) children = {};
	String name = {};
	String editorName = {};
	String tooltip = {};
	u32 size = 0;
	s32 offset = 0;
	u32 align = 0;
	bool isReflected = false;
	bool isExcluded = false;
	void Print() const;
	bool operator==(const meReflectedType& other) const
	{
		return name == other.name &&
			size == other.size &&
			offset == other.offset &&
			align == other.align;
	}
};

struct meReflectedFile
{
	// name hash -> type
	meMap<u32, meReflectedType> reflectedTypes = {};
};

struct ClangParsingContext
{
	Arena* allocator = nullptr;
	String projectRootDir = {};
	Stack<String> namespaces = {};
	// header id -> file reflection info
	meMap<u32, meReflectedFile> reflectedFiles = {};
	u32 errorCode = 0;
	CXTranslationUnit* tu = nullptr;
	bool HasErrorOccurred() const { return errorCode != 0; }

	static ClangParsingContext& GetSingleInstance()
	{
		static ClangParsingContext g_parsingContext = {};
		return g_parsingContext;
	}
};

void meReflectedType::Print() const
{
	LOG_INFO("%.*s\n\t[excluded = %i] [editorName = %.*s] [tooltip = %.*s] offset = %i size = %u align = %u", 
		STRING_VAARGS(name), isExcluded, STRING_VAARGS(editorName), STRING_VAARGS(tooltip), offset, size, align);
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

inline String GetCursorDisplayName(const CXCursor& cr, meAllocator* allocator )
{
	auto displayName = clang_getCursorDisplayName(cr);
	const char* strMem = clang_getCString(displayName);
	u64 strLen = CStringLength(strMem);
	void* ownedMem = MEALLOC(allocator, strLen);
	ME_MEMCPY(ownedMem, strMem, strLen);
	clang_disposeString(displayName);
	return String((char*)ownedMem, strLen);
}

inline u32 GetLineNumberForCursor(const CXCursor& cr)
{
	uint32_t line, column, offset;
	CXSourceRange range = clang_getCursorExtent(cr);
	CXSourceLocation start = clang_getRangeStart(range);
	clang_getExpansionLocation( start, nullptr, &line, &column, &offset );
	return line;
}

void NormalizePathSeperators(StringView str)
{
	for (u32 i = 0; i < str.len; i++)
	{
		if (str.data[i] == '\\')
		{
			str.data[i] = '/';
		}
	}
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
		u32 filePathLen = CStringLength(filePathMem);
		HeaderFilePath = String((const char*)ReallocateBuffer(allocator, (void*)filePathMem, filePathLen).data, filePathLen);
		NormalizePathSeperators(HeaderFilePath);
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
	bool inProjDir = FindInString(headerPath, projectRootDir, 0, StringOpFlags_CaseInsensitive) != -1;
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
	return reflType;
}

// when we encounter a MEREFLECT macro, the entire content inside it is passed in here
// I.E. MEREFLECT(something, another)     "something, another" would be passed in
void StoreReflectedTypeInfo(
	CXCursor parent, 
	ClangParsingContext& ctx, 
	StringView macroContent)
{
	meAllocator* allocator = ctx.allocator;
	meReflectedType& reflType = GetReflectedType(parent, allocator, ctx);

	if (reflType.isReflected) // already parsed this type
	{
		return;
	}
	CXType parentType = clang_getCursorType(parent);
	String cursorParentName = GetCursorDisplayName(parent, allocator);
	CXType parentParentType = clang_getCursorType(clang_getCursorLexicalParent(parent));

	reflType.isReflected = true;
	s64 typeSize = clang_Type_getSizeOf(parentType);
	s64 typeAlign = clang_Type_getAlignOf(parentType);
	const char* fieldName = CStringFromString(cursorParentName, allocator);
	s64 offset = clang_Type_getOffsetOf(parentParentType, fieldName);
	if (offset < 0)
	{
		// invalid offset, happens when reflecting on a struct type, since the struct decl itself has no offset
		// so, this is only valid when we're reflecting on a *field inside* a struct, which only happens
		// when we add additional reflection markup on a field, like a tooltip, description, exclusion, etc
		// not a fatal error, just documenting this quirk
	}
	reflType.name = cursorParentName;
	reflType.size = typeSize;
	reflType.offset = offset;
	reflType.align = typeAlign;

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
		reflType.editorName.CopyOf(descriptionParam, ctx.allocator);

		StringView tooltipParam = GetStringParam(STRING_LIT("Tooltip"), macroContent);
		reflType.tooltip.CopyOf(tooltipParam, ctx.allocator);

		if (FindInString(macroContent, STRING_LIT("exclude")) > -1)
		{
			reflType.isExcluded = true;
		}
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
	String cursorName = GetCursorDisplayName(cr, ctx.allocator);
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
	ClangParsingContext& ctx = *(ClangParsingContext*)clientData;
	Arena* allocator = ctx.allocator;
	String cursorName = GetCursorDisplayName(cr, ctx.allocator);
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

	CXString parentStr = clang_getCursorSpelling(parent);

	meReflectedType& reflType = GetReflectedType(cr, allocator, ctx);
	if (reflType.isExcluded)
	{
		return CXVisit_Continue;
	}
	meReflectedType& parentReflType = GetReflectedType(parent, allocator, ctx);

	// add this field to its parent structs children
	bool childAlreadyThere = false;
	for (s32 i = 0; i < DynArrayGetSize(parentReflType.children); i++)
	{
		if (*parentReflType.children[i] == reflType)
		{
			childAlreadyThere = true;
			break;
		}
	}
	if (!childAlreadyThere)
	{
		DynArrayPush(parentReflType.children, &reflType);
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
	String cursorName = GetCursorDisplayName(cr, ctx.allocator);
	if (cursorName.len == 0)
	{
		return CXChildVisit_Continue;
	}

	switch (kind)
	{
		case CXCursor_AnnotateAttr:
		{
			OnFindInterestingDecl(cr, parent, clientData);
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
	if (!meOSOpenFile(compileCmdsFile, compileDatabasePath, OnlyIfExists))
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

void ProcessReflectedTypes(ClangParsingContext& ctx);

int main(int argc, char* argv[])
{
	InitializeLogger();
	if (argc < 2)
	{
		LOG_ERROR("Invalid args, need to pass path to compile_commands.json\n");
		return 1;
	}
	StringView compileCmdsDatabaseFilePath = StringFromCString(argv[1]);
	LOG_INFO("[Mindseye Reflector] Reflecting %.*s\n", STRING_VAARGS(compileCmdsDatabaseFilePath));
	const char* projectRootDir = argv[2];
	meAllocator* systemAllocator = GetSystemAllocator();
	Arena reflectorArena = ArenaInit(MEGABYTES_BYTES(100ull), "Main reflector arena", systemAllocator);
	// create a header file on disk that is a sort of "unity" build single file that includes all the files we want to run our reflection parser on
	DynArray(CompileCommand) compileCommands = CompileDatabaseToCommandsList(&reflectorArena, compileCmdsDatabaseFilePath);
	if (DynArrayGetSize(compileCommands) == 0)
	{
		LOG_WARN("[Reflector] No compile commands found");
		return 0;
	}
	// Previously, I was generating this based on compile commands.
	// Honestly though, I think it's better if I manually maintain this file
	// as a list of headers that should be passed through through the reflection system
	// that way i can manually exclude unnecessary stuff easily.
	const char* reflectorHeaderFilename = "Reflector.h";
	char* reflectorFilePath = DynArrayCreate<char>(&reflectorArena, 50);
	char* exePath = meOSGetExeFileFolder();
	u32 exePathLen = CStringLength(exePath);
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
	char* absProjectRootPath = meOSResolveRelativeToAbsPath(&reflectorArena, StringFromCString(projectRootDir));
	ctx.projectRootDir = String(absProjectRootPath, CStringLength(absProjectRootPath));
	NormalizePathSeperators(ctx.projectRootDir);
	clang_checkDiagnostics(tu);
	if (result == CXError_Success)
	{
		auto cursor = clang_getTranslationUnitCursor(tu);
		// populates the parsingcontext with info about all reflected types
		clang_visitChildren(cursor, visitTranslationUnit, &ctx);
		ProcessReflectedTypes(ctx);
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


void ProcessReflectedTypes(ClangParsingContext& ctx)
{
	// BOOKMARK2: take all types and their children and write em out into headers
	for (const auto& [headerID, fileReflection] : ctx.reflectedFiles)
	{
		for (const auto& [nameID, typeRefl] : fileReflection.reflectedTypes)
		{
			if (typeRefl.isExcluded) continue;
			typeRefl.Print();
		}
	}
}
