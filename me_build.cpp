// TODO: put this and the build artifacts for this builder in the tools dir

// cmdline used to build ourself. Should match the invocation in build.bat
// for debugging this builder program, add "-g" here

enum meBuildMode
{
	DEBUG, RELEASE,
};
static char* root = nullptr;
static char* g_compilerExe = nullptr;

#define NOB_REBUILD_URSELF(binary_path, source_path) g_compilerExe, "-o", binary_path, source_path, "-std=c++20", "-g"
#define NOB_IMPLEMENTATION
#define NOB_WARN_DEPRECATED
#include "tools/nob.h"

#include "mindseye/core/me_defines.h"
#include <stdio.h>


enum BuildResult
{
	DID_NOT_BUILD,
	BUILD_FAILED,
	BUILD_SUCCEEDED,
};

struct BuildableArtifact
{
	Nob_Cmd compileCmd = {};
	Nob_Cmd postprocessCmd = {};
	Nob_Cmd_Opt options = {};
	Nob_File_Paths inputs = {};
	Nob_String_View output = {};

	void addInputs(const char** inputs, size_t numInputs)
	{
		for (int i = 0; i < numInputs; i++)
		{
			nob_cmd_append(&compileCmd, inputs[i]);
			nob_da_append(&this->inputs, inputs[i]);
		}
	}
	// for stuff like header files, we want to check them to rebuild, but not actually compile them
	void addInputsNoCompile(const char** inputs, size_t numInputs)
	{
		for (int i = 0; i < numInputs; i++)
		{
			nob_da_append(&this->inputs, inputs[i]);
		}
	}
	void addOutput(const char* output)
	{
		ME_ASSERT(this->output.count == 0);
		nob_cc_output(&compileCmd, output);
		this->output = nob_sv_from_cstr(output);

		const char* mebuild = nob_temp_sprintf("%s/me_build.cpp", root);
		addInputsNoCompile(&mebuild, 1);
	}

	BuildResult build(bool force = false)
	{
		if (force || (output.count && nob_needs_rebuild(output.data, inputs.items, inputs.count) > 0))
		{
			bool result = false;
			if (compileCmd.count)
			{
				result = nob_cmd_run(&compileCmd);
			}
			if (result && postprocessCmd.count)
			{
				result = nob_cmd_run_opt(&postprocessCmd, options);
			}
			return result ? BUILD_SUCCEEDED : BUILD_FAILED;
		}
		return DID_NOT_BUILD;
	}
};

typedef bool(*readDirFileFilter)(const char* filename);
void readEntireDirRecursive(const char* parent, Nob_File_Paths* paths, readDirFileFilter filefilter)
{
	Nob_File_Paths thisDirEntries = {};
	if (!nob_read_entire_dir(parent, &thisDirEntries))
	{
		nob_log(NOB_ERROR, "Failed to read files from dir %s", parent);
		return;
	}
	for (int i = 0; i < thisDirEntries.count; i++)
	{
		const char* item = thisDirEntries.items[i];
		if (strncmp(item, ".", 1) == 0 || strncmp(item, "..", 2) == 0) continue;
		const char* itemFullPath = nob_temp_sprintf("%s/%s", parent, item);
		Nob_File_Type filetype = nob_get_file_type(itemFullPath);
		if (filetype == NOB_FILE_DIRECTORY)
		{
			readEntireDirRecursive(itemFullPath, paths, filefilter);
		}
		else if (filetype == NOB_FILE_REGULAR)
		{
			if (!filefilter(itemFullPath))
			{
				continue;
			}
			nob_da_append(paths, nob_temp_strdup(itemFullPath));
		}
	}
}

#define NOB_CMD_APPEND_MULTIPLE(compile, arr ...) \
for (int i = 0; i < ARRAY_SIZE(arr); i++) \
{\
nob_cmd_append(&compile, arr[i]);\
}

void normalizePathSeperators(char* str)
{
	for (u32 i = 0; i < strlen(str); i++)
	{
		if (str[i] == '\\')
		{
			str[i] = '/';
		}
	}
}

const char* nob_get_filename_from_path(const char* path)
{
	const char* lastSlash = strrchr(path, '/');
	if (!lastSlash) return path;
	const char* filename = lastSlash + 1;
	return filename;
}

void clean()
{
    nob_log(NOB_INFO, "Cleaning...");
    nob_delete_dir("build");
    nob_delete_dir("mindseye/generatedtypes");
    nob_delete_dir("mindseye/shaders/generated");

    Nob_File_Paths toolsfiles = {};
    nob_read_entire_dir("tools", &toolsfiles);
    for (int i = 0; i < toolsfiles.count; i++)
    {
        const char* toolsfile = toolsfiles.items[i];
        if (strstr(toolsfile, "me_build"))
        {
            nob_delete_file(nob_temp_sprintf("%s/tools/%s", root, toolsfile));
        }
    }
}

int main(int argc, char** argv)
{
	nob_minimal_log_level = NOB_INFO;
	g_compilerExe = argv[1];
	NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "mindseye/core/me_defines.h", "tools/nob.h");
	root = argv[2];
    const char* command = argv[3];
    if (argc > 3 && strcmp(command, "clean") == 0)
    {
        clean();
        return 0;
    }
	nob_log(NOB_INFO, "%s", g_compilerExe);
	normalizePathSeperators(root);
	normalizePathSeperators(g_compilerExe);
	meBuildMode mode = DEBUG;
	bool forceBuildLibs = false;
	for (int i = 1; i < argc; i++)
	{
		Nob_String_View argvSv = nob_sv_from_cstr(argv[i]);
		if (nob_sv_eq(argvSv, nob_sv_from_cstr("release")))
		{
			mode = RELEASE;
			nob_log(NOB_INFO, "[Release mode]");
		}
		else if (nob_sv_eq(argvSv, nob_sv_from_cstr("libs")))
		{
			forceBuildLibs = true;
		}
	}
	nob_mkdir_if_not_exists("build");

	if (!nob_file_exists("mindseye/external/vulkan_lib/Lib"))
	{
		nob_log(NOB_INFO, "[First time setup] downloading vulkan binaries...");
		Nob_Cmd vulkanDownloadBatch = {};
		nob_cmd_append(&vulkanDownloadBatch, "cmd", "/c", "call", "tools/download_vulkan.bat");
		bool result = nob_cmd_run(&vulkanDownloadBatch);
		if (!result)
		{
			nob_log(NOB_ERROR, "Failed to download vulkan binaries!");
			return 1;
		}
		else
		{
			nob_log(NOB_INFO, "Successfully downloaded vulkan binaries to mindseye/external/vulkan_lib/Lib");
		}
	}
	if (!nob_file_exists("build/vulkan-1.dll"))
	{
		nob_copy_file("mindseye/external/vulkan_lib/Lib/vulkan-1.dll", "build/vulkan-1.dll");
	}

	if (!nob_file_exists("mindseye/external/bgfx/bin"))
	{
		nob_log(NOB_INFO, "[First time setup] downloading bgfx binaries...");
		Nob_Cmd bgfxDownloadBatch = {};
		nob_cmd_append(&bgfxDownloadBatch, "cmd", "/c", "call", "tools/download_bgfx.bat");
		bool result = nob_cmd_run(&bgfxDownloadBatch);
		if (!result)
		{
			nob_log(NOB_ERROR, "Failed to download bgfx binaries!");
			return 1;
		}
		else
		{
			nob_log(NOB_INFO, "Successfully downloaded bgfx binaries to mindseye/external/bgfx/bin");
		}
	}
	
	const char* linkerFlagsCommon[] =
	{ 
		"-luser32", "-lgdi32", "-fuse-ld=lld-link", (mode == DEBUG ? "-lmsvcrtd" : "-lmsvcrt"),
		nob_temp_sprintf("-L%s/build", root),
		nob_temp_sprintf("-L%s/tools/clang/lib/clang/21/lib/windows", root),
		"-lclang_rt.builtins-x86_64",
	};
	
	const char* compilerFlagsCommon[] =
	{
		// general flags
		nob_temp_sprintf("-I%s", root),
		"-std=c++20",
		"-march=native",
		"-Wno-deprecated-declarations",
		"-g", "-gno-column-info",
		"-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-microsoft-include", "-ferror-limit=500",
		// 	for /f %%i in ('call git describe --always --dirty')   do set compile_flags_common=%compile_flags_common% -DBUILD_GIT_HASH=\"%%i\"
        (mode == DEBUG ? "-D_DEBUG" : "-DNDEBUG"),

		// app flags
		"-DSHIPPING_BUILD=0", 
		nob_temp_sprintf("-I%s/mindseye", root), 
		nob_temp_sprintf("-I%s/mindseye/external", root),

		// include libs
		nob_temp_sprintf("-I%s/mindseye/external/bgfx/bgfx/3rdparty/dear-imgui", root),
		nob_temp_sprintf("-I%s/mindseye/external/bgfx/bgfx/include", root),
		nob_temp_sprintf("-I%s/mindseye/external/bgfx/bgfx/3rdparty", root),
		nob_temp_sprintf("-I%s/mindseye/external/bgfx/bx/include", root),
		nob_temp_sprintf("-I%s/mindseye/external/bgfx/bimg/include", root),
		nob_temp_sprintf("-I%s/mindseye/external/ktx", root),
		nob_temp_sprintf("-I%s/mindseye/external/cereal/include", root),
	};


	// ======================== DRIVER ==============================================
	BuildableArtifact driver = {};
	Nob_Cmd& driverCompile = driver.compileCmd;
	// driver compile
	nob_cmd_append(&driverCompile, g_compilerExe);
	if (mode == DEBUG)
	{ 
		nob_cmd_append(&driverCompile, "-O0", "-DBUILD_DEBUG=1");
	} 
	else if (mode == RELEASE)
	{
		nob_cmd_append(&driverCompile, "-O2" ,"-DBUILD_DEBUG=0");
	}
	NOB_CMD_APPEND_MULTIPLE(driverCompile, compilerFlagsCommon);
	// driver linker
	NOB_CMD_APPEND_MULTIPLE(driverCompile, linkerFlagsCommon);
	nob_cmd_append(&driverCompile, "-lmindseye", "-Wl,/subsystem:windows");
	// input/output
	const char* input = nob_temp_sprintf("%s/mindseye/platform/driver.cpp", root);
	driver.addInputs(&input, 1);
	driver.addOutput("driver.exe");
	// =====================================================================================

	// ======================== External libraries (Object) ==============================================
	BuildableArtifact externalLibsObj = {};
	Nob_Cmd& externalLibsCmd = externalLibsObj.compileCmd;
	// compile to object file
	nob_cmd_append(&externalLibsCmd, g_compilerExe);
	if (mode == DEBUG)
	{
		nob_cmd_append(&externalLibsCmd,
		"-O0", "-DBUILD_DEBUG=1" ,"-DMEEXPORT", "-DBX_CONFIG_DEBUG=1", "-c", "-w");
	}
	else if (mode == RELEASE)
	{
		nob_cmd_append(&externalLibsCmd, 
		"-O2", "-DBUILD_DEBUG=0", "-DMEEXPORT", "-DBX_CONFIG_DEBUG=0", "-c", "-w");
	}
	NOB_CMD_APPEND_MULTIPLE(externalLibsCmd, compilerFlagsCommon);
	// input/output
	const char* externalLibsInputs = nob_temp_sprintf("%s/mindseye/me_external_unity.cpp", root);
	externalLibsObj.addInputs(&externalLibsInputs, 1);
	externalLibsObj.addOutput("mindseye_ext.o");
	// =====================================================================================
	
	// ======================== Mindseye Shaders ==============================================
	static constexpr const char shaderCodeFileExt[] = ".sc";
	static constexpr const char shaderCodeOutputExt[] = ".h";
	const char* shaderOutputDir = nob_temp_sprintf("%s/%s", root, "mindseye/shaders/generated");
	Nob_File_Paths shaderSourcePaths = {};
	auto shaderSourceFileFilter = + [](const char* filename)
	{
		// does it have the right extension and is that extension at the very end of the filename string
		const char* fileext = strstr(filename, shaderCodeFileExt);
		int idxToEnd = sizeof(shaderCodeFileExt)-1;
		char endOfFileExt = fileext ? fileext[idxToEnd] : 0;
		return fileext != 0 && endOfFileExt == '\0';
	};
	readEntireDirRecursive("mindseye/shaders", &shaderSourcePaths, shaderSourceFileFilter);
	nob_mkdir_if_not_exists("mindseye/shaders/generated");
	static constexpr int MAX_SHADER_FILES = 500;
	int fragmentInputsTotal = 0;
	const char* fragmentInputs[MAX_SHADER_FILES];
	memset(fragmentInputs, 0, sizeof(fragmentInputs));
	int vertexInputsTotal = 0;
	const char* vertexInputs[MAX_SHADER_FILES];
	memset(vertexInputs, 0, sizeof(vertexInputs));
	for (int i = 0; i < shaderSourcePaths.count; i++)
	{
		const char* item = shaderSourcePaths.items[i];
		if (strstr(item, shaderCodeFileExt))
		{
			const char* relItemPath = nob_sv_trim_right(nob_sv_from_cstr(item), shaderCodeFileExt).data;
			const char* absItemPath = nob_temp_sprintf("%s/%s", root, relItemPath);
			// IMPLICIT CONVENTION: vertex shaders need _vs in their name, and fragment shaders need _fs
			if (strstr(item, "_vs"))
			{
				vertexInputs[vertexInputsTotal++] = absItemPath;
			}
			else if (strstr(item, "_fs"))
			{
				fragmentInputs[fragmentInputsTotal++] = absItemPath;
			}
			if (vertexInputsTotal > MAX_SHADER_FILES || fragmentInputsTotal > MAX_SHADER_FILES)
			{
				nob_log(NOB_ERROR, "Too many shader files! Bump MAX_SHADER_FILES");
			}
		}
	}

	int numShadersToCompile = fragmentInputsTotal + vertexInputsTotal;
	BuildableArtifact* shaderArtifacts = (BuildableArtifact*)nob_temp_alloc(sizeof(BuildableArtifact) * numShadersToCompile);
	
	const char* shaderCompiler = nob_temp_sprintf("%s/mindseye/external/bgfx/bin/shadercRelease.exe", root);
	const char* shaderCommonFlags[] =
	{
		"--bin2c", "--platform", "windows", "-p", "spirv16-13", "--varyingdef", nob_temp_sprintf("%s/mindseye/shaders/varying.def.sc", root),
	};
	int shaderCompileIdx = 0;
	for (int i = 0; i < ARRAY_SIZE(fragmentInputs); i++)
	{
		const char* fragmentInput = fragmentInputs[i];
		if (!fragmentInput) continue;
		BuildableArtifact& shaderBuild = shaderArtifacts[shaderCompileIdx++];
		Nob_Cmd& mindseyeShadersCmd = shaderBuild.compileCmd;
		nob_cmd_append(&mindseyeShadersCmd, shaderCompiler, "--type", "fragment", "-f");
		shaderBuild.addInputs(&fragmentInput, 1);
		NOB_CMD_APPEND_MULTIPLE(mindseyeShadersCmd, shaderCommonFlags);
		const char* fragmentInputFilename = nob_get_filename_from_path(fragmentInput);
		const char* output = nob_temp_sprintf("%s/%s%s", shaderOutputDir, fragmentInputFilename, shaderCodeOutputExt);
		shaderBuild.addOutput(output);
	}
	for (int i = 0; i < ARRAY_SIZE(vertexInputs); i++)
	{
		const char* vertexInput = vertexInputs[i];
		if (!vertexInput) continue;
		BuildableArtifact& shaderBuild = shaderArtifacts[shaderCompileIdx++];
		Nob_Cmd& mindseyeShadersCmd = shaderBuild.compileCmd;
		nob_cmd_append(&mindseyeShadersCmd, shaderCompiler, "--type", "vertex", "-f");
		shaderBuild.addInputs(&vertexInput, 1);
		NOB_CMD_APPEND_MULTIPLE(mindseyeShadersCmd, shaderCommonFlags);
		const char* vertexInputFilename = nob_get_filename_from_path(vertexInput);
		const char* output = nob_temp_sprintf("%s/%s%s", shaderOutputDir, vertexInputFilename, shaderCodeOutputExt);
		shaderBuild.addOutput(output);
	}

	// =====================================================================================

	// ======================== Mindseye Engine ==============================================
	Nob_File_Paths mindseyeSourceRootFolders = {};
	const char* rootMindseyeSourceDir = nob_temp_sprintf("%s/mindseye", root);
	nob_read_entire_dir(rootMindseyeSourceDir, &mindseyeSourceRootFolders);
	Nob_File_Paths mindseyeSourceFiles = {};
	auto mindseyeRelevantCompileFileFilter = +[](const char* filename)
	{
		bool result = strstr(filename, "external") == 0 &&
			(strstr(filename, ".cpp") || strstr(filename, ".h"));
		return result;
	};
	for (int i = 0; i < mindseyeSourceRootFolders.count; i++)
	{
		const char* item = mindseyeSourceRootFolders.items[i];
		const char* itemFullPath = nob_temp_sprintf("%s/%s", rootMindseyeSourceDir, item);
		if (nob_get_file_type(itemFullPath) == NOB_FILE_DIRECTORY)
		{
			if (strncmp(item, ".", 1) == 0 || strncmp(item, "..", 2) == 0) continue;
			readEntireDirRecursive(itemFullPath, &mindseyeSourceFiles, mindseyeRelevantCompileFileFilter);
		}
		else
		{
			if (!mindseyeRelevantCompileFileFilter(itemFullPath))
			{
				continue;
			}
			nob_da_append(&mindseyeSourceFiles, itemFullPath);
		}
	}

	BuildableArtifact mindseyeEngineObj = {};
	Nob_Cmd& mindseyeCmd = mindseyeEngineObj.compileCmd;
	// compile to object file
	nob_cmd_append(&mindseyeCmd, g_compilerExe);
	if (mode == DEBUG)
	{
		nob_cmd_append(&mindseyeCmd, 
		"-O0", "-DBUILD_DEBUG=1", "-DMEEXPORT", "-D_USRDLL", "-D_WINDLL", "-D_DLL", "-c", "-DBX_CONFIG_DEBUG=1");
	}
	else if (mode == RELEASE)
	{
		nob_cmd_append(&mindseyeCmd, 
		"-O2", "-DBUILD_DEBUG=0", "-DMEEXPORT", "-D_USRDLL", "-D_WINDLL", "-D_DLL", "-c", "-DBX_CONFIG_DEBUG=0");
	}
	NOB_CMD_APPEND_MULTIPLE(mindseyeCmd, compilerFlagsCommon);
	// input/output
	const char* mindseyeEngineInputs = nob_temp_sprintf("%s/mindseye/me_unity.cpp", root);
	mindseyeEngineObj.addInputs(&mindseyeEngineInputs, 1);
	mindseyeEngineObj.addInputsNoCompile(mindseyeSourceFiles.items, mindseyeSourceFiles.count);
	mindseyeEngineObj.addOutput("mindseye.o");
	// =====================================================================================

	// ======================== Mindseye DLL (Link Step) ==============================================
	BuildableArtifact mindseyeDll = {};
	Nob_Cmd& mindseyeLinkCmd = mindseyeDll.compileCmd;
	// link object files together into DLL
	nob_cmd_append(&mindseyeLinkCmd, g_compilerExe, "-shared");
	NOB_CMD_APPEND_MULTIPLE(mindseyeLinkCmd, linkerFlagsCommon);
	NOB_CMD_APPEND_MULTIPLE(mindseyeLinkCmd, compilerFlagsCommon);
	nob_cmd_append(&mindseyeLinkCmd, nob_temp_sprintf("-L%s/mindseye/external/ktx/lib", root), "-lktx", "-lshell32");
	if (mode == DEBUG) 
    {
		nob_cmd_append(&mindseyeLinkCmd, nob_temp_sprintf("-L%s/mindseye/external/bgfx/bin", root), "-lbgfxDebug", "-lbimgDebug", "-lbxDebug");
	} 
    else 
    {
		nob_cmd_append(&mindseyeLinkCmd, nob_temp_sprintf("-L%s/mindseye/external/bgfx/bin", root), "-lbgfxRelease", "-lbimgRelease", "-lbxRelease");
	}
	// input objects
	const char* mindseyeDllInputs[] = { "mindseye.o", "mindseye_ext.o" };
	mindseyeDll.addInputs(mindseyeDllInputs, ARRAY_SIZE(mindseyeDllInputs));
	mindseyeDll.addOutput("mindseye.dll");
	// =====================================================================================

	// ======================== Mindseye Reflector==============================================
	BuildableArtifact mindseyeReflectorCompile = {};
	Nob_Cmd& mindseyeReflectorCompileCmd = mindseyeReflectorCompile.compileCmd;
	nob_cmd_append(&mindseyeReflectorCompileCmd, g_compilerExe);
	nob_cmd_append(&mindseyeReflectorCompileCmd,
				   nob_temp_sprintf("-I%s/mindseye", root), "-DMEEXPORT", "-DBUILD_DEBUG=1");
	nob_cmd_append(&mindseyeReflectorCompileCmd,
				   "-Wall", "-std=c++20", "-g", nob_temp_sprintf("-I%s", root), "-lntdll");
	const char* clangPath = nob_temp_sprintf("%s/tools/clang", root);
	nob_cmd_append(&mindseyeReflectorCompileCmd,
				   nob_temp_sprintf("-I%s/include", clangPath), nob_temp_sprintf("-L%s/lib", clangPath), "-llibclang", "-lLLVMSupport");
	const char* mindseyeReflectorInputs = "mindseye/reflector/me_reflector.cpp";
	mindseyeReflectorCompile.addInputs(&mindseyeReflectorInputs, 1);
	mindseyeReflectorCompile.addOutput("mindseye/reflector/me_reflector.exe");

	BuildableArtifact mindseyeReflectorRun = {};
	Nob_Cmd& mindseyeReflectorRunCmd = mindseyeReflectorRun.compileCmd;

	Nob_String_Builder mindseyeEngineRenderedCmd = {};
	nob_cmd_render(mindseyeEngineObj.compileCmd, &mindseyeEngineRenderedCmd);
	nob_sb_append_null(&mindseyeEngineRenderedCmd); // just in case...
	const char* mindseyeBuildCommand = mindseyeEngineRenderedCmd.items;
	const char* compileCommandsFile = "build/compile_commands_mindseye.txt";
	if (!nob_write_entire_file(compileCommandsFile, mindseyeBuildCommand, mindseyeEngineRenderedCmd.count))
	{
		nob_log(NOB_ERROR, "Something went wrong while trying to write the mindseye compile commands!");
		return 1;
	}
	const char* compileCommandsFileFullpath = nob_temp_sprintf("%s/%s", root, compileCommandsFile);
	nob_cmd_append(&mindseyeReflectorRunCmd, 
				   "mindseye/reflector/me_reflector.exe", compileCommandsFileFullpath, nob_temp_sprintf("%s/mindseye", root), nob_temp_sprintf("%s/mindseye/generatedtypes", root));
	mindseyeReflectorRun.addInputsNoCompile(mindseyeSourceFiles.items, mindseyeSourceFiles.count);
	mindseyeReflectorRun.addInputsNoCompile(&mindseyeReflectorInputs, 1);
	
	// =====================================================================================

	// copy tools/clang/bin/libclang.dll to reflector/ with nob_copy_file
	const char* libclangSource = nob_temp_sprintf("%s/tools/clang/bin/libclang.dll", root);
	const char* libclangDest = nob_temp_sprintf("%s/mindseye/reflector/libclang.dll", root);
	if (!nob_file_exists(libclangDest))
	{
		nob_copy_file(libclangSource, libclangDest);
	}
	// copy ktx.dll to build folder
	const char* ktxDllSource = nob_temp_sprintf("%s/mindseye/external/ktx/bin/ktx.dll", root);
	const char* ktxDllDest = nob_temp_sprintf("%s/build/ktx.dll", root);
	if (!nob_file_exists(ktxDllDest))
	{
		nob_copy_file(ktxDllSource, ktxDllDest);
	}

	// ======================== Testbed ==============================================
	BuildableArtifact testbedBuild = {};
	Nob_Cmd& testbedCmd = testbedBuild.compileCmd;
	// compile
	nob_cmd_append(&testbedCmd, g_compilerExe);
	if (mode == DEBUG)
	{
		nob_cmd_append(&testbedCmd,
						"-O0", "-DBUILD_DEBUG=1", 
					   	//"-D_USRDLL", "-D_WINDLL", "-D_DLL", 
					   	"-shared");
	}
	else if (mode == RELEASE)
	{
		nob_cmd_append(&testbedCmd,
						"-O2", "-DBUILD_DEBUG=0", 
					   	//"-D_USRDLL", "-D_WINDLL", "-D_DLL", 
					   	"-shared");
	}
	NOB_CMD_APPEND_MULTIPLE(testbedCmd, compilerFlagsCommon);
	// link
	NOB_CMD_APPEND_MULTIPLE(testbedCmd, linkerFlagsCommon);
	nob_cmd_append(&testbedCmd, nob_temp_sprintf("-L%s/build", root), "-lmindseye");
	if (!nob_file_exists(nob_temp_sprintf("%s/projects/testbed/gltf-samples", root)))
	{
		nob_log(NOB_INFO, "Testbed project relies on gltf-samples submodule. Pulling it in now...");
		Nob_Cmd submoduleUpdateCmd = {};
		nob_cmd_append(&submoduleUpdateCmd, "git", "submodule", "update", "--init", "--recursive");
		nob_cmd_run(&submoduleUpdateCmd);
	}
	const char* testbedInputs = nob_temp_sprintf("%s/projects/testbed/testbed.cpp", root);
	testbedBuild.addInputs(&testbedInputs, 1);
	testbedBuild.addInputsNoCompile(mindseyeSourceFiles.items, mindseyeSourceFiles.count);
	testbedBuild.addOutput("testbed.dll");
	// =====================================================================================

	
	// ======================== Invoke Build ==============================================
	Nob_Procs procs = {};

	#define CHECK_BUILD_RESULT(buildResult) \
		if (buildResult == BUILD_FAILED) \
		{ \
			return 1; \
		}
	for (int i = 0; i < numShadersToCompile; i++)
	{
		BuildableArtifact& shaderArtifact = shaderArtifacts[i];
		shaderArtifact.options.max_procs = 0; // implies nob_nprocs
		shaderArtifact.options.async = &procs;
		CHECK_BUILD_RESULT(shaderArtifact.build());
	}
	
	// can check build for reflector in parallel with shaders
	mindseyeReflectorCompile.options.max_procs = 0;
	mindseyeReflectorCompile.options.async = &procs;
	BuildResult reflectorBuildResult = mindseyeReflectorCompile.build();
	if (reflectorBuildResult == BUILD_SUCCEEDED)
	{
		// if we rebuilt the reflector program, we should force a full re-reflect of everything by deleting the output folder
		nob_delete_dir(nob_temp_sprintf("%s/mindseye/generatedtypes", root));
	}
	CHECK_BUILD_RESULT(mindseyeReflectorRun.build(true));
	
	if (!nob_procs_flush(&procs))
	{
		nob_log(NOB_ERROR, "Tragedy struck while waiting for build processes");
		return 1;
	}
	nob_set_current_dir("build");

	// Build object files in parallel
	externalLibsObj.options.async = &procs;
	BuildResult externalLibsBuildRes = externalLibsObj.build(forceBuildLibs);
	CHECK_BUILD_RESULT(externalLibsBuildRes);
	bool builtExternalLibs = externalLibsBuildRes == BUILD_SUCCEEDED;
	
	mindseyeEngineObj.options.async = &procs;
	BuildResult builtMindseyeObjRes = mindseyeEngineObj.build(builtExternalLibs);
	bool builtMindseyeObj = builtMindseyeObjRes == BUILD_SUCCEEDED;
	CHECK_BUILD_RESULT(builtMindseyeObjRes);
	
	testbedBuild.options.async = &procs;
	testbedBuild.options.max_procs = 0;

	BuildResult testbedResult = testbedBuild.build();
	
	if (testbedResult == DID_NOT_BUILD && builtMindseyeObj)
	{
		testbedBuild.build(true);
	}
	if (!nob_procs_flush(&procs))
	{
		nob_log(NOB_ERROR, "Tragedy struck while waiting for build processes");
		return 1;
	}

	
	// the link needs ext libs and the mindseye objs, so is dependent on the above stuff
	BuildResult builtMindseye = mindseyeDll.build(builtMindseyeObj);
	CHECK_BUILD_RESULT(builtMindseye);
	CHECK_BUILD_RESULT(driver.build());

	return 0;
}
