

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
*/


struct meReflectedType
{
	String name = {};
	DynArray(meReflectedType) children = {};
	String editorName = {};
	String tooltip = {};
	u32 size = 0;
	s32 offset = 0;
	u32 align = 0;
	bool isReflected = false;
	void Print()
	{
		LOG_INFO("%.*s [editorName = %.*s] [tooltip = %.*s] offset = %i size = %u align = %u", STRING_VAARGS(name), STRING_VAARGS(editorName), STRING_VAARGS(tooltip), offset, size, align);
	}
};

struct meReflectedFile
{
	// qualified name hash -> type
	meMap<u32, meReflectedType> reflectedTypes = {};
};

struct ClangParsingContext
{
	Arena* allocator = nullptr;
	String projectRootDir = {};
	Stack<String> namespaces = {};
	meMap<u32, meReflectedFile> reflectedFiles = {};
	u32 errorCode = 0;
	CXTranslationUnit* tu = nullptr;
	bool HasErrorOccurred() const { return errorCode != 0; }
};

inline String GetCursorDisplayName(const CXCursor& cr, Arena* allocator )
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

String GetHeaderPathForCursor(CXCursor cr, Arena* allocator)
{
	CXFile pFile;
	CXSourceRange const cursorRange = clang_getCursorExtent( cr );
	clang_getExpansionLocation( clang_getRangeStart( cursorRange ), &pFile, nullptr, nullptr, nullptr );
	String HeaderFilePath;
	if ( pFile != nullptr )
	{
		CXString clangFilePath = clang_File_tryGetRealPathName( pFile );
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

// when we encounter a MEREFLECT macro, the entire content inside it is passed in here
// I.E. MEREFLECT(something, another)     "something, another" would be passed in
void StoreReflectedTypeInfo(
	CXCursor parent, 
	ClangParsingContext& ctx, 
	StringView macroContent)
{
	if (FindInString(macroContent, STRING_LIT("exclude")) > -1)
	{
		return;
	}
	ArenaTemp scratchArena = ArenaTempInit(ctx.allocator);
	ME_ON_SCOPE_EXIT([&scratchArena]() { ArenaTempEnd(scratchArena); });

	String headerPath = GetHeaderPathForCursor(parent, scratchArena.arena);
	u32 headerID = HashBytes((u8*)headerPath.data, headerPath.len);
	CXType parentType = clang_getCursorType(parent);
	String cursorParentName = GetCursorDisplayName(parent, scratchArena);
	// for struct fields, the cursor will be the attribute, parent will be the fielddecl, and parentparent will be the structdecl
	CXType parentParentType = clang_getCursorType(clang_getCursorLexicalParent(parent));
	const char* parentTypeStr = clang_getCString(clang_getTypeSpelling(parentType));
	u32 nameID = HashBytes((u8*)parentTypeStr, CStringLength(parentTypeStr));
	meReflectedType& reflType = ctx.reflectedFiles[headerID].reflectedTypes[nameID];
	if (reflType.isReflected) // already parsed this type
	{
		return;
	}
	reflType.isReflected = true;
	s64 typeSize = clang_Type_getSizeOf(parentType);
	s64 typeAlign = clang_Type_getAlignOf(parentType);
	const char* fieldName = CStringFromString(cursorParentName, scratchArena);
	s64 offset = clang_Type_getOffsetOf(parentParentType, fieldName);
	if (offset < 0)
	{
		// invalid offset, happens when reflecting on a struct type, since the struct decl itself has no offset
		// so, this is only valid when we're reflecting on a *field inside* a struct, which only happens
		// when we add additional reflection markup on a field, like a tooltip, description, exclusion, etc
	}
	reflType.name = cursorParentName;
	reflType.size = typeSize;
	reflType.offset = offset;
	reflType.align = typeAlign;
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

	StringView descriptionParam = GetStringParam(STRING_LIT("Description"), macroContent);
	reflType.editorName.CopyOf(descriptionParam, ctx.allocator);

	StringView tooltipParam = GetStringParam(STRING_LIT("Tooltip"), macroContent);
	reflType.tooltip.CopyOf(tooltipParam, ctx.allocator);

	reflType.Print();
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
	ME_ASSERT(macroContentStartIdx > -1);
	StringView macroContentStart = startContent.OffsetView(macroContentStartIdx);
	s32 macroContentEndIdx = FindInString(macroContentStart, STRING_LIT(")"));
	ME_ASSERT(macroContentEndIdx > -1);
	StringView macroContents = StringView(macroContentStart.data, macroContentEndIdx);
	// A reasonable limitation??? If we want this, would be simple to count parenthesis...
	ME_ASSERT(FindInString(macroContents, STRING_LIT("(")) == -1 && "you cannot use parenthesis inside a reflection macro");
	
	return macroContents;
}

CXVisitorResult VisitStructureFields(CXCursor cr, CXClientData clientData);

void OnFindAnnotated(CXCursor cr, CXCursor parent, CXClientData clientData)
{
	#if !defined(ME_REFLECT_ATTR_STR) || !defined(MEREFLECT)
	#error Undefined MEREFLECT attribute str/macro... include me_defines.h
	#endif
	ClangParsingContext& ctx = *(ClangParsingContext*)clientData;
	String cursorName = GetCursorDisplayName(cr, ctx.allocator);
	if (cursorName == STRING_LIT(ME_REFLECT_ATTR_STR))
	{
		ArenaTemp scratchArena = ArenaTempInit(ctx.allocator);
		ME_ON_SCOPE_EXIT([&scratchArena]() { ArenaTempEnd(scratchArena); });

		StringView macroContents = ParseReflectionMacroContent(parent, *ctx.tu);
		StoreReflectedTypeInfo(parent, ctx, macroContents);

		// this is called for a structure decl, OR on a fielddecl.
		// for the structure, this visits the fields, as one might intuit
		// for the field decl, this is basically a noop
		CXType parentType = clang_getCursorType(parent);
		clang_Type_visitFields(parentType, VisitStructureFields, clientData);
	}
}

CXVisitorResult VisitStructureFields(CXCursor cr, CXClientData clientData)
{
	ClangParsingContext* ctx = (ClangParsingContext*)clientData;
	Arena* allocator = ctx->allocator;
	ArenaTemp scratchArena = ArenaTempInit(allocator);
	ME_ON_SCOPE_EXIT([&scratchArena]() { ArenaTempEnd(scratchArena); });
	String cursorName = GetCursorDisplayName(cr, ctx->allocator);
	CXCursorKind kind = clang_getCursorKind(cr);
	switch (kind)
	{
		case CXCursor_FieldDecl:
		{
			clang_visitChildren(cr, +[](CXCursor cr, CXCursor parent, CXClientData clientData){
				CXCursorKind kind = clang_getCursorKind(cr);
				switch (kind)
				{
					case CXCursor_AnnotateAttr:
					{
						OnFindAnnotated(cr, parent, clientData);
					}
					break;
					default: break;
				}
				return CXChildVisit_Continue;
			}, clientData);
		}
		break;
		default: break;
	}

	CXCursor parent = clang_getCursorLexicalParent(cr);
	CXString parentStr = clang_getCursorSpelling(parent);
	// TODO:
	// add this field to its parent struct's children
	LOG_INFO("\t%s::%.*s", clang_getCString(parentStr), STRING_VAARGS(cursorName));

	return CXVisit_Continue;
}

CXChildVisitResult visitTranslationUnit(CXCursor cr, CXCursor parent, CXClientData clientData)
{
	ClangParsingContext& ctx = *(ClangParsingContext*)clientData;
	Arena* allocator = ctx.allocator;
	CXCursorKind const kind = clang_getCursorKind(cr);
	ArenaTemp scratchArena = ArenaTempInit(allocator);
	ME_ON_SCOPE_EXIT([&scratchArena]() { ArenaTempEnd(scratchArena); });
	String headerPath = GetHeaderPathForCursor(cr, scratchArena.arena);
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
			OnFindAnnotated(cr, parent, clientData);
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

	DynArrayPush(clangArgs, ( "-x" ));
	DynArrayPush(clangArgs, ( "c++" ));

	CXTranslationUnit tu;
	CXErrorCode result = CXError_Failure;
	{
		result = clang_parseTranslationUnit2( idx, reflectorFilePath, clangArgs, DynArrayGetSize(clangArgs), 0, 0, clangOptions, &tu );
	}
	ClangParsingContext parsingContext;
	parsingContext.tu = &tu;
	parsingContext.allocator = &reflectorArena;
	char* absProjectRootPath = meOSResolveRelativeToAbsPath(&reflectorArena, StringFromCString(projectRootDir));
	parsingContext.projectRootDir = String(absProjectRootPath, CStringLength(absProjectRootPath));
	NormalizePathSeperators(parsingContext.projectRootDir);
	clang_checkDiagnostics(tu);
	if ( result == CXError_Success )
	{
		auto cursor = clang_getTranslationUnitCursor( tu );
		clang_visitChildren( cursor, visitTranslationUnit, &parsingContext );
	}
	else
	{
		switch ( result )
		{
			case CXError_Failure:
                LOG_ERROR( "Clang Unknown failure" );
                break;

			case CXError_Crashed:
                LOG_ERROR( "Clang crashed" );
                break;

			case CXError_InvalidArguments:
                LOG_ERROR( "Clang Invalid arguments" );
                break;

			case CXError_ASTReadError:
                LOG_ERROR( "Clang AST read error" );
                break;
			default:
			{
				LOG_INFO("Clang AST read success");
			} 
			break;
		}
	}
	clang_disposeIndex( idx );

	return 0;
}