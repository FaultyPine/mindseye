
#include "me_source.cpp"

#include "clang/AST/AST.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

/*
using the compile commands database,
generate an omega header file that includes all the stuff we care about
Pass that one header to clang

clangArgs.push_back( "-x" );
clangArgs.push_back( "c++" );
clangArgs.push_back( "-std=c++20" );
clangArgs.push_back( "-O0" );
clangArgs.push_back( "-D NDEBUG" );
clangArgs.push_back( "-Werror" );
clangArgs.push_back( "-Wno-multichar" );
clangArgs.push_back( "-Wno-deprecated-builtins" );
clangArgs.push_back( "-fparse-all-comments" );
clangArgs.push_back( "-fms-extensions" );
clangArgs.push_back( "-fms-compatibility" );
clangArgs.push_back( "-Wno-unknown-warning-option" );
clangArgs.push_back( "-Wno-return-type-c-linkage" );
clangArgs.push_back( "-Wno-gnu-folding-constant" );
clangArgs.push_back( "-Wno-vla-extension-static-assert" );

+ any macro defines like -DSHIPPING_BUILD

exclude system libs & stl stuff with ClassFinder & MatchFinder - see https://www.youtube.com/watch?v=XoYVeduK4yI&ab_channel=cpponsea
Should I only reflect reflection annotated types/fields? Or try to do it all?

Generate in-memory representation of the data i care about.
The pass that to a code generator to write out generated headers for mindseye to use.

*/

DynArray(String) CompileDatabaseToHeaderList(String compileDatabasePath)
{
	return {};
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		LOG_ERROR("Invalid args, need to pass path to compile_commands.json\n");
		return 1;
	}
	const char* compileCmdsDatabaseFilePath = argv[1];
	LOG_ERROR("hello from reflector. Reflecting %s\n", compileCmdsDatabaseFilePath);
	return 0;
}