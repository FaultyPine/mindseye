#pragma once

#include "mindseye/core/me_string.h"

// TODO:
// use LLJIT instead of interpreter

#if __has_include("clang-c/Index.h")
#include "clang-c/Index.h"
#else
enum CXCursorKind : int;
typedef struct
{
    enum CXCursorKind kind;
    int xdata;
    const void *data[3];
} CXCursor;
typedef void *CXTranslationUnit;
#endif

// reflecting pass finds these MEREFLECT(CompileRun) markers and caches the cursor location of them
// then we do a second pass where we take the string content of each function and copypaste it into a new clang module for running
// That's why we need the functions to exist for reflection pass, but then we don't want redefinition errors when doing the compilerun pass 
// (because the funcs themselves are already copypasted elsewhere)
#if defined(ME_REFLECTING) && !defined(ME_COMPILE_RUN)
#define ME_COMPILERUN_GUARD 1
#else
#define ME_COMPILERUN_GUARD 0
#endif

namespace clang
{
class ASTContext;
class Decl;
class SourceManager;
}

enum meCompileRunStage
{
    meCompileRunStage_VisitDecl,
    meCompileRunStage_Finalize,
};

struct meCompileRunContext
{
    meCompileRunStage stage = meCompileRunStage_VisitDecl;
    CXCursor cursor = {};
    CXTranslationUnit translationUnit = {};
    StringView reflectOp = {};
    StringView macroContent = {};

    clang::Decl *decl = nullptr;
    clang::ASTContext *ast = nullptr;
    clang::SourceManager *sourceManager = nullptr;

    const char *projectRoot = nullptr;
    const char *outputDir = nullptr;
    const char *sourceFile = nullptr;
};
