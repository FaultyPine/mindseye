#pragma once

#include "mindseye/core/me_log.h"
#include "mindseye/reflector/me_compile_run_api.h"

MEREFLECT(ConsoleCommand) void MyTestConsoleCommand()
{
    LOG_INFO("My Test Console Command");
}

#if ME_COMPILERUN_GUARD
MEREFLECT(CompileRun) void meCompileRunSmokeTest(meCompileRunContext* ctx)
{
    static u32 numVisitedDecls = 0;
    if (ctx->stage == meCompileRunStage_VisitDecl)
    {
        numVisitedDecls++;
    }
    else if (ctx->stage == meCompileRunStage_Finalize)
    {
        LOG_INFO("[CompileRun] Hello from normal Mindseye engine code after visiting %u annotated decl(s)", numVisitedDecls);
    }
}

MEREFLECT(CompileRun) void meCompileRunSmokeTest2(meCompileRunContext* ctx)
{
    static u32 numMeAssetTypes = 0;
    if (ctx->stage == meCompileRunStage_VisitDecl)
    {
        if (ctx->reflectOp != STRING_LIT("type"))
        {
            return;
        }

        CXCursorKind kind = clang_getCursorKind(ctx->cursor);
        if (kind != CXCursor_StructDecl && kind != CXCursor_ClassDecl)
        {
            return;
        }

        bool inheritsFromBaseAsset = false;
        clang_visitChildren(ctx->cursor, +[](CXCursor child, CXCursor parent, CXClientData clientData)
        {
            UNUSED(parent);
            if (clang_getCursorKind(child) != CXCursor_CXXBaseSpecifier)
            {
                return CXChildVisit_Continue;
            }

            CXType baseType = clang_getCursorType(child);
            CXString baseTypeSpelling = clang_getTypeSpelling(baseType);
            const char *baseTypeCStr = clang_getCString(baseTypeSpelling);
            StringView baseTypeName = StringFromCString(baseTypeCStr);
            if (FindInString(baseTypeName, STRING_LIT("meBaseAsset")) != -1)
            {
                *(bool *)clientData = true;
                clang_disposeString(baseTypeSpelling);
                return CXChildVisit_Break;
            }

            clang_disposeString(baseTypeSpelling);
            return CXChildVisit_Continue;
        }, &inheritsFromBaseAsset);

        if (inheritsFromBaseAsset)
        {
            CXType baseType = clang_getCursorType(ctx->cursor);
            CXString baseTypeSpelling = clang_getTypeSpelling(baseType);
            const char *baseTypeCStr = clang_getCString(baseTypeSpelling);
            StringView baseTypeName = StringFromCString(baseTypeCStr);
            LOG_INFO("Found asset type: " STRING_FMT, STRING_VAARGS(baseTypeName));
            clang_disposeString(baseTypeSpelling);
            numMeAssetTypes++;
        }
    }
    else if (ctx->stage == meCompileRunStage_Finalize)
    {
        LOG_INFO("[CompileRun] Found %u reflected meAsset type(s)", numMeAssetTypes);
    }
}

// This is just an example
// but in the future i'd like to use this system to write a compile-time parser pass
// that finds functions with a `CONSOLE_COMMAND` tag and auto-generates the boilerplate to register that function as a console command
MEREFLECT(CompileRun) void meConsoleCommandMetaGenerator(meCompileRunContext* ctx)
{
    if (ctx->stage == meCompileRunStage_VisitDecl)
    {
        if (ctx->reflectOp != STRING_LIT("ConsoleCommand"))
        {
            return;
        }
        CXCursorKind kind = clang_getCursorKind(ctx->cursor);
        if (kind != CXCursor_FunctionDecl)
        {
            return;
        }
        CXString spelling = clang_getCursorSpelling(ctx->cursor);
        const char *spellingStr = clang_getCString(spelling);
        LOG_INFO("Found ConsoleCommand: %s", spellingStr);
        clang_disposeString(spelling);
    }
}

#endif
