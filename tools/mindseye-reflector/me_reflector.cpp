

#define ME_CORE_ONLY
#include "mindseye/me_unity.cpp"

#include "clang/AST/AST.h"
#include "clang/AST/Type.h"
#include "clang-c/Index.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

/*
exclude system libs & stl stuff with ClassFinder & MatchFinder - see https://www.youtube.com/watch?v=XoYVeduK4yI&ab_channel=cpponsea
Should I only reflect reflection annotated types/fields? Or try to do it all?

Generate in-memory representation of the data i care about.
The pass that to a code generator to write out generated headers for mindseye to use.
*/


struct ClangParsingContext
{
	Arena* allocator = nullptr;
	String projectRootDir = {};
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
		clang_disposeString(clangFilePath);
	}
	return HeaderFilePath;
}

bool IsHeaderInDir(StringView headerPath, StringView projectRootDir)
{
	return false;
}

CXChildVisitResult visitTranslationUnit(CXCursor cr, CXCursor parent, CXClientData clientData)
{
	ClangParsingContext& ctx = *(ClangParsingContext*)clientData;
	Arena* allocator = ctx.allocator;
	CXCursorKind const kind = clang_getCursorKind(cr);
	ArenaTemp scratchArena = ArenaTempInit(allocator);
	String cursorDisplayName = GetCursorDisplayName(cr, scratchArena.arena);
	String headerPath = GetHeaderPathForCursor(cr, scratchArena.arena);
	if (!IsHeaderInDir(headerPath, ctx.projectRootDir))
	{
		return CXChildVisit_Continue;
	}
	u32 fileLineNum = GetLineNumberForCursor(cr);
	switch ( kind )
	{
		// Classes / Structs
		case CXCursor_ClassTemplate:
		{
			//ReflectionMacro macro;
			//if ( pContext->GetReflectionMacroForType( headerID, cr, macro ) )
			//{
			//	pContext->LogError( "Cannot register template class (%s)", cursorName.c_str() );
			//	return CXChildVisit_Break;
			//}

			return CXChildVisit_Recurse;
		}
		break;

		// Classes / Structs
		case CXCursor_ClassDecl:
		case CXCursor_StructDecl:
		{
			// Process children before the parent so that we can correctly handle the mapping between macro and types
			// We dont want an nested registration macro to cause an unwanted type to be registered
		//	pContext->PushNamespace( cursorName );
		//	clang_visitChildren( cr, VisitTranslationUnit, pClientData );
		//	pContext->PopNamespace();

		//	if ( pContext->HasErrorOccured() )
		//	{
		//		return CXChildVisit_Break;
		//	}
			if (cursorDisplayName == STRING_LIT("meScene"))
			{
				LOG_INFO("%.*s %.*s:%u\n", STRING_VAARGS(cursorDisplayName), STRING_VAARGS(headerPath), fileLineNum);
			}

		//	return VisitStructure( pContext, cr, headerFilePath, headerID );
			return CXChildVisit_Recurse;

		}
		break;

		// Enums
		case CXCursor_EnumDecl:
		{
			//return VisitEnum( pContext, cr, headerID );
			return CXChildVisit_Recurse;

		}
		break;

		// Non-Type Cursors
		case CXCursor_Namespace:
		{
			//if ( pContext->IsInEngineNamespace() || pContext->IsEngineNamespace( cursorName ) )
			//{
			//	pContext->PushNamespace( cursorName );
			//	clang_visitChildren( cr, VisitTranslationUnit, pClientData );
			//	pContext->PopNamespace();
			//}

			return CXChildVisit_Recurse;
		}
		break;

		// Macros
		case CXCursor_MacroExpansion:
		{
			//return VisitMacro( pContext, pReflectedHeader, cr, cursorName );
			return CXChildVisit_Recurse;

		}
		break;

		// Irrelevant Cursors
		default:
		{
			return CXChildVisit_Recurse;
		}
	}
	
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
	
	auto idx = clang_createIndex(0, 1);
	u32 clangOptions = 0
		| CXTranslationUnit_DetailedPreprocessingRecord
		| CXTranslationUnit_SkipFunctionBodies
		| CXTranslationUnit_IncludeBriefCommentsInCodeCompletion
		//| CXTranslationUnit_KeepGoing
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
		result = clang_parseTranslationUnit2( idx, reflectorHeaderFilename, clangArgs, DynArrayGetSize(clangArgs), 0, 0, clangOptions, &tu );
	}
	ClangParsingContext parsingContext;
	parsingContext.allocator = &reflectorArena;
	parsingContext.projectRootDir = String(projectRootDir, CStringLength(projectRootDir));
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