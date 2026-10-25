static const char* compiler;
#define NOB_REBUILD_URSELF(binary, source) compiler, "-o", binary, source, "-std=c++20", "-g"
#define NOB_IMPLEMENTATION
#include "tools/nob.h"
#undef NOB_IMPLEMENTATION
#include "tools/ninja_generate.h"

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        nob_log(NOB_ERROR, "Usage: me_build <clang> <project-root> [options]");
        return 1;
    }
    compiler = argv[1];
    NOB_GO_REBUILD_URSELF_PLUS(argc, argv, "tools/nob.h", "tools/ninja_generate.h");
    if (!nob_set_current_dir(argv[2]))
    {
        return 1;
    }
    const char* root = fullPath(".");
    compiler = fullPath(compiler);
    const char* ninjaExe = fullPath("tools/ninja/windows/ninja.exe");
    bool release = false, asan = false, profile = false, appOnly = false, reflect = false, libs = false;
    for (int i = 3; i < argc; ++i)
    {
        const char* option = argv[i];
        if (equal(option, "clean"))
        {
            for (const char* folder : Strings{"build", "mindseye/generatedtypes", "mindseye/shaders/generated"})
            {
                if (nob_file_exists(folder) && !nob_delete_dir(fullPath(folder)))
                {
                    return 1;
                }
            }
            return removeFile("build.ninja") ? 0 : 1;
        }
        if (equal(option, "tsan"))
        {
            nob_log(NOB_ERROR, "ThreadSanitizer is unsupported on Windows.");
            return 1;
        }
        if (equal(option, "release"))
        {
            release = true;
        }
        else if (equal(option, "asan"))
        {
            asan = true;
        }
        else if (equal(option, "profile"))
        {
            profile = true;
        }
        else if (equal(option, "app-only"))
        {
            appOnly = true;
        }
        else if (equal(option, "reflect"))
        {
            reflect = true;
        }
        else if (equal(option, "libs"))
        {
            libs = true;
        }
    }
    // ================ Build prerequisites ======================
    const struct
    {
        const char* path;
        const char* script;
    } setups[] = {
        {"mindseye/external/vulkan_lib/Lib", "tools/download_vulkan.bat"},
        {"mindseye/external/bgfx/bin", "tools/download_bgfx.bat"},
    };
    for (const auto& setup : setups)
    {
        if (!nob_file_exists(setup.path) && !run({"cmd", "/c", "call", setup.script}))
        {
            return 1;
        }
    }
    if (!nob_file_exists("mindseye/external/imgui-node-editor/imgui_node_editor.cpp") &&
        !run({"git", "submodule", "update", "--init", "--recursive"}))
    {
        return 1;
    }
    if (!makeDirectory("build") || !makeDirectory("mindseye/generatedtypes") ||
        !makeDirectory("mindseye/shaders/generated"))
    {
        return 1;
    }
    Strings files;
    if (!collectFiles(fullPath("mindseye"), files, words("external generatedtypes thirdparty generated")))
    {
        return 1;
    }

    // ================ Shared compiler and linker flags ======================
    const char* commonCompilerFlags =
        "-std=c++20 -march=native -g -gno-column-info -Wall -Wextra -Wno-unused-parameter "
        "-Wno-microsoft-include -Wno-deprecated-declarations -ferror-limit=500";
    Strings common = words(commonCompilerFlags);
    const char* releaseCompilerFlags = "-O2 -DNDEBUG -DBUILD_DEBUG=0 -DSHIPPING_BUILD=1 -DBX_CONFIG_DEBUG=0";
    const char* debugCompilerFlags =
        "-O0 -D_DEBUG -DBUILD_DEBUG=1 -DSHIPPING_BUILD=0 -DBX_CONFIG_DEBUG=1 -fno-pie -DTRACY_ENABLE=1";
    append(common, words(release ? releaseCompilerFlags : debugCompilerFlags));
    if (asan)
    {
        common.add("-fsanitize=address");
    }
    if (profile)
    {
        common.add("-ftime-trace");
    }
    const char* includeDirectories =
        ". mindseye mindseye/external "
        "mindseye/external/bgfx/bgfx/3rdparty/dear-imgui mindseye/external/bgfx/bgfx/include "
        "mindseye/external/bgfx/bgfx/3rdparty mindseye/external/imgui-node-editor "
        "mindseye/external/bgfx/bx/include "
        "mindseye/external/bgfx/bimg/include mindseye/external/ktx "
        "mindseye/external/tracy/public";
    for (const char* include : words(includeDirectories))
    {
        common.add(nob_temp_sprintf("-I%s", fullPath(include)));
    }
    Strings engineFlags = common;
    append(engineFlags, words("-DMEEXPORT -D_DLL -DTRACY_EXPORTS"));
    Strings linkFlags = {"-fuse-ld=lld-link"};
    linkFlags.add(release ? "-lmsvcrt" : "-lmsvcrtd");
    if (!release)
    {
        linkFlags.add("-Wl,/DYNAMICBASE:NO");
    }
    if (asan)
    {
        linkFlags.add("-fsanitize=address");
    }

    // ================ Runtime dependencies ======================
    Ninja graph(compiler, root);
    Strings runtime;
    auto copy = [&](const char* source, const char* dest)
    {
        graph.copy(source, dest);
        runtime.add(fullPath(dest));
    };
    const char* clangDir = fullPath("tools/clang");
    const char* libclang = fullPath("mindseye/reflector/libclang.dll");
    copy("tools/clang/bin/libclang.dll", libclang);
    copy("mindseye/external/vulkan_lib/Lib/vulkan-1.dll", "build/vulkan-1.dll");
    copy("mindseye/external/ktx/bin/ktx.dll", "build/ktx.dll");

    // ================ Mindseye reflector ======================
    size_t runtimeGraph = graph.text.count;
    const char* manifest = fullPath("build/reflection.outputs");
    const char* stamp = fullPath("build/reflection.stamp");
    auto emitReflection = [&](Ninja& out)
    {
        Strings flags = words("-std=c++20 -g -Wall -DMEEXPORT -DBUILD_DEBUG=1 -DME_CORE_ONLY");
        append(flags, {nob_temp_sprintf("-I%s", fullPath(".")), nob_temp_sprintf("-I%s", fullPath("mindseye")),
                       nob_temp_sprintf("-I%s/include", clangDir), "-DSTBSP__PUBLICDEC=extern \"C\" MEAPI"});
        Strings objects;
        const char* reflectorSources =
            "mindseye/reflector/me_reflector.cpp mindseye/platform/me_os.cpp "
            "mindseye/platform/windows/me_os_win.cpp mindseye/core/me_log.cpp mindseye/core/me_string.cpp "
            "mindseye/core/me_arena.cpp mindseye/core/me_core.cpp mindseye/core/me_math.cpp "
            "mindseye/core/me_memory.cpp "
            "mindseye/core/me_filesystem.cpp mindseye/reflector/reflection_types.cpp mindseye/thirdparty/stb.cpp";
        for (const char* source : words(reflectorSources))
        {
            objects.add(out.compile(fullPath(source), flags, "reflector"));
        }
        Strings libraries = {"-lntdll", "-Wl,/NOIMPLIB", "-Wl,/DEBUG",
                             nob_temp_sprintf("%s/lib/libclang.lib", clangDir)};
        const char* reflectorSystemLibraries = "-lshell32 -lole32 -luuid -ladvapi32 -lversion";
        append(libraries, words(reflectorSystemLibraries));
        const char* reflectorLibraries =
            "clangInterpreter.lib clangFrontendTool.lib clangFrontend.lib clangAPINotes.lib clangSerialization.lib "
            "clangDriver.lib clangCodeGen.lib clangExtractAPI.lib clangInstallAPI.lib clangIndex.lib "
            "clangRewriteFrontend.lib clangRewrite.lib clangStaticAnalyzerFrontend.lib "
            "clangStaticAnalyzerCheckers.lib clangStaticAnalyzerCore.lib clangCrossTU.lib clangToolingCore.lib "
            "clangParse.lib clangSema.lib clangAnalysis.lib clangASTMatchers.lib clangAST.lib clangLex.lib "
            "clangBasic.lib clangEdit.lib clangSupport.lib LLVMPasses.lib LLVMHipStdPar.lib LLVMCoroutines.lib "
            "LLVMipo.lib LLVMVectorize.lib LLVMSandboxIR.lib LLVMLinker.lib LLVMFrontendOpenMP.lib "
            "LLVMFrontendDirective.lib LLVMFrontendAtomic.lib LLVMFrontendDriver.lib LLVMFrontendHLSL.lib "
            "LLVMFrontendOffloading.lib LLVMObjectYAML.lib LLVMWindowsDriver.lib LLVMOrcDebugging.lib LLVMLTO.lib "
            "LLVMX86CodeGen.lib LLVMX86AsmParser.lib LLVMX86Desc.lib LLVMX86Info.lib LLVMOrcJIT.lib "
            "LLVMOrcShared.lib LLVMOrcTargetProcess.lib LLVMJITLink.lib LLVMExecutionEngine.lib LLVMRuntimeDyld.lib "
            "LLVMAsmPrinter.lib LLVMCodeGen.lib LLVMCodeGenTypes.lib LLVMCGData.lib LLVMSelectionDAG.lib "
            "LLVMGlobalISel.lib LLVMInstrumentation.lib LLVMIRPrinter.lib LLVMCFGuard.lib LLVMAsmParser.lib "
            "LLVMMCParser.lib LLVMMCDisassembler.lib LLVMMC.lib LLVMObject.lib LLVMTarget.lib "
            "LLVMTransformUtils.lib LLVMScalarOpts.lib LLVMInstCombine.lib LLVMAggressiveInstCombine.lib "
            "LLVMObjCARCOpts.lib LLVMAnalysis.lib LLVMCoverage.lib LLVMProfileData.lib LLVMDebugInfoCodeView.lib "
            "LLVMDebugInfoDWARF.lib LLVMDebugInfoDWARFLowLevel.lib LLVMTextAPI.lib LLVMIRReader.lib "
            "LLVMBitWriter.lib LLVMBitReader.lib LLVMCore.lib LLVMRemarks.lib LLVMBitstreamReader.lib "
            "LLVMOption.lib LLVMBinaryFormat.lib LLVMTargetParser.lib LLVMDemangle.lib LLVMSupport.lib";
        for (const char* library : words(reflectorLibraries))
        {
            libraries.add(nob_temp_sprintf("%s/lib/%s", clangDir, library));
        }
        const char* executable = fullPath("mindseye/reflector/me_reflector.exe");
        out.link(executable, libraries, objects, {fullPath("mindseye/reflector/me_reflector.pdb")});
        // ================ Reflection generation ======================
        Strings inputs = {executable, fullPath("build/reflection.flags"), fullPath("build/reflection.inputs")};
        Nob_String_Builder headerList = {};
        for (const char* file : files)
        {
            if ((endsWith(file, ".h") || endsWith(file, ".inl")))
            {
                inputs.add(file);
                nob_sb_appendf(&headerList, "%s\n", file);
            }
        }
        bool wroteHeaders = write("build/reflection.inputs", headerList);
        nob_sb_free(headerList);
        if (!wroteHeaders)
        {
            return false;
        }
        Strings reflectArgs = {compiler};
        append(reflectArgs, engineFlags);
        reflectArgs.add(fullPath("mindseye/core/me_core.cpp"));
        if (!write("build/reflection.flags", render(reflectArgs)))
        {
            return false;
        }
        Strings outputs = {stamp, manifest};
        append(outputs, lines(manifest));
        out.edge("run", outputs, inputs, {libclang});
        out.variable("cmd", nob_temp_sprintf("cmd /d /s /c \"\"%s\" %s && type nul > \"%s\"\"", executable,
                                             render({fullPath("build/reflection.flags"), fullPath("mindseye"),
                                                     fullPath("mindseye/generatedtypes"), manifest}),
                                             stamp));
        out.variable("label", "REFLECT");
        return out.valid;
    };
    // Reflection discovers its output set
    if (!appOnly)
    {
        if (!emitReflection(graph) || !graph.save("build/reflection.ninja"))
        {
            return 1;
        }
        if (reflect && !removeFile(stamp))
        {
            return 1;
        }
        if (!runNinja(ninjaExe, "build/reflection.ninja", stamp))
        {
            return 1;
        }
        // successful reflection run may have changed its output set
        graph.text.count = runtimeGraph;
        if (!emitReflection(graph))
        {
            return 1;
        }
    }

    Strings shaderOutputs;
    if (!appOnly)
    {
        // ================ Mindseye shaders ======================
        Strings shaderInputs;
        Strings shaderFiles;
        if (!collectFiles(fullPath("mindseye/shaders"), shaderFiles))
        {
            return 1;
        }
        for (const char* path : shaderFiles)
        {
            if (endsWith(path, ".sc"))
            {
                shaderInputs.add(path);
            }
        }
        const char* shaderc = fullPath("mindseye/external/bgfx/bin/shadercRelease.exe");
        for (const char* source : shaderInputs)
        {
            const char* name = nob_path_name(source);
            bool vertex = strstr(name, "_vs") != nullptr;
            if (!vertex && strstr(name, "_fs") == nullptr)
            {
                continue;
            }
            const char* output = fullPath(nob_temp_sprintf("mindseye/shaders/generated/%s.h", name));
            Strings inputs = shaderInputs;
            inputs.add(shaderc);
            graph.edge("run", {output}, inputs);
            graph.variable("cmd", render({shaderc, "--type", vertex ? "vertex" : "fragment", "-f", source, "--bin2c",
                                          "--platform", "windows", "-p", "spirv16-13", "--varyingdef",
                                          fullPath("mindseye/shaders/varying.def.sc"), "-o", output}));
            graph.variable("label", nob_temp_sprintf("SHADER %s", name));
            shaderOutputs.add(output);
        }
        // ================ Mindseye precompiled header ======================
        const char* pch = fullPath("build/me_pch.h.pch");
        const char* pchSource = fullPath("mindseye/me_pch.h");
        Strings pchArgs = engineFlags;
        append(pchArgs,
               {"-x", "c++-header", pchSource, "-o", pch, "-MMD", "-MF", nob_temp_sprintf("%s.d", pch), "-MT", pch});
        graph.edge("cc", {pch}, {pchSource, compiler});
        graph.ccCommand(pchArgs, pch);
        graph.variable("depfile", nob_temp_sprintf("%s.d", pch));
        // ================ Mindseye engine core ======================
        Strings objects, order = shaderOutputs;
        append(order, {stamp, pch});
        Strings compileFlags = engineFlags;
        append(compileFlags, {"-include-pch", pch});
        for (const char* source : files)
        {
            if (!endsWith(source, ".cpp") || equal(source, fullPath("mindseye/reflector/me_reflector.cpp")) ||
                equal(source, fullPath("mindseye/platform/driver.cpp")) ||
                strstr(source, "/vulkan_backend/") != nullptr)
            {
                continue;
            }
            objects.add(graph.compile(source, compileFlags, "engine", order));
        }
        // ================ Generated reflection sources ======================
        for (const char* source : lines(manifest))
        {
            if (endsWith(source, ".cpp"))
            {
                objects.add(graph.compile(source, compileFlags, "generated", order));
            }
        }
        // ================ External libraries ======================
        Strings vendorFlags = engineFlags;
        const char* vendorCompilerFlags =
            "-w -DIMGUI_DEFINE_MATH_OPERATORS -DIMGUI_API=MEAPI -DIMGUI_NODE_EDITOR_API=MEAPI "
            "-DIMGUI_DISABLE_SSE -DUSE_LOCAL_STB=0";
        append(vendorFlags, words(vendorCompilerFlags));
        append(vendorFlags,
               {"-include", fullPath("mindseye/core/me_defines.h"), "-DSTBSP__PUBLICDEC=extern \"C\" MEAPI"});
        const char* vendorSources =
            "mindseye/thirdparty/stb.cpp mindseye/thirdparty/miniz.cpp mindseye/thirdparty/imgui_stubs.cpp "
            "mindseye/external/bgfx/bgfx/3rdparty/dear-imgui/imgui.cpp "
            "mindseye/external/bgfx/bgfx/3rdparty/dear-imgui/imgui_draw.cpp "
            "mindseye/external/bgfx/bgfx/3rdparty/dear-imgui/imgui_widgets.cpp "
            "mindseye/external/bgfx/bgfx/3rdparty/dear-imgui/imgui_tables.cpp "
            "mindseye/external/imgui-node-editor/imgui_canvas.cpp "
            "mindseye/external/imgui-node-editor/crude_json.cpp "
            "mindseye/external/imgui-node-editor/imgui_node_editor.cpp "
            "mindseye/external/imgui-node-editor/imgui_node_editor_api.cpp "
            "mindseye/external/tracy/public/TracyClient.cpp "
            "mindseye/external/tinyfiledialogs/tinyfiledialogs.c "
            "mindseye/external/bgfx/bgfx/examples/common/imgui/imgui.cpp "
            "mindseye/external/bgfx/bgfx/examples/common/debugdraw/debugdraw.cpp";
        for (const char* source : words(vendorSources))
        {
            Strings flags = vendorFlags;
            if (endsWith(source, ".c"))
            {
                append(flags, {"-x", "c++"});
            }
            if (strstr(source, "dear-imgui/imgui.cpp") != nullptr)
            {
                append(flags, {"-include", fullPath("mindseye/thirdparty/imgui_config.h")});
            }
            const char* object = graph.compile(fullPath(source), flags, "vendor");
            if (libs && !removeFile(object))
            {
                return 1;
            }
            objects.add(object);
        }
        // ================ Mindseye engine DLL ======================
        Strings flags = linkFlags;
        const char* engineSystemLibraries =
            "-shared -luser32 -lgdi32 -ldbghelp -ladvapi32 -lws2_32 -lshell32 -lole32 -lcomdlg32";
        append(flags, words(engineSystemLibraries));
        for (const char* library : words("ktx bgfx bimg bx"))
        {
            const char* path = equal(library, "ktx") ? "mindseye/external/ktx/lib/ktx.lib"
                                                     : nob_temp_sprintf("mindseye/external/bgfx/bin/%s%s.lib", library,
                                                                        release ? "Release" : "Debug");
            flags.add(fullPath(path));
        }
        graph.link(fullPath("build/mindseye.dll"), flags, objects, {fullPath("build/mindseye.lib")});
    }
    // ================ Testbed application DLL ======================
    Strings appFlags = common;
    appFlags.add("-DTRACY_IMPORTS");
    Strings appLink = linkFlags;
    appLink.add(fullPath("build/mindseye.lib"));
    Strings appOrder = appOnly ? Strings{} : Strings{stamp};
    const char* appObject = graph.compile(fullPath("projects/testbed/testbed.cpp"), appFlags, "app", appOrder);
    Strings dllFlags = appLink;
    dllFlags.add("-shared");
    const char* appDll = fullPath("build/testbed.dll");
    graph.link(appDll, dllFlags, {appObject});
    Strings defaults = {appDll};
    if (!appOnly)
    {
        // ================ Mindseye driver ======================
        const char* driver = graph.compile(fullPath("mindseye/platform/driver.cpp"), appFlags, "app", {stamp});
        appLink.add("-lshell32");
        appLink.add("-Wl,/subsystem:windows");
        graph.link(fullPath("build/driver.exe"), appLink, {driver});
        defaults.add(fullPath("build/driver.exe"));
        append(defaults, runtime);
    }
    // ================ Generate and invoke Ninja ======================
    graph.defaults(defaults);
    const char* graphPath = appOnly ? "build/app-only.ninja" : "build.ninja";
    if (!graph.save(graphPath))
    {
        return 1;
    }
    uint64_t before = fileTime(appDll);
    if (!runNinja(ninjaExe, graphPath))
    {
        return 1;
    }
    bool rebuilt = fileTime(appDll) != before;
    if (appOnly)
    {
        return rebuilt ? 0 : 2;
    }
    // ================ Hot reload DLL copies ======================
    if (rebuilt)
    {
        Nob_File_Paths entries = {};
        if (!nob_read_entire_dir("build", &entries))
        {
            return 1;
        }
        bool success = true;
        for (size_t i = 0; i < entries.count; ++i)
        {
            const char* name = entries.items[i];
            if (strncmp(name, "testbed-", 8) != 0 || !endsWith(name, ".dll"))
            {
                continue;
            }
            size_t digits = strspn(name + 8, "0123456789");
            if (digits && equal(name + 8 + digits, ".dll"))
            {
                success = removeFile(nob_temp_sprintf("build/%s", name)) && success;
            }
        }
        nob_da_free(entries);
        if (!success || !nob_copy_file(appDll, "build/testbed-1.dll"))
        {
            return 1;
        }
    }
    return 0;
}
