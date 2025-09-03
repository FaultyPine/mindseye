

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
*/

struct meReflectedType
{
	String name = {};
	u32 size = 0;
	bool isReflected = false;
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
	bool reflecting = false;
	bool reflectionExcluding = false;
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


// exclude stuff that isn't in our project tree, and 3rd party libs
bool IsHeaderWeCareAbout(StringView headerPath, StringView projectRootDir)
{
	if (!headerPath || !projectRootDir)
	{
		return false;
	}
	// assumed these are both absolute paths for simplicity
	bool inProjDir = FindInString(headerPath, projectRootDir, 0, CaseInsensitive) != -1;
	s32 is3rdPartyLib = FindInString(headerPath, STRING_LIT("/external/")) != -1;
	return inProjDir && !is3rdPartyLib;
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

		CXTranslationUnit& tu = *ctx.tu;
		String headerPath = GetHeaderPathForCursor(cr, scratchArena.arena);
		u32 headerID = HashBytes((u8*)headerPath.data, headerPath.len);
		CXType type = clang_getCursorType(parent);
		CXString typeStr = clang_getTypeSpelling(type);
		u32 nameID = HashBytes((u8*)typeStr.data, CStringLength((const char*)typeStr.data));
		if (ctx.reflectedFiles[headerID].reflectedTypes[nameID].isReflected)
		{
			return;
		}

		CXSourceRange range = clang_getCursorExtent(parent);
		CXToken *tokens = nullptr;
		u32 numTokens = 0;
		clang_tokenize(tu, range, &tokens, &numTokens);
		// if needed, this can map tokens to cursors if we need bidirectional translation between source <-> ast
		//clang_annotateTokens(tu, tokens, numTokens, cursors);
		DynArray(char) reflectMacroContent = DynArrayCreate<char>(ctx.allocator);
		bool inReflectionMacroTokens = false;
		for (u32 i = 0; i < numTokens; i++) 
		{
			CXToken token = tokens[i];
			CXString tokenSpelling = clang_getTokenSpelling(tu, token);
			const char* tokenCstr = clang_getCString(tokenSpelling);
			if (!inReflectionMacroTokens &&
				FindInString(StringFromCString(tokenCstr), STRING_LIT(ME_MACRO_STRINGIZE(MEREFLECT))) >= 0)
			{
				inReflectionMacroTokens = true;
				i++;
			}
			else if (inReflectionMacroTokens)
			{
				if (FindInString(StringFromCString(tokenCstr), STRING_LIT(")")) >= 0)
				{
					break;
				}
				else
				{
					DynArrayPush(reflectMacroContent, (char*)tokenCstr, CStringLength(tokenCstr));
				}
			}
		}
		LOG_INFO("reflection macro content: %.*s", STRING_VAARGS(String(reflectMacroContent, DynArrayGetSize(reflectMacroContent))));
		clang_disposeTokens(tu, tokens, numTokens);

		ctx.reflectedFiles[headerID].reflectedTypes[nameID].isReflected = true;
		{
			LOG_INFO("reflected structure %s", clang_getCString(typeStr));
			CXType type = clang_getCursorType(parent);
			clang_Type_visitFields(type, VisitStructureFields, clientData);
		}
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
		case CXCursor_AnnotateAttr:
		{
			OnFindAnnotated(cr, clang_getCursorLexicalParent(cr), clientData);
		}
		break;
		default: break;
	}

	// TODO:
	// process field itself and any extra potential reflection annotation data on the field here
	LOG_INFO("\t%.*s", STRING_VAARGS(cursorName));

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
	String compileDatabasePath)
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
	cmd.arguments = compileCmdsContentsStr.CreateView(idx + 1);
	s32 endFileIdx = FindInString(compileCmdsContentsStr, STRING_LIT(".cpp")) + 4;
	s32 startFileIdx = FindInStringRev(compileCmdsContentsStr, STRING_LIT(" "), compileCmdsContentsStr.len - endFileIdx)+1;
	cmd.inFile = compileCmdsContentsStr.CreateView(startFileIdx, endFileIdx - startFileIdx);
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
	const char* compileCmdsDatabaseFilePath = argv[1];
	LOG_INFO("[Mindseye Reflector] Reflecting %s\n", compileCmdsDatabaseFilePath);
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
			StringView currentArgsView = args.CreateView(stridx);
			stridx += EatCharsOffset(currentArgsView, ' ');
			s32 nextSpace = FindInString(args, STRING_LIT(" "), stridx);
			if (nextSpace < 0) break;
			s32 len = nextSpace - stridx;
			StringView subArg = args.CreateView(stridx, len);

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