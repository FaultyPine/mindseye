#define ME_CORE_ONLY

// Include STB_SPRINTF implementation for the reflector
#define STB_SPRINTF_IMPLEMENTATION
#define STBSP__PUBLICDEC extern "C"
#include "mindseye/external/stb/stb_sprintf.h"
#undef STB_SPRINTF_IMPLEMENTATION
#undef STBSP__PUBLICDEC

#include "mindseye/me_unity.cpp"

#include "clang/AST/AST.h"
#include "clang/AST/Type.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Interpreter/Interpreter.h"
#include "clang-c/Index.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"

#include "me_compile_run_api.h"
#include "reflection_types.h"

#include <memory>
#include <vector>

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

// NOTE(1/4/2026): ============== ON HANDLING TEMPLATED TYPES ===================== (how i've chosen to do it)
// when parsing, we store the full templated name, but when using it
// (I.E. outputting into generated headers, or using it for hashing)
// we remove the templated portion.
// We represent different instantiations of a templated type by a combo of the base type and a list of template params
// Like this:
// struct Something {
//	DynArray<int> arrayField;
// };
// becomes (pseudocode)
// TD_SOMETHING
//{
//	field->arrayField
//	{
//		.thisType = TD_DYNARRAY
//		.templateTypes = something_arrayfield_templateargs
//	}
//}
// typedescriptor something_arrayfield_templateargs = { TD_INT }
// ============================================================

// NOTE: the most common debugging that happens for this system
// is "there's a field that isn't right"
// for those, ctrl+f for "FOUND FIELD TO REFLECT", and do a field name check & breakpoint there

StringView TypeNameSanitize(StringView name)
{
    s32 templateArgsStart = FindInString(name, STRING_LIT("<"));
    if (templateArgsStart != -1)
    {
        // get rid of the template part of the name
        // since the way we handle templated types is to have the "base" type
        // defined without templates, and then a sidecar list of templated types that goes with it (meTypeDescriptor::thisType and templatedTypes)
        name = name.OffsetView(0, templateArgsStart);
    }
    return name;
}

// can represent structure types, field decls, others...
// Very similar to meTypeDescriptor, but this is used during reflection whereas meTypeDescriptor is used in the generated headers
// there is state we want to keep around during reflection we may not want to include in the generated headers, hence having two separate types
// EDIT: Just merge these two...
struct meReflectedType
{
    DynArray<meReflectedType *> children = {};
    StringView name = {};
    CXCursorKind kind = CXCursor_NoDeclFound;
    meReflectedType *innerType = nullptr; // for fields, this is their type
    DynArray<meReflectedType *> templateTypes = {};
    StringView editorName = {};
    StringView tooltip = {};
    u32 size = 0;
    s32 offsetBits = 0; // offset of this type from it's parent type, in bits
    s32 value = 0;
    u32 align = 0;
    u32 version = 0;
    meTypeDescriptorFlags flags = 0;
    StringView serializerFnName = {};
    StringView deserializerFnName = {};
    StringView equalsFnName = {};
    StringView editorRenderFnName = {};
    // for template specializations where the typical "name" field here isn't sufficient to fully describe the type
    // Empty for non-template or builtin types where name is sufficient.
    StringView fullTemplateName = {};

    void Print() const;
    bool operator==(const meReflectedType &other) const
    {
        return TypeNameSanitize(name) == TypeNameSanitize(other.name) &&
               size == other.size &&
               offsetBits == other.offsetBits &&
               align == other.align;
    }
    bool IsExcluded() const
    {
        return TEST_BIT(flags, meTypeDescriptorFlag_Excluded);
    }

    meReflectedType() = default;
    meReflectedType(meAllocator *allocator)
    {
        *this = meReflectedType();
        children = DynArrayCreate<meReflectedType *>(allocator);
        templateTypes = DynArrayCreate<meReflectedType *>(allocator);
    }
};

struct meReflectedFile
{
    String fileName;
    // name hash -> type
    meMap<u32, meReflectedType> reflectedTypes = {};
};

// a function tagged as CompileRun
struct meCompileRunDecl
{
    CXCursor cursor = clang_getNullCursor();
    StringView functionName = {};
    StringView functionSource = {};
    String sourceFile = {};
    u32 line = 0;
};

// any decl with MEREFLECT
struct meAnnotatedDecl
{
    CXCursor cursor = clang_getNullCursor();
    StringView op = {};
    StringView macroContent = {};
    String sourceFile = {};
    u32 line = 0;
};

struct ClangParsingContext
{
    Arena *allocator = nullptr;
    String projectRootDir = {};
    String headerOutputFolder = {};
    // header id -> file reflection info
    meMap<u32, meReflectedFile> reflectedFiles = {};
    DynArray<meCompileRunDecl> compileRunDecls = {};
    DynArray<meAnnotatedDecl> annotatedDecls = {};
    DynArray<const char *> clangArgs = {};
    CXTranslationUnit *tu = nullptr;
    // When non-null, field insertion targets this cursor's reflected type instead of the
    // field's lexical parent. Used to inject base class fields into derived types.
    CXCursor childrenTargetOverride = clang_getNullCursor();

    static ClangParsingContext &GetSingleInstance()
    {
        static ClangParsingContext g_parsingContext = {};
        return g_parsingContext;
    }
};

void meReflectedType::Print() const
{
    LOG_INFO("%.*s\n\t[excluded = %i] [editorName = %.*s] [tooltip = %.*s] offset = %i size = %u align = %u",
             STRING_VAARGS(name), IsExcluded(), STRING_VAARGS(editorName), STRING_VAARGS(tooltip), offsetBits, size, align);
    if (children)
    {
        s32 numChildren = DynArrayGetSize(children);
        for (s32 i = 0; i < numChildren; i++)
        {
            const meReflectedType *typeRefl = children[i];
            LOG_INFO("Child:");
            typeRefl->Print();
        }
    }
}

inline StringView GetCursorDisplayName(const CXCursor &cr, meAllocator *allocator)
{
    auto displayName = clang_getCursorDisplayName(cr);
    const char *strMem = clang_getCString(displayName);
    StringView result = StringView(strMem, CStringLength(strMem));
    // result.CopyOfCStr(strMem, allocator);
    // clang_disposeString(displayName);
    return result;
}

inline StringView GetCursorSpellingName(const CXCursor& cr, meAllocator *allocator)
{
    auto spelling = clang_getCursorSpelling(cr);
    const char *strMem = clang_getCString(spelling);
    StringView result = StringView(strMem, CStringLength(strMem));
    return result;
}

inline u32 GetLineNumberForCursor(const CXCursor &cr)
{
    uint32_t line, column, offset;
    CXSourceRange range = clang_getCursorExtent(cr);
    CXSourceLocation start = clang_getRangeStart(range);
    clang_getExpansionLocation(start, nullptr, &line, &column, &offset);
    return line;
}

String GetHeaderPathForCursor(CXCursor cr, meAllocator *allocator)
{
    CXFile file;
    CXSourceRange const cursorRange = clang_getCursorExtent(cr);
    clang_getExpansionLocation(clang_getRangeStart(cursorRange), &file, nullptr, nullptr, nullptr);
    String HeaderFilePath = {};
    if (file != nullptr)
    {
        CXString clangFilePath = clang_File_tryGetRealPathName(file);
        const char *filePathMem = clang_getCString(clangFilePath);
        HeaderFilePath.CopyOfCStr(filePathMem, allocator);
        meFsNormalizePathSeperators(HeaderFilePath);
        clang_disposeString(clangFilePath);
    }
    return HeaderFilePath;
}

void clang_checkDiagnostics(CXTranslationUnit &tu)
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
        LOG_WARN("line %u:%u %s", line, column, clang_getCString(clang_getDiagnosticSpelling(diagnostic)));
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
    bool isGeneratedHeader = FindInString(headerPath, STRING_LIT(".generated")) != -1;
    return inProjDir && !is3rdPartyLib && !isGeneratedHeader;
}

bool GetReflectedTypeHashes(
    CXCursor cr,
    meAllocator *allocator,
    ClangParsingContext &ctx,
    u32 &headerID,
    u32 &nameID)
{
    String headerPath = GetHeaderPathForCursor(cr, allocator);
    headerID = HashBytes((u8 *)headerPath.data, headerPath.len);
    if (cr.kind == CXCursor_ClassTemplate)
    {
        // for templated types, we can't get the cursor "type" - it's invalid since it isn't instantiated
        StringView typeName = GetCursorDisplayName(cr, allocator);
        typeName = TypeNameSanitize(typeName);
        nameID = HashBytes((u8 *)typeName.data, typeName.len);
    }
    else
    {
        CXType type = clang_getCursorType(cr);
        if (type.kind == CXType_Invalid)
        {
            return false;
        }
        StringView typeStr = StringFromCString(clang_getCString(clang_getTypeSpelling(type)));
        typeStr = TypeNameSanitize(typeStr);
        nameID = HashBytes((u8 *)typeStr.data, typeStr.len);
    }
    return true;
}

static std::unordered_map<CXTypeKind, meTypeDescriptor *> clangToMePrimitiveType =
    {
        {CXType_Bool, &TD_BOOL},
        {CXType_Char_U, &TD_UNSIGNED_CHAR},
        {CXType_UChar, &TD_UNSIGNED_CHAR},
        {CXType_Char16, &TD_SHORT},
        {CXType_Char32, &TD_INT},
        {CXType_Enum, &TD_INT}, // NOTE: enums are mapped to int. This may need to be revisited later
        {CXType_UShort, &TD_UNSIGNED_SHORT},
        {CXType_UInt, &TD_UNSIGNED_INT},
        {CXType_ULong, &TD_UNSIGNED_LONG},
        {CXType_ULongLong, &TD_UNSIGNED_LONG_LONG},
        {CXType_Char_S, &TD_CHAR},
        {CXType_SChar, &TD_CHAR},
        {CXType_WChar, &TD_WCHAR},
        {CXType_Short, &TD_SHORT},
        {CXType_Int, &TD_INT},
        {CXType_Long, &TD_LONG},
        {CXType_LongLong, &TD_LONGLONG},
        {CXType_Float, &TD_FLOAT},
        {CXType_Double, &TD_DOUBLE},
};

// TD_MEASSET is a generated type and is not available in the reflector binary.
// Only the .name field matters here: TransferRelevantReflectedTypeInfoToTypeDescriptor copies it,
// and SanitizeAndCopyTypeDescriptorName then uppercases it to produce "TD_MEASSET" in emitted code.
static meTypeDescriptor s_meAssetBuiltinStub = {.name = STRING_LIT("meAsset")};

static std::unordered_map<StringView, meTypeDescriptor *> builtinStructs =
    {
        {STRING_LIT("String"), &TD_STRING},
        {STRING_LIT("StringView"), &TD_STRINGVIEW},
        {STRING_LIT("meSpan"), &TD_SPAN},
        {STRING_LIT("DynArray"), &TD_DYNARRAY},
        {STRING_LIT("glm::vec"), &TD_VEC3},
        {STRING_LIT("glm::qua"), &TD_QUAT},
        // meTypedAsset<T> inherits from meAsset and has the same binary layout.
        {STRING_LIT("meTypedAsset"), &s_meAssetBuiltinStub},
};

meTypeDescriptor *MapClangPrimitiveTypeToTypeDescriptor(
    CXCursor cr)
{
    CXType type = clang_getCursorType(cr);
    CXTypeKind kind = type.kind;
    if (kind == CXType_Typedef)
    {
        kind = clang_getTypedefDeclUnderlyingType(cr).kind;
    }
    if (kind == CXType_Elaborated)
    {
        type = clang_getCanonicalType(type);
        kind = type.kind;
    }
    meTypeDescriptor *result = nullptr;
    if (kind == CXType_Record)
    {
        CXString typeSpelling = clang_getTypeSpelling(type);
        const char *typeName = clang_getCString(typeSpelling);
        StringView typeNameStr = StringView(typeName, CStringLength(typeName));
        typeNameStr = TypeNameSanitize(typeNameStr);
        if (builtinStructs.count(typeNameStr))
        {
            result = builtinStructs.at(typeNameStr);
        }
        clang_disposeString(typeSpelling);
    }
    else if (kind == CXType_ConstantArray)
    {
        // for constant arrays, the "inner type" is the type of the array
        // and the size of the type is the actual byte size of the array.
        // if you want the number of elements in the array, take the total type size and divide by the inner type's size
        CXType internalArrayType = clang_getArrayElementType(type);
        CXCursor internalArrayCursor = clang_getTypeDeclaration(internalArrayType);
        meTypeDescriptor *internalArrayDescriptor = MapClangPrimitiveTypeToTypeDescriptor(internalArrayCursor);
        ME_ASSERT(internalArrayDescriptor);
        return internalArrayDescriptor;
    }
    else if (clangToMePrimitiveType.count(kind))
    {
        result = clangToMePrimitiveType.at(kind);
    }
    return result;
}

meReflectedType TransferRelevantReflectedTypeInfoToTypeDescriptor(
    meTypeDescriptor *typeDesc,
    meAllocator *allocator)
{
    const meTypeDescriptor &other = *typeDesc;
    meReflectedType result;
    result.children = DynArrayCreate<meReflectedType *>(allocator);
    result.templateTypes = DynArrayCreate<meReflectedType *>(allocator);
    result.name = other.name;
    result.editorName = other.editorName;
    result.tooltip = other.tooltip;
    result.version = other.version;
    result.size = other.size;
    result.offsetBits = other.offsetBits;
    result.align = other.align;
    result.flags = other.flags;
    result.value = other.value;
    // result.innerType = other.thisType;
    return result;
}

bool IsPrimitiveType(CXTypeKind kind)
{
    return (kind >= CXType_FirstBuiltin && kind <= CXType_LastBuiltin) || (kind == CXType_Pointer);
}

bool IsBuiltinType(CXCursor cr)
{
    CXType type = clang_getCursorType(cr);
    CXCursor typeDecl = clang_getTypeDeclaration(type);
    CXType underlyingtype = clang_getCursorType(typeDecl);
    CXTypeKind kind = underlyingtype.kind;
    meTypeDescriptor *builtinTypeDesc = MapClangPrimitiveTypeToTypeDescriptor(cr);
    if (kind == CXType_Typedef)
    {
        kind = clang_getTypedefDeclUnderlyingType(typeDecl).kind;
    }
    return builtinTypeDesc != nullptr || IsPrimitiveType(kind);
}

meReflectedType *GetReflectedType(
    CXCursor cr,
    meAllocator *allocator,
    ClangParsingContext &ctx)
{
    String headerPath = GetHeaderPathForCursor(cr, allocator);
    u32 headerID = 0;
    u32 nameID = 0;
    if (!GetReflectedTypeHashes(cr, allocator, ctx, headerID, nameID))
    {
        return nullptr;
    }
    meReflectedType &reflType = ctx.reflectedFiles[headerID].reflectedTypes[nameID];
    if (!reflType.children)
    {
        reflType.children = DynArrayCreate<meReflectedType *>(allocator);
        reflType.templateTypes = DynArrayCreate<meReflectedType *>(allocator);
    }
    ctx.reflectedFiles[headerID].fileName = headerPath;
    meTypeDescriptor *builtinTypeDesc = MapClangPrimitiveTypeToTypeDescriptor(cr);
    if (builtinTypeDesc)
    {
        reflType = TransferRelevantReflectedTypeInfoToTypeDescriptor(builtinTypeDesc, allocator);
    }
    return &reflType;
}

bool DoesDeclarationHaveReflectionAnnotation(
    CXCursor typeDecl,
    ClangParsingContext &ctx)
{
    if (typeDecl.kind == CXCursor_NoDeclFound)
    {
        return false;
    }
    u32 result = clang_visitChildren(typeDecl, +[](CXCursor cr, CXCursor parent, CXClientData clientData)
                                               {
		CXCursorKind kind = clang_getCursorKind(cr);
		switch (kind)
		{
			case CXCursor_AnnotateAttr:
			{
				ClangParsingContext& ctx = *(ClangParsingContext*)clientData;
				StringView cursorName = GetCursorDisplayName(cr, ctx.allocator);
				// when the cursor is the reflection attribute, the parent cursor is the one with the actual decl we care about
				if (cursorName == STRING_LIT(ME_REFLECT_ATTR_STR))
				{
					return CXChildVisit_Break; // indicates to our code just below (result == 0) that we found an attribute, and already parsed this decl
				}
			}
			break;
			default: break;
		}
		return CXChildVisit_Continue; },
                                     &ctx);
    bool foundReflectionAnnotation = result != 0;
    return foundReflectionAnnotation;
}

// when we encounter a MEREFLECT macro, the entire content inside it is passed in here
// I.E. MEREFLECT(something, another)     "something, another" would be passed in macroContent
// In that case ^ the cursor points to either a fielddecl or structdecl that has been annotated
// This also processes fielddecls that won't have the macro on them. For those,
// the macroContent is empty and the cursor points to the fielddecl.
void StoreReflectedTypeInfo(
    CXCursor cr,
    ClangParsingContext &ctx,
    StringView macroContent)
{
    meAllocator *allocator = ctx.allocator;

    CXCursorKind crKind = clang_getCursorKind(cr);
    CXCursor parentCr = clang_getCursorLexicalParent(cr);
    StringView cursorName = GetCursorDisplayName(cr, allocator);
    CXType crType = clang_getCursorType(cr);
    meReflectedType *parentReflType = GetReflectedType(parentCr, allocator, ctx);

    bool excluded = false;
    if (FindInString(macroContent, STRING_LIT("exclude")) > -1)
    {
        excluded = true;
    }

    // FOUND FIELD TO REFLECT

    meReflectedType *reflTypePtr = nullptr;
    // for fields, fill out the inner type, and add this type to the parent struct's children
    if (crKind == CXCursor_FieldDecl)
    {
        CXCursor fieldTypeCr = clang_getTypeDeclaration(crType);
        bool isBuiltin = IsBuiltinType(fieldTypeCr) || fieldTypeCr.kind == CXCursor_NoDeclFound;
        if (isBuiltin)
        {
            // primitive type. I.E. u32
            fieldTypeCr = cr;
        }
        else
        {
            // non-builtin/primitive and unknown. Likely an external type we won't include in the final generated output
            StoreReflectedTypeInfo(fieldTypeCr, ctx, {});
        }
        // annotated fields have their parentCr as the fielddecl. Unannotated fields have their parentCr as the struct decl
        bool isReflectedType = DoesDeclarationHaveReflectionAnnotation(fieldTypeCr, ctx);
        meReflectedType *fieldInnerTypeRefl = GetReflectedType(fieldTypeCr, allocator, ctx);
        SET_BIT(fieldInnerTypeRefl->flags, meTypeDescriptorFlag_Excluded, fieldInnerTypeRefl->IsExcluded() || excluded);

        meReflectedType &fieldMemberRefl = *MENEW(ctx.allocator, meReflectedType, ctx.allocator); // this will contain the field's type info
        // if it's a const array, mark the field as such. It's inner type will indicate what it's an array of, and it's size / sizeof(inner type) indicates the num elements in the array
        if (clang_getCursorType(fieldTypeCr).kind == CXType_ConstantArray)
        {
            SET_BIT(fieldMemberRefl.flags, meTypeDescriptorFlag_ConstantArray, true);
        }

        meReflectedType* childrenTarget = (!clang_Cursor_isNull(ctx.childrenTargetOverride))
            ? GetReflectedType(ctx.childrenTargetOverride, allocator, ctx)
            : parentReflType;
        if (childrenTarget)
        {
            for (DynArray_Foreach(childrenTarget->children, childIdx))
            {
                if (childrenTarget->children[childIdx]->name == cursorName)
                {
                    return;
                }
            }
            DynArrayPush(childrenTarget->children, &fieldMemberRefl);
        }

        fieldMemberRefl.innerType = fieldInnerTypeRefl;
        reflTypePtr = &fieldMemberRefl;
        if (isBuiltin || isReflectedType)
        {
            // for non-primitive builtin types (I.E. String) we should include those
            SET_BIT(reflTypePtr->flags, meTypeDescriptorFlag_IncludeInGeneratedHeader, true);
        }

        // keep an optional list of template types inside each type.
        // I.E. map<int, char> would have TD_INT and TD_CHAR entries in the templated types list
        int numTemplateArgs = clang_Type_getNumTemplateArguments(crType);
        if (numTemplateArgs != -1)
        {
            // The type is a template specialization (e.g., std::vector<int>)
            for (s32 i = 0; i < numTemplateArgs; i++)
            {
                CXType templateType = clang_Type_getTemplateArgumentAsType(crType, i);
                if (templateType.kind == CXType_Invalid)
                {
                    // Non-type template arg (e.g. an enum value like MAMesh in meTypedAsset<MAMesh>).
                    // clang_Type_getTemplateArgumentAsType can't handle these, but the cursor-level
                    // API can extract integral values directly from the specialisation cursor.
                    CXCursor specialCr = clang_getTypeDeclaration(crType);
                    int numCursorArgs = clang_Cursor_getNumTemplateArguments(specialCr);
                    bool handledAsIntegral = false;
                    if (i < (u32)numCursorArgs &&
                        clang_Cursor_getTemplateArgumentKind(specialCr, i) == CXTemplateArgumentKind_Integral)
                    {
                        long long integralValue = clang_Cursor_getTemplateArgumentValue(specialCr, i);
                        meReflectedType &integralArgRefl = *MENEW(allocator, meReflectedType, allocator);
                        integralArgRefl.value = (s32)integralValue;
                        SET_BIT(integralArgRefl.flags, meTypeDescriptorFlag_IntegralStub, true);
                        DynArrayPush(reflTypePtr->templateTypes, &integralArgRefl);
                        handledAsIntegral = true;
                    }
                    if (!handledAsIntegral && reflTypePtr->innerType == nullptr)
                    {
                        LOG_WARN("incomplete template arg for " STRING_FMT, STRING_VAARGS(reflTypePtr->name));
                    }
                    continue;
                }
                CXCursor templateTypeCr = clang_getTypeDeclaration(templateType);
                meReflectedType *templateInnerType = GetReflectedType(templateTypeCr, allocator, ctx);
                if (templateInnerType)
                {
                    // If this template arg is itself a template specialization create a fresh meReflectedType so we can
                    // attach the nested template args without changing the cached reflected type.
                    int numNestedTemplateArgs = clang_Type_getNumTemplateArguments(templateType);
                    if (numNestedTemplateArgs > 0)
                    {
                        meReflectedType &nestedRefl = *MENEW(allocator, meReflectedType, allocator);
                        nestedRefl.name = templateInnerType->name;
                        nestedRefl.size  = (u32)clang_Type_getSizeOf(templateType);
                        nestedRefl.align = (u32)clang_Type_getAlignOf(templateType);
                        nestedRefl.flags = templateInnerType->flags;
                        nestedRefl.children = DynArrayCreate<meReflectedType *>(allocator);
                        nestedRefl.templateTypes = DynArrayCreate<meReflectedType *>(allocator);
                        CXString fullSpelling = clang_getTypeSpelling(templateType);
                        const char* fullSpellingStr = clang_getCString(fullSpelling);
                        String fullSpellingCopy = {};
                        fullSpellingCopy.CopyOfCStr(fullSpellingStr, allocator);
                        nestedRefl.fullTemplateName = fullSpellingCopy;
                        clang_disposeString(fullSpelling);

                        CXCursor nestedSpecialCr = templateTypeCr; // same cursor
                        int numCursorNested = clang_Cursor_getNumTemplateArguments(nestedSpecialCr);

                        for (s32 j = 0; j < numNestedTemplateArgs; j++)
                        {
                            CXType nestedTemplateType = clang_Type_getTemplateArgumentAsType(templateType, j);
                            if (nestedTemplateType.kind == CXType_Invalid)
                            {
                                // Non-type (integral/enum) arg
                                if (j < numCursorNested &&
                                    clang_Cursor_getTemplateArgumentKind(nestedSpecialCr, j) == CXTemplateArgumentKind_Integral)
                                {
                                    long long integralValue = clang_Cursor_getTemplateArgumentValue(nestedSpecialCr, j);
                                    meReflectedType &integralArgRefl = *MENEW(allocator, meReflectedType, allocator);
                                    integralArgRefl.value = (s32)integralValue;
                                    SET_BIT(integralArgRefl.flags, meTypeDescriptorFlag_IntegralStub, true);
                                    DynArrayPush(nestedRefl.templateTypes, &integralArgRefl);
                                }
                            }
                            else
                            {
                                CXCursor nestedArgCr = clang_getTypeDeclaration(nestedTemplateType);
                                meReflectedType *nestedArgType = GetReflectedType(nestedArgCr, allocator, ctx);
                                if (nestedArgType)
                                {
                                    DynArrayPush(nestedRefl.templateTypes, nestedArgType);
                                }
                            }
                        }
                        DynArrayPush(reflTypePtr->templateTypes, &nestedRefl);
                    }
                    else
                    {
                        DynArrayPush(reflTypePtr->templateTypes, templateInnerType);
                    }
                }
            }
            if (numTemplateArgs > 1 && reflTypePtr->innerType == nullptr)
            {
                LOG_ERROR("Haven't yet implemented multi-template arg reflection | " STRING_FMT, STRING_VAARGS(reflTypePtr->name));
                UNIMPLEMENTED();
            }
        }
    }
    else
    {
        reflTypePtr = GetReflectedType(cr, allocator, ctx);
        SET_BIT(reflTypePtr->flags, meTypeDescriptorFlag_IncludeInGeneratedHeader, true);
    }
    ME_ASSERT(reflTypePtr);
    meReflectedType &reflType = *reflTypePtr;

    if (reflType.kind != CXCursor_NoDeclFound) // already parsed this type
    {
        return;
    }
    CXType parentType = clang_getCursorType(parentCr);

    s64 typeSize = clang_Type_getSizeOf(crType);

    // This can happen if a type depends on templates or other types that haven't been fully resolved
    if ((crKind == CXCursor_StructDecl || crKind == CXCursor_ClassDecl) &&
        typeSize <= 1)
    {
        String headerPath = GetHeaderPathForCursor(cr, allocator);
        u32 lineNum = GetLineNumberForCursor(cr);
        LOG_WARN("Encountered incomplete type '%.*s' (size=%lli, line %u) in '%.*s'. This may indicate missing includes, unresolved template dependencies, or forward declarations. "
                 "Check that all required headers are included and templates are properly instantiated.",
                 STRING_VAARGS(cursorName), typeSize, lineNum, STRING_VAARGS(headerPath));
        // Don't assert - just return early since we can't properly reflect an incomplete type
        // return;
    }

    s32 typeSizeBits = clang_getFieldDeclBitWidth(cr);
    if (typeSizeBits != -1)
    {
        StringView bitfieldTypeName = GetCursorDisplayName(parentCr, allocator);
        LOG_ERROR("Bitfields are not supported in reflected types. %.*s::%.*s", STRING_VAARGS(bitfieldTypeName), STRING_VAARGS(cursorName));
        return;
    }
    s64 typeAlign = clang_Type_getAlignOf(crType);
    const char *fieldName = CStringFromString(cursorName, allocator);
    s64 offset = clang_Type_getOffsetOf(parentType, fieldName);
    if (offset < 0)
    {
        // invalid offset, happens when reflecting on a struct type, since the struct decl itself has no offset
        // so, this is only valid when we're reflecting on a *field inside* a struct, which only happens
        // when we add additional reflection markup on a field, like a tooltip, description, exclusion, etc
        // not a fatal error, just documenting this quirk
    }
    reflType.name = TypeNameSanitize(cursorName);
    reflType.size = typeSize;
    reflType.offsetBits = offset;
    reflType.align = typeAlign;
    reflType.kind = crKind;

    // if there's an annotation, parse the content
    auto GetStringParam = [](StringView key, StringView macroContent) -> StringView
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
        s32 endParamStrContent = 0;
        // string params like Description="something"
        if (param[0] == '"')
        {
            param = EatChars(param, '"');
            endParamStrContent = EatCharsOffset(param, '"', true);
        }
        // non-string params like Version=1
        else
        {
            endParamStrContent = MEMIN(EatCharsOffset(param, ',', true), EatCharsOffset(param, ')', true));
        }
        StringView paramStrContent = param.OffsetView(0, endParamStrContent);
        return paramStrContent;
    };

    if (macroContent)
    {
        StringView descriptionParam = GetStringParam(STRING_LIT("Description"), macroContent);
        reflType.editorName = descriptionParam;

        StringView tooltipParam = GetStringParam(STRING_LIT("Tooltip"), macroContent);
        reflType.tooltip = tooltipParam;

        StringView versionParam = GetStringParam(STRING_LIT("Version"), macroContent);
        if (versionParam)
            reflType.version = StringToUint(versionParam);

        StringView serializerParam = GetStringParam(STRING_LIT("Serializer"), macroContent);
        reflType.serializerFnName = serializerParam;

        StringView deserializerParam = GetStringParam(STRING_LIT("Deserializer"), macroContent);
        reflType.deserializerFnName = deserializerParam;

        StringView equalsFnParam = GetStringParam(STRING_LIT("Equals"), macroContent);
        reflType.equalsFnName = equalsFnParam;

        StringView editorRenderParam = GetStringParam(STRING_LIT("EditorRender"), macroContent);
        reflType.editorRenderFnName = editorRenderParam;

        SET_BIT(reflType.flags, meTypeDescriptorFlag_Excluded, reflType.IsExcluded() || excluded);
    }
}

StringView ParseReflectionMacroContent(CXCursor cr, CXTranslationUnit &tu)
{
    CXSourceRange range = clang_getCursorExtent(cr);
    CXSourceLocation start = clang_getRangeStart(range);

    CXFile file;
    unsigned line;
    unsigned col;
    unsigned offset;
    clang_getExpansionLocation(start, &file, &line, &col, &offset);

    u64 filesize = 0;
    const char *filecontentCStr = clang_getFileContents(tu, file, &filesize);
    StringView filecontent = {filecontentCStr, filesize};

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

StringView ParseReflectionMacroOp(StringView macroContent)
{
    u64 start = 0;
    while (start < macroContent.len && (macroContent[start] == ' ' || macroContent[start] == '\t' || macroContent[start] == '\n' || macroContent[start] == '\r'))
    {
        start++;
    }
    u64 end = start;
    while (end < macroContent.len && macroContent[end] != ',' && macroContent[end] != ' ' && macroContent[end] != '\t' && macroContent[end] != '\n' && macroContent[end] != '\r')
    {
        end++;
    }
    return macroContent.OffsetView(start, end - start);
}

void StoreAnnotatedDecl(CXCursor declCursor, ClangParsingContext& ctx, StringView macroContent)
{
    meAnnotatedDecl decl = {};
    decl.cursor = declCursor;
    decl.op = ParseReflectionMacroOp(macroContent);
    decl.macroContent = macroContent;
    decl.sourceFile = GetHeaderPathForCursor(declCursor, ctx.allocator);
    decl.line = GetLineNumberForCursor(declCursor);
    DynArrayPush(ctx.annotatedDecls, decl);
}

StringView ExtractCursorSource(CXCursor cr, CXTranslationUnit& tu)
{
    CXSourceRange range = clang_getCursorExtent(cr);
    CXSourceLocation start = clang_getRangeStart(range);
    CXSourceLocation end = clang_getRangeEnd(range);

    CXFile startFile;
    CXFile endFile;
    unsigned startLine;
    unsigned startCol;
    unsigned startOffset;
    unsigned endLine;
    unsigned endCol;
    unsigned endOffset;
    clang_getExpansionLocation(start, &startFile, &startLine, &startCol, &startOffset);
    clang_getExpansionLocation(end, &endFile, &endLine, &endCol, &endOffset);
    if (startFile != endFile || endOffset <= startOffset)
    {
        return {};
    }

    u64 filesize = 0;
    const char *filecontentCStr = clang_getFileContents(tu, startFile, &filesize);
    if (!filecontentCStr || endOffset > filesize)
    {
        return {};
    }

    return StringView(filecontentCStr + startOffset, endOffset - startOffset);
}

bool StoreCompileRunDecl(CXCursor functionCursor, ClangParsingContext& ctx)
{
    CXCursorKind kind = clang_getCursorKind(functionCursor);
    if (kind != CXCursor_FunctionDecl)
    {
        LOG_ERROR("MEREFLECT(CompileRun) is only supported on free functions for now");
        return false;
    }

    meCompileRunDecl decl = {};
    decl.cursor = functionCursor;
    decl.functionName = GetCursorSpellingName(functionCursor, ctx.allocator);
    decl.functionSource = ExtractCursorSource(functionCursor, *ctx.tu);
    decl.sourceFile = GetHeaderPathForCursor(functionCursor, ctx.allocator);
    decl.line = GetLineNumberForCursor(functionCursor);
    if (!decl.functionSource)
    {
        LOG_ERROR("Failed to extract source for compile-run function %.*s", STRING_VAARGS(decl.functionName));
        return false;
    }

    DynArrayPush(ctx.compileRunDecls, decl);
    return true;
}

bool LogLLVMError(llvm::Error err, const char *context)
{
    if (!err)
    {
        return false;
    }
    std::string msg = llvm::toString(std::move(err));
    LOG_ERROR("%s: %s", context, msg.c_str());
    return true;
}

CXVisitorResult VisitStructureFields(CXCursor cr, CXClientData clientData);

// a decl we care about parsing/storing in the reflection data
void OnFindInterestingDecl(CXCursor cr, CXCursor parent, CXClientData clientData)
{
#if !defined(ME_REFLECT_ATTR_STR) || !defined(MEREFLECT)
#error Undefined MEREFLECT attribute str/macro... include me_defines.h
#endif
    ClangParsingContext &ctx = *(ClangParsingContext *)clientData;
    StringView cursorName = GetCursorDisplayName(cr, ctx.allocator);
    // when the cursor is the reflection attribute, the parent cursor is the one with the actual decl we care about
    if (cursorName == STRING_LIT(ME_REFLECT_ATTR_STR))
    {
        StringView macroContents = ParseReflectionMacroContent(parent, *ctx.tu);
        StoreAnnotatedDecl(parent, ctx, macroContents);
        if (ParseReflectionMacroOp(macroContents) == STRING_LIT("CompileRun"))
        {
            StoreCompileRunDecl(parent, ctx);
            return;
        }

        StoreReflectedTypeInfo(parent, ctx, macroContents);

        // since this is called for a structure decl, OR on a fielddecl...
        CXCursorKind parentKind = clang_getCursorKind(parent);
        if (parentKind == CXCursor_StructDecl || parentKind == CXCursor_ClassDecl)
        {
            // Inject base class fields FIRST so they precede direct fields in the children list.
            // Offsets are computed against the base class type (correct for standard-layout structs
            // where the base subobject is always at offset 0 in the derived class).
            clang_visitChildren(parent, +[](CXCursor cr, CXCursor parent, CXClientData clientData) -> CXChildVisitResult {
                if (clang_getCursorKind(cr) != CXCursor_CXXBaseSpecifier)
                    return CXChildVisit_Continue;
                ClangParsingContext& ctx = *(ClangParsingContext*)clientData;
                CXType baseType = clang_getCursorType(cr);
                ctx.childrenTargetOverride = parent;
                clang_Type_visitFields(baseType, VisitStructureFields, clientData);
                ctx.childrenTargetOverride = clang_getNullCursor();
                return CXChildVisit_Continue;
            }, clientData);

            CXType parentType = clang_getCursorType(parent);
            clang_Type_visitFields(parentType, VisitStructureFields, clientData);
        }
        // else
        // {
        // 	CXString parentKindStr = clang_getCursorKindSpelling(parentKind);
        // 	const char* parentKindCStr = clang_getCString(parentKindStr);
        // 	LOG_ERROR("Unsupported reflection annotation on cursor %.*s type %s", STRING_VAARGS(cursorName), parentKindCStr);
        // }
    }
    else
    {
        // otherwise, cr is the cursor we care about
        StoreReflectedTypeInfo(cr, ctx, {});
    }
}

CXVisitorResult VisitStructureFields(CXCursor cr, CXClientData clientData)
{
    // ClangParsingContext& ctx = *(ClangParsingContext*)clientData;
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
						return CXChildVisit_Break; // indicates to our code just below (result == 0) that we found an attribute, and already parsed this decl
					}
					break;
					default: break;
				}
				return CXChildVisit_Continue; },
                                         clientData);
        if (result == 0)
        {
            // this means we didn't find an annotation on the field, and have yet to parse it
            OnFindInterestingDecl(cr, parent, clientData);
        }
    }
    break;
    default:
        break;
    }

    return CXVisit_Continue;
}

CXChildVisitResult visitTranslationUnit(CXCursor cr, CXCursor parent, CXClientData clientData)
{
    ClangParsingContext &ctx = *(ClangParsingContext *)clientData;
    Arena *allocator = ctx.allocator;
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
        OnFindInterestingDecl(cr, parent, clientData);
        return CXChildVisit_Continue;
    }
    break;

        // other potential TODOs to support...
        // - reflected structs inside namespaces
        // - templated reflection?

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

DynArray<CompileCommand> CompileDatabaseToCommandsList(
    meAllocator *allocator,
    StringView compileDatabasePath)
{
    DynArray<CompileCommand> cmds = DynArrayCreate<CompileCommand>(allocator);
    OSFileReference compileCmdsFile = {};
    if (!meOSOpenFile(compileCmdsFile, compileDatabasePath, OSFileFlags_OnlyIfExists))
    {
        LOG_ERROR("Failed to open compile commands database file %s", compileDatabasePath.data);
        return cmds;
    }
    u64 fileSize = meOSGetFileSize(compileCmdsFile);
    char *compileCmdsContentsMem = (char *)MEALLOC(allocator, fileSize);
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
    s32 startFileIdx = FindInStringRev(compileCmdsContentsStr, STRING_LIT(" "), compileCmdsContentsStr.len - endFileIdx) + 1;
    cmd.inFile = compileCmdsContentsStr.OffsetView(startFileIdx, endFileIdx - startFileIdx);
    DynArrayPush(cmds, cmd);
    return cmds;
}

void GeneratedReflectionHeaders(ClangParsingContext &ctx, const char *headerOutputFolder);
bool RunCompileRunDecls(ClangParsingContext& ctx);

// ============================================================================
// ABOVE: Parsing the clang translation unit for reflection-annotated types & gathering the data
// ================================================================================
int main(int argc, char *argv[])
{
    InitializeLogger();
    if (argc < 4)
    {
        LOG_ERROR(
            "Not enough args...\n"
            "Arg1 should be path to compile_command.txt\n"
            "Arg2 should be the project's root directory\n"
            "Arg3 should be the output directory for generated header files");
        return 1;
    }
    StringView compileCmdsDatabaseFilePath = StringFromCString(argv[1]);
    LOG_INFO("[Mindseye Reflector] Reflecting %.*s", STRING_VAARGS(compileCmdsDatabaseFilePath));
    const char *projectRootDir = argv[2];
    const char *headerOutputFolder = argv[3];

    meAllocator *systemAllocator = GetSystemAllocator();
    Arena &reflectorArena = *MENEW(systemAllocator, Arena);
    reflectorArena = ArenaInit(MEGABYTES_BYTES(100ull), "Main reflector arena", systemAllocator);
    // create a header file on disk that is a sort of "unity" build single file that includes all the files we want to run our reflection parser on
    DynArray<CompileCommand> compileCommands = CompileDatabaseToCommandsList(&reflectorArena, compileCmdsDatabaseFilePath);
    if (DynArrayGetSize(compileCommands) == 0)
    {
        LOG_WARN("[Reflector] No compile commands found");
        return 0;
    }

    const char *reflectorHeaderFilename = "Reflector.h";
    DynArray<char> reflectorFilePath = DynArrayCreate<char>(&reflectorArena, 50);
    StringView exePath = meOSGetExeFileFolder();
    // we expect Reflector.h to be next to the reflector executable
    DynArrayPush(reflectorFilePath, exePath.data, exePath.len);
    if (exePath[exePath.len - 1] != '\\' && exePath[exePath.len - 1] != '/')
        DynArrayPush(reflectorFilePath, '/');
    DynArrayPush(reflectorFilePath, (char *)reflectorHeaderFilename, CStringLength(reflectorHeaderFilename));
    auto idx = clang_createIndex(0, 0);
    u32 clangOptions = 0 | CXTranslationUnit_DetailedPreprocessingRecord | CXTranslationUnit_IncludeBriefCommentsInCodeCompletion | CXTranslationUnit_KeepGoing
        //| CXTranslationUnit_SingleFileParse
        ;

    DynArray<const char *> clangArgs = DynArrayCreate<const char *>(&reflectorArena, 20);
    String absProjectRootPath = meOSResolveRelativeToAbsPath(&reflectorArena, StringFromCString(projectRootDir));
    meFsNormalizePathSeperators(absProjectRootPath);
    StringView clangIncludePath = StringFormatNew(&reflectorArena, "-I%s/../tools/clang/include", absProjectRootPath.cstr());
    DynArrayPush(clangArgs, clangIncludePath.cstr());

    u32 numCompileCommands = DynArrayGetSize(compileCommands);
    for (u32 i = 0; i < numCompileCommands; i++)
    {
        u32 stridx = 0;
        StringView args = compileCommands[i].arguments;
        while (stridx < args.len && stridx >= 0)
        {
            StringView currentArgsView = args.OffsetView(stridx);
            stridx += EatCharsOffset(currentArgsView, ' ');
            s32 nextSpace = FindInString(args, STRING_LIT(" "), stridx);
            if (nextSpace < 0)
                break;
            s32 len = nextSpace - stridx;
            StringView subArg = args.OffsetView(stridx, len);

            bool isDefineFlag = FindInString(subArg, STRING_LIT("-D")) != -1;
            bool isIncludeDir = FindInString(subArg, STRING_LIT("-I")) != -1;
            bool isStdVer = FindInString(subArg, STRING_LIT("-std")) != -1;
            if (isDefineFlag || isIncludeDir || isStdVer)
            {
                void *strmem = MEALLOC(&reflectorArena, len + 1);
                ME_MEMCLEAR(strmem, len + 1);
                ME_MEMCPY(strmem, args.data + stridx, len);
                DynArrayPush(clangArgs, (const char *)strmem);
            }
            stridx = nextSpace;
        }
    }

    DynArrayPush(clangArgs, "-x");
    DynArrayPush(clangArgs, "c++");
    DynArrayPush(clangArgs, "-DME_REFLECTING");

    CXTranslationUnit tu;
    CXErrorCode result = CXError_Failure;
    {
        result = clang_parseTranslationUnit2(idx, reflectorFilePath, clangArgs, DynArrayGetSize(clangArgs), 0, 0, clangOptions, &tu);
    }
    ClangParsingContext &ctx = ClangParsingContext::GetSingleInstance();
    ctx.tu = &tu;
    ctx.allocator = &reflectorArena;
    ctx.compileRunDecls = DynArrayCreate<meCompileRunDecl>(&reflectorArena);
    ctx.annotatedDecls = DynArrayCreate<meAnnotatedDecl>(&reflectorArena);
    ctx.clangArgs = clangArgs;
    ctx.projectRootDir = absProjectRootPath;
    ctx.headerOutputFolder.CopyOfCStr(headerOutputFolder, &reflectorArena);
    // clang_checkDiagnostics(tu);
    if (result == CXError_Success)
    {
        auto cursor = clang_getTranslationUnitCursor(tu);
        // populates the parsingcontext with info about all reflected types
        clang_visitChildren(cursor, visitTranslationUnit, &ctx);
        if (!RunCompileRunDecls(ctx))
        {
            return 1;
        }
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

bool ProcessReflectedFile(
    const meReflectedFile &fileRefl,
    const char *headerOutputFolder,
    meAllocator *allocator);

// given the ClangParsingContext that is filled with all the relevant data, generate headers
void GeneratedReflectionHeaders(
    ClangParsingContext &ctx,
    const char *headerOutputFolder)
{
    meOSEnsureDirectoriesExist(headerOutputFolder);

    StringBuilder sb(ctx.allocator);

    u32 numReflectedFiles = ctx.reflectedFiles.size();
    u32 numProcessedFiles = 0;
    for (const auto &[headerID, fileReflection] : ctx.reflectedFiles)
    {
        // TODO: this is ripe for super easy parallelism here
        // chunk up allocators for each thread, and have them all generate & write out each header
        Arena fileArena = ArenaInit(ArenaGetFreeSpace(ctx.allocator) / numReflectedFiles, "File Reflection Arena", ctx.allocator);
        bool didGenerate = ProcessReflectedFile(fileReflection, headerOutputFolder, &fileArena);
        numProcessedFiles += didGenerate ? 1 : 0;

        if (!fileReflection.reflectedTypes.empty() /*&& didGenerate*/)
        {
            StringView parsedHeaderExistingPath = fileReflection.fileName;
            if (!parsedHeaderExistingPath || FindInString(parsedHeaderExistingPath, StringFromCString(headerOutputFolder)) != -1)
            {
                continue;
            }
            StringView parsedHeaderFilename = meFsGetFileFromFullPath(parsedHeaderExistingPath);
            s32 extensionIdx = FindInStringRev(parsedHeaderFilename, STRING_LIT("."));
            StringView parsedHeaderFilenameNoExt = parsedHeaderFilename.OffsetView(0, extensionIdx);
            StringView includeText = StringFormatTmp("generatedtypes/%.*s.generated.cpp", STRING_VAARGS(parsedHeaderFilenameNoExt));
            sb.AppendFormat("#include \"" STRING_FMT "\"\n", STRING_VAARGS(includeText));
        }
    }

    if (numProcessedFiles > 0)
    {
        OSFileReference sourceFile = {};
        StringView dstFilePath = StringFormatTmp("%s/generatedtypes_unity_sources.generated.cpp", headerOutputFolder);
        if (!meOSOpenFile(sourceFile, dstFilePath, (OSFileFlags_StompExisting | OSFileFlags_ScopedFile)))
        {
            LOG_ERROR("Failed to open file %s while trying to generated reflected headers", dstFilePath);
        }
        else
        {
            meOSWriteFileContent(sourceFile, sb.data, sb.len);
        }
    }

    LOG_INFO("[Mindseye Reflector] wrote %d generated files", numProcessedFiles);
}

bool IsStructural(CXCursorKind kind)
{
    return kind == CXCursor_StructDecl || kind == CXCursor_ClassDecl || kind == CXCursor_ClassTemplate;
}

String SanitizeAndCopyTypeDescriptorName(
    StringView name,
    meAllocator *allocator)
{
    String underlyingTD = String(name, allocator);
    ToUpper(underlyingTD);
    StringReplace(underlyingTD, ' ', '_');
    return meMove(underlyingTD);
}

bool GenerateForwardDecls(
    StringBuilder &builder,
    const meMap<u32, meReflectedType> &reflectedTypes,
    meAllocator *allocator)
{
    bool generatedAny = false;
    for (const auto &[nameID, typeRefl] : reflectedTypes)
    {
        if (typeRefl.IsExcluded())
            continue;
        if (IsStructural(typeRefl.kind))
        {
            String uppercaseName = SanitizeAndCopyTypeDescriptorName(typeRefl.name, allocator);
            u32 numChildren = DynArrayGetSize(typeRefl.children);
            if (numChildren > 0)
            {
                for (u32 i = 0; i < numChildren; i++)
                {
                    meReflectedType &childReflType = *typeRefl.children[i];
                    if (childReflType.innerType && childReflType.innerType->name)
                    {
                        bool isPrimitive = !childReflType.innerType->children || DynArrayGetSize(childReflType.innerType->children) == 0;
                        if (isPrimitive || !TEST_BIT(childReflType.flags, meTypeDescriptorFlag_IncludeInGeneratedHeader))
                        {
                            continue;
                        }
                        String underlyingTD = SanitizeAndCopyTypeDescriptorName(childReflType.innerType->name, allocator);
                        builder.AppendFormat("extern meTypeDescriptor TD_%.*s;\n", STRING_VAARGS(underlyingTD));
                    }
                }
            }
            builder.AppendFormat("extern meTypeDescriptor TD_%.*s;\n", STRING_VAARGS(uppercaseName));
            if (typeRefl.deserializerFnName)
            {
                // matches signature of DeserializerFn
                builder.AppendFormat("bool " STRING_FMT "(const meTypeDescriptor& typeDescriptor, DeserializeContext& ctx);\n", STRING_VAARGS(typeRefl.deserializerFnName));
            }
            if (typeRefl.serializerFnName)
            {
                // matches signature of SerializerFn
                builder.AppendFormat("void " STRING_FMT "(const meTypeDescriptor& typeDescriptor, SerializeContext& ctx);\n", STRING_VAARGS(typeRefl.serializerFnName));
            }
            if (typeRefl.equalsFnName)
            {
                // matches signature of EqualsFn
                builder.AppendFormat("bool " STRING_FMT "(const meTypeDescriptor& typeDescriptor, const void* a, const void* b);\n", STRING_VAARGS(typeRefl.equalsFnName));
            }
            generatedAny = true;
        }
    }
    return generatedAny;
}

#ifdef OS_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
s32 needsRebuild(const char *output_path, const char **input_paths, size_t input_paths_count)
{
    BOOL bSuccess;

    HANDLE output_path_fd = CreateFileA(output_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (output_path_fd == INVALID_HANDLE_VALUE)
    {
        // NOTE: if output does not exist it 100% must be rebuilt
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return 1;
        // nob_log(NOB_ERROR, "Could not open file %s: %s", output_path, nob_win32_error_message(GetLastError()));
        return 1;
    }
    FILETIME output_path_time;
    bSuccess = GetFileTime(output_path_fd, NULL, NULL, &output_path_time);
    CloseHandle(output_path_fd);
    if (!bSuccess)
    {
        // nob_log(NOB_ERROR, "Could not get time of %s: %s", output_path, nob_win32_error_message(GetLastError()));
        return -1;
    }

    for (size_t i = 0; i < input_paths_count; ++i)
    {
        const char *input_path = input_paths[i];
        HANDLE input_path_fd = CreateFileA(input_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
        if (input_path_fd == INVALID_HANDLE_VALUE)
        {
            // NOTE: non-existing input is an error cause it is needed for building in the first place
            // nob_log(NOB_ERROR, "Could not open file %s: %s", input_path, nob_win32_error_message(GetLastError()));
            return -1;
        }
        FILETIME input_path_time;
        bSuccess = GetFileTime(input_path_fd, NULL, NULL, &input_path_time);
        CloseHandle(input_path_fd);
        if (!bSuccess)
        {
            // nob_log(NOB_ERROR, "Could not get time of %s: %s", input_path, nob_win32_error_message(GetLastError()));
            return -1;
        }

        // NOTE: if even a single input_path is fresher than output_path that's 100% rebuild
        if (CompareFileTime(&input_path_time, &output_path_time) == 1)
            return 1;
    }

    return 0;
}
#else
// TODO: platform agnostic timestamp checking
s32 needsRebuild(const char *output_path, const char **input_paths, size_t input_paths_count) { return 1; }
#endif

bool ProcessReflectedFile(
    const meReflectedFile &fileRefl,
    const char *headerOutputFolder,
    meAllocator *allocator)
{
    StringBuilder headerContentBuilder = StringBuilder(allocator, MEGABYTES_BYTES(1));

    StringView parsedHeaderExistingPath = fileRefl.fileName;
    if (!parsedHeaderExistingPath || FindInString(parsedHeaderExistingPath, StringFromCString(headerOutputFolder)) != -1)
    {
        return false;
    }
    StringView parsedHeaderFilename = meFsGetFileFromFullPath(parsedHeaderExistingPath);
    s32 extensionIdx = FindInStringRev(parsedHeaderFilename, STRING_LIT("."));
    StringView parsedHeaderFilenameNoExt = parsedHeaderFilename.OffsetView(0, extensionIdx);
    StringView dstHeaderFilePath = StringFormatTmp("%s/%.*s.generated.h", headerOutputFolder, STRING_VAARGS(parsedHeaderFilenameNoExt));
    const char *inputCheckFile = parsedHeaderExistingPath.cstr();
    if (needsRebuild(dstHeaderFilePath.cstr(), &inputCheckFile, 1) == 0 && !AmIBeingDebugged()) // in a debugger, always rebuild
    {
        // doesn't need to be reprocessed. Already up-to-date
        return false;
    }

    // for simplicity, if anyone wants access to the reflection data for some type, they shouldn't be including
    // the actual header, not the generated one. The generated one should be included by the file it reflects
    headerContentBuilder.Append(STRING_LIT("#pragma once\n"));
    headerContentBuilder.Append(STRING_LIT("// ====== THIS FILE IS AUTOGENERATED =======\n"));
    headerContentBuilder.AppendFormat("// ====== THIS FILE SHOULD ONLY BE INCLUDED BY %.*s =======\n", STRING_VAARGS(parsedHeaderFilename));
    headerContentBuilder.Append(STRING_LIT("#include \"reflector/reflection_types.h\"\n"));
    headerContentBuilder.AppendFormat("STATIC_ASSERT(ConstexprStrstr(STRING_LIT(__FILE__), STRING_LIT(\"%.*s\")) != -1);\n", STRING_VAARGS(parsedHeaderFilenameNoExt));

    bool generatedAny = false;
    bool generatedFwdDecls = GenerateForwardDecls(headerContentBuilder, fileRefl.reflectedTypes, allocator);
    generatedAny |= generatedFwdDecls;

    StringView fileContent = headerContentBuilder;
    if (fileContent)
    {
        OSFileReference headerFile = {};

        if (!meOSOpenFile(headerFile, dstHeaderFilePath, OSFileFlags_StompExisting))
        {
            LOG_ERROR("Failed to open file %s while trying to generated reflected headers", dstHeaderFilePath);
            return false;
        }
        // write to the file here
        meOSWriteFileContent(headerFile, fileContent.data, fileContent.len);
        meOSCloseFile(headerFile);
    }

    StringBuilder sourceContentBuilder = StringBuilder(allocator, MEGABYTES_BYTES(1), StringBuilder::IsScopedAlloc(true));
    sourceContentBuilder.Append(STRING_LIT("// ====== THIS FILE IS AUTOGENERATED =======\n"));
    sourceContentBuilder.AppendFormat("#include \"%.*s.generated.h\"\n", STRING_VAARGS(parsedHeaderFilenameNoExt));

    for (const auto &[nameID, typeRefl] : fileRefl.reflectedTypes)
    {
        if (typeRefl.IsExcluded())
            continue;
        if (IsStructural(typeRefl.kind))
        {
            String uppercaseName = SanitizeAndCopyTypeDescriptorName(typeRefl.name, allocator);
            u32 numChildren = DynArrayGetSize(typeRefl.children);
            if (numChildren > 0)
            {
                StringBuilder fieldsArrayContent = StringBuilder(allocator);
                StringBuilder templateTypesContent = StringBuilder(allocator);
                u32 numPaddingMembers = 0;
                u32 currentOffsetBytes = 0;
                for (u32 i = 0; i < numChildren; i++)
                {
                    meReflectedType &childReflType = *typeRefl.children[i];

                    // NOTE: this whole padding thing might be made a lot better if I just use clang's
                    // info like the offsetBits with the sizeBits and extract padding info from there instead of calculating it myself.

                    // this is an assumption to make implementation simpler. If this assert hits, there's likely a bitfield with a non-multiple-of-eight size. It could be supported, but requires more thought into the math here.
                    u32 childAlign = childReflType.align;
                    u32 childPadding = (childAlign - (currentOffsetBytes % childAlign)) % childAlign;
                    currentOffsetBytes += childReflType.size;
                    currentOffsetBytes += childPadding;

                    if (childPadding)
                    {
                        ME_ASSERT(i > 0); // first member should never have padding
                        StringBuilder paddingVarName = StringBuilder(GetTLScratch());
                        const meReflectedType &paddedField = *typeRefl.children[i - 1];
                        u32 paddedFieldSizeBits = paddedField.size * 8;
                        paddingVarName.AppendFormat(STRING_FMT "_padding", STRING_VAARGS(paddedField.name));
                        u32 paddingMemberOffsetBits = paddedField.offsetBits + paddedFieldSizeBits;
                        fieldsArrayContent.AppendFormat(
                            "\t{ .name = STRING_LIT(\"%.*s\"), .flags = NTH_BIT(meTypeDescriptorFlag_PaddingMember), .size = %i, .align = %i, .offsetBits = %i },\n",
                            STRING_VAARGS(paddingVarName), childPadding, 1, paddingMemberOffsetBits);
                        numPaddingMembers++;
                    }

                    if (childReflType.IsExcluded())
                    {
                        // excluded fields are still "there", but they have no underlying type
                        // think of it like "padding" bytes so the other field offsets make sense
                        fieldsArrayContent.AppendFormat(
                            "\t{ .name = STRING_LIT(\"%.*s\"), .flags = NTH_BIT(meTypeDescriptorFlag_Excluded), .size = %i, .align = %i, .offsetBits = %i },\n",
                            STRING_VAARGS(childReflType.name), childReflType.size, childReflType.align, childReflType.offsetBits);
                        continue;
                    }

                    fieldsArrayContent.Append(STRING_LIT("\t{ "));
                    fieldsArrayContent.AppendFormat(".name = STRING_LIT(\"%.*s\"), ", STRING_VAARGS(childReflType.name));
                    if (childReflType.editorName)
                        fieldsArrayContent.AppendFormat(".editorName = STRING_LIT(\"%.*s\"), ", STRING_VAARGS(childReflType.editorName));
                    if (childReflType.tooltip)
                        fieldsArrayContent.AppendFormat(".tooltip = STRING_LIT(\"%.*s\"), ", STRING_VAARGS(childReflType.tooltip));
                    if (childReflType.flags & meTypeDescriptorFlagsSerializedBitmask)
                    {
                        fieldsArrayContent.Append(STRING_LIT(".flags = ("));
                        u32 num = 0;
                        for (u32 i = 0; i < meTypeDescriptorFlag_NonSerializedFlagsMarker; i++)
                        {
                            if (TEST_BIT(childReflType.flags, i))
                            {
                                if (num++ != 0)
                                {
                                    fieldsArrayContent.Append(STRING_LIT(" | "));
                                }
                                StringView flagStr = meTypeDescriptorFlagToString(i);
                                fieldsArrayContent.AppendFormat("NTH_BIT(" STRING_FMT ")", STRING_VAARGS(flagStr));
                            }
                        }
                        fieldsArrayContent.Append(STRING_LIT("), "));
                    }
                    fieldsArrayContent.AppendFormat(".version = %i, ", childReflType.version);
                    fieldsArrayContent.AppendFormat(".size = %i, ", childReflType.size != 0 ? childReflType.size : (childReflType.innerType ? childReflType.innerType->size : 0));
                    fieldsArrayContent.AppendFormat(".align = %i, ", childReflType.align != 0 ? childReflType.align : (childReflType.innerType ? childReflType.innerType->align : 0));
                    fieldsArrayContent.AppendFormat(".offsetBits = %i, ", childReflType.offsetBits);
                    if (childReflType.innerType && childReflType.innerType->name && TEST_BIT(childReflType.flags, meTypeDescriptorFlag_IncludeInGeneratedHeader))
                    {
                        String underlyingTD = SanitizeAndCopyTypeDescriptorName(childReflType.innerType->name, allocator);
                        fieldsArrayContent.AppendFormat(".thisType = &TD_%.*s, ", STRING_VAARGS(underlyingTD));
                        if (DynArrayGetSize(childReflType.templateTypes))
                        {
                            StringView templateArgsListVarName = StringFormatNew(allocator, "g_templateArgs_" STRING_FMT, STRING_VAARGS(childReflType.name));
                            for (DynArray_Foreach(childReflType.templateTypes, templateArgIdx))
                            {
                                meReflectedType *templateArgType = childReflType.templateTypes[templateArgIdx];
                                if (TEST_BIT(templateArgType->flags, meTypeDescriptorFlag_IntegralStub))
                                {
                                    templateTypesContent.AppendFormat(
                                        "static meTypeDescriptor g_intarg_%.*s_%u = { .value = %i };\n",
                                        STRING_VAARGS(childReflType.name), (u32)templateArgIdx, templateArgType->value);
                                }
                                else if (DynArrayGetSize(templateArgType->templateTypes))
                                {
                                    // This named type arg is itself a template specialization.
                                    // Emit its nested integral stubs, then its templateArgs array,
                                    // then the intermediate descriptor.
                                    u32 numNestedArgs = DynArrayGetSize(templateArgType->templateTypes);
                                    for (u32 nestedIdx = 0; nestedIdx < numNestedArgs; nestedIdx++)
                                    {
                                        meReflectedType *nestedArg = templateArgType->templateTypes[nestedIdx];
                                        if (TEST_BIT(nestedArg->flags, meTypeDescriptorFlag_IntegralStub))
                                        {
                                            templateTypesContent.AppendFormat(
                                                "static meTypeDescriptor g_intarg_%.*s_%u_%u = { .value = %i };\n",
                                                STRING_VAARGS(childReflType.name), (u32)templateArgIdx, nestedIdx, nestedArg->value);
                                        }
                                    }
                                    templateTypesContent.AppendFormat(
                                        "static meTypeDescriptor* g_nestedTemplateArgs_%.*s_%u[] = {\n",
                                        STRING_VAARGS(childReflType.name), (u32)templateArgIdx);
                                    for (u32 nestedIdx = 0; nestedIdx < numNestedArgs; nestedIdx++)
                                    {
                                        meReflectedType *nestedArg = templateArgType->templateTypes[nestedIdx];
                                        if (TEST_BIT(nestedArg->flags, meTypeDescriptorFlag_IntegralStub))
                                        {
                                            templateTypesContent.AppendFormat(
                                                "\t&g_intarg_%.*s_%u_%u,\n",
                                                STRING_VAARGS(childReflType.name), (u32)templateArgIdx, nestedIdx);
                                        }
                                        else
                                        {
                                            String nestedTDName = SanitizeAndCopyTypeDescriptorName(nestedArg->name, allocator);
                                            templateTypesContent.AppendFormat("\t&TD_" STRING_FMT ",\n", STRING_VAARGS(nestedTDName));
                                        }
                                    }
                                    templateTypesContent.Append(STRING_LIT("};\n"));
                                    String argTDName = SanitizeAndCopyTypeDescriptorName(templateArgType->name, allocator);
                                    // Use the full template spelling so (for instance) we get meTypedAsset<MAEntity>'s ctor, not just meAsset's.
                                    StringView ctorTypeName = templateArgType->fullTemplateName
                                        ? templateArgType->fullTemplateName
                                        : templateArgType->name;
                                    templateTypesContent.AppendFormat(
"static meTypeDescriptor g_typearg_%.*s_%u =\n"
"{\n"
"    .name = STRING_LIT(\"" STRING_FMT "\"),\n"
"    .size = %u, .align = %u,\n"
"    .thisType = &TD_" STRING_FMT ",\n"
"    .templatedTypes = meSpanTyped<meTypeDescriptor*>(g_nestedTemplateArgs_%.*s_%u),\n"
"    .equalsFn = &meTypeDescriptorEquals<" STRING_FMT ">,\n"
"    .setToDefaultsFn = &meTypeDescriptorSetToDefaults<" STRING_FMT ">,\n"
"};\n",
                                        STRING_VAARGS(childReflType.name), (u32)templateArgIdx,
                                        STRING_VAARGS(templateArgType->name),
                                        templateArgType->size, templateArgType->align,
                                        STRING_VAARGS(argTDName),
                                        STRING_VAARGS(childReflType.name), (u32)templateArgIdx,
                                        STRING_VAARGS(ctorTypeName),
                                        STRING_VAARGS(ctorTypeName)
                                    );
                                }
                            }
                            templateTypesContent.AppendFormat("meTypeDescriptor* " STRING_FMT "[] = {\n", STRING_VAARGS(templateArgsListVarName));
                            for (DynArray_Foreach(childReflType.templateTypes, templateArgIdx))
                            {
                                meReflectedType *templateArgType = childReflType.templateTypes[templateArgIdx];
                                if (TEST_BIT(templateArgType->flags, meTypeDescriptorFlag_IntegralStub))
                                {
                                    templateTypesContent.AppendFormat(
                                        "\t&g_intarg_%.*s_%u,\n",
                                        STRING_VAARGS(childReflType.name), (u32)templateArgIdx);
                                }
                                else if (DynArrayGetSize(templateArgType->templateTypes))
                                {
                                    // nested template descriptor (e.g. meTypedAsset<MAEntity>)
                                    templateTypesContent.AppendFormat(
                                        "\t&g_typearg_%.*s_%u,\n",
                                        STRING_VAARGS(childReflType.name), (u32)templateArgIdx);
                                }
                                else
                                {
                                    String templateTDName = SanitizeAndCopyTypeDescriptorName(templateArgType->name, allocator);
                                    templateTypesContent.AppendFormat("\t&TD_" STRING_FMT ",\n", STRING_VAARGS(templateTDName));
                                }
                            }
                            templateTypesContent.Append(STRING_LIT("};\n"));
                            fieldsArrayContent.AppendFormat(
                                ".templatedTypes = meSpanTyped<meTypeDescriptor*>(" STRING_FMT "), ",
                                STRING_VAARGS(templateArgsListVarName));
                        }
                    }
                    fieldsArrayContent.Append(STRING_LIT("},"));
                    if (i != numChildren - 1)
                    {
                        fieldsArrayContent.Append(STRING_LIT("\n"));
                    }
                }
                u64 structAlign = typeRefl.align;
                u64 finalPadding = (structAlign - (currentOffsetBytes % structAlign)) % structAlign;
                currentOffsetBytes += finalPadding;
                ME_ASSERT(currentOffsetBytes == typeRefl.size);
                if (finalPadding)
                {
                    StringBuilder paddingVarName = StringBuilder(GetTLScratch());
                    const meReflectedType &paddedField = *typeRefl.children[numChildren - 1];
                    u32 paddedFieldSizeBits = paddedField.size * 8;
                    paddingVarName.AppendFormat(STRING_FMT "_padding", STRING_VAARGS(paddedField.name));
                    u32 paddingMemberOffsetBits = paddedField.offsetBits + paddedFieldSizeBits;
                    fieldsArrayContent.AppendFormat(
                        "\n\t{ .name = STRING_LIT(\"%.*s\"), .flags = NTH_BIT(meTypeDescriptorFlag_PaddingMember), .size = %i, .align = %i, .offsetBits = %i }",
                        STRING_VAARGS(paddingVarName), finalPadding, 1, paddingMemberOffsetBits);
                    numPaddingMembers++;
                }

                sourceContentBuilder.Append(templateTypesContent);
                sourceContentBuilder.AppendFormat(
                    "meTypeDescriptor g_%.*s_fields[%i] = {\n%.*s\n};\n",
                    STRING_VAARGS(typeRefl.name), numChildren + numPaddingMembers, STRING_VAARGS(fieldsArrayContent));
            }
            StringBuilder mainTypeDescriptorContent = StringBuilder(allocator);
            mainTypeDescriptorContent.AppendFormat("\t.name = STRING_LIT(\"%.*s\"),\n", STRING_VAARGS(typeRefl.name));
            if (typeRefl.editorName)
                mainTypeDescriptorContent.AppendFormat("\t.editorName = STRING_LIT(\"%.*s\"),\n", STRING_VAARGS(typeRefl.editorName));
            if (typeRefl.tooltip)
                mainTypeDescriptorContent.AppendFormat("\t.tooltip = STRING_LIT(\"%.*s\"),\n", STRING_VAARGS(typeRefl.tooltip));

            if (DynArrayGetSize(typeRefl.children))
            {
                mainTypeDescriptorContent.AppendFormat("\t.fields = {g_%.*s_fields},\n", STRING_VAARGS(typeRefl.name));
            }
            mainTypeDescriptorContent.AppendFormat("\t.version = %i,\n", typeRefl.version);
            mainTypeDescriptorContent.AppendFormat("\t.size = %i,\n", typeRefl.size);
            mainTypeDescriptorContent.AppendFormat("\t.align = %i,\n", typeRefl.align);
            if (typeRefl.serializerFnName)
            {
                mainTypeDescriptorContent.AppendFormat("\t.serializerFn = " STRING_FMT ",\n", STRING_VAARGS(typeRefl.serializerFnName));
            }
            if (typeRefl.deserializerFnName)
            {
                mainTypeDescriptorContent.AppendFormat("\t.deserializerFn = " STRING_FMT ",\n", STRING_VAARGS(typeRefl.deserializerFnName));
            }
            if (typeRefl.equalsFnName)
            {
                mainTypeDescriptorContent.AppendFormat("\t.equalsFn = " STRING_FMT ",\n", STRING_VAARGS(typeRefl.equalsFnName));
            }
            else
            {
                mainTypeDescriptorContent.AppendFormat("\t.equalsFn = &meTypeDescriptorEquals<" STRING_FMT ">,\n", STRING_VAARGS(typeRefl.name));
            }
            if (typeRefl.editorRenderFnName)
            {
                mainTypeDescriptorContent.AppendFormat("\t.editorRenderFn = " STRING_FMT ",\n", STRING_VAARGS(typeRefl.editorRenderFnName));
            }

            mainTypeDescriptorContent.AppendFormat("\t.setToDefaultsFn = &meTypeDescriptorSetToDefaults<" STRING_FMT ">,\n", STRING_VAARGS(typeRefl.name));

            sourceContentBuilder.AppendFormat("meTypeDescriptor TD_%.*s = {\n%.*s};\n", STRING_VAARGS(uppercaseName), STRING_VAARGS(mainTypeDescriptorContent));
            generatedAny = true;
        }
    }

    // if (generatedAny)
    {
        OSFileReference sourceFile = {};
        s32 extensionIdx = FindInStringRev(parsedHeaderFilename, STRING_LIT("."));
        StringView parsedSourceFilenameNoExt = parsedHeaderFilename.OffsetView(0, extensionIdx);
        StringView dstFilePath = StringFormatTmp("%s/%.*s.generated.cpp", headerOutputFolder, STRING_VAARGS(parsedSourceFilenameNoExt));
        if (!meOSOpenFile(sourceFile, dstFilePath, OSFileFlags_StompExisting))
        {
            LOG_ERROR("Failed to open file %s while trying to generated reflected headers", dstFilePath);
            return false;
        }
        meOSWriteFileContent(sourceFile, sourceContentBuilder.data, sourceContentBuilder.len);
        meOSCloseFile(sourceFile);
    }
    return generatedAny;
}

// ========= CompileRun system ======================

using CompileRunEntryFn = void (*)(meCompileRunContext *);

bool CreateCompileRunInterpreter(
    ClangParsingContext& ctx,
    std::unique_ptr<clang::Interpreter>& outInterpreter,
    std::vector<CompileRunEntryFn>& outEntryFns)
{
    std::vector<const char *> interpreterArgs;
    for (u32 i = 0; i < DynArrayGetSize(ctx.clangArgs); i++)
    {
        interpreterArgs.push_back(ctx.clangArgs[i]);
    }
    //interpreterArgs.push_back("-DME_REFLECTING");
    interpreterArgs.push_back("-DME_COMPILE_RUN");

    clang::IncrementalCompilerBuilder builder;
    builder.SetCompilerArgs(interpreterArgs);

    auto ciExpected = builder.CreateCpp();
    if (!ciExpected)
    {
        LogLLVMError(ciExpected.takeError(), "Failed to create compile-run compiler instance");
        return false;
    }

    auto interpExpected = clang::Interpreter::create(std::move(*ciExpected));
    if (!interpExpected)
    {
        LogLLVMError(interpExpected.takeError(), "Failed to create compile-run interpreter");
        return false;
    }
    std::unique_ptr<clang::Interpreter> interpreter = std::move(*interpExpected);

    StringBuilder code(ctx.allocator, MEGABYTES_BYTES(1));
    code.Append(STRING_LIT(
        "#ifndef ME_REFLECTING\n"
        "#define ME_REFLECTING\n"
        "#endif\n"
        "#ifndef ME_COMPILE_RUN\n"
        "#define ME_COMPILE_RUN\n"
        "#endif\n"
        "#ifndef ME_CORE_ONLY\n"
        "#define ME_CORE_ONLY\n"
        "#endif\n"
        "#include \"mindseye/me_unity.cpp\"\n"
        "#include \"mindseye/reflector/me_compile_run_api.h\"\n\n"));
    u32 numCompileRunDecls = DynArrayGetSize(ctx.compileRunDecls);
    for (u32 i = 0; i < numCompileRunDecls; i++)
    {
        code.Append(ctx.compileRunDecls[i].functionSource);
        code.Append(STRING_LIT("\n\n"));
    }
    for (u32 i = 0; i < numCompileRunDecls; i++)
    {
        const meCompileRunDecl &decl = ctx.compileRunDecls[i];
        code.AppendFormat(
            "extern \"C\" void me_compile_run_entry_%u(meCompileRunContext* ctx)\n"
            "{\n"
            "    %.*s(ctx);\n"
            "}\n\n",
            i,
            STRING_VAARGS(decl.functionName));
    }

    //LOG_INFO(STRING_FMT, STRING_VAARGS(code)); // for debugging - prints the string passed to the interpreter
    llvm::StringRef codeRef(code.data, code.len);
    if (LogLLVMError(interpreter->ParseAndExecute(codeRef), "Compile-run ParseAndExecute failed"))
    {
        return false;
    }

    for (u32 i = 0; i < numCompileRunDecls; i++)
    {
        StringView entryName = StringFormatTmp("me_compile_run_entry_%u", i);
        auto entryAddr = interpreter->getSymbolAddressFromLinkerName(entryName.cstr());
        if (!entryAddr)
        {
            LogLLVMError(entryAddr.takeError(), "Failed to find compile-run entry point");
            return false;
        }
        outEntryFns.push_back(entryAddr->toPtr<CompileRunEntryFn>());
    }

    outInterpreter = std::move(interpreter);
    return true;
}

void FillCompileRunContextCommon(meCompileRunContext& runCtx, ClangParsingContext& ctx)
{
    runCtx.translationUnit = *ctx.tu;
    runCtx.projectRoot = ctx.projectRootDir.cstr();
    runCtx.outputDir = ctx.headerOutputFolder.cstr();
}

void RunCompileRunStageForDecl(
    ClangParsingContext& ctx,
    const meAnnotatedDecl& decl,
    meCompileRunStage stage,
    const std::vector<CompileRunEntryFn>& entryFns)
{
    meCompileRunContext runCtx = {};
    FillCompileRunContextCommon(runCtx, ctx);
    runCtx.stage = stage;
    runCtx.cursor = decl.cursor;
    runCtx.reflectOp = decl.op;
    runCtx.macroContent = decl.macroContent;
    runCtx.sourceFile = decl.sourceFile ? decl.sourceFile.cstr() : nullptr;
    for (CompileRunEntryFn entry : entryFns)
    {
        entry(&runCtx);
    }
}

bool RunCompileRunDecls(ClangParsingContext& ctx)
{
    u32 numCompileRunDecls = DynArrayGetSize(ctx.compileRunDecls);
    if (numCompileRunDecls == 0)
    {
        LOG_INFO("No compile-run work to do");
        return true;
    }

    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeX86AsmParser();

    //LOG_INFO("[Mindseye Reflector] running %u compile-run function(s)", numCompileRunDecls);
    std::unique_ptr<clang::Interpreter> interpreter;
    std::vector<CompileRunEntryFn> entryFns;
    if (!CreateCompileRunInterpreter(ctx, interpreter, entryFns))
    {
        return false;
    }

    u32 numAnnotatedDecls = DynArrayGetSize(ctx.annotatedDecls);
    for (u32 declIdx = 0; declIdx < numAnnotatedDecls; declIdx++)
    {
        RunCompileRunStageForDecl(ctx, ctx.annotatedDecls[declIdx], meCompileRunStage_VisitDecl, entryFns);
    }
    meAnnotatedDecl decl = {};
    decl.cursor = clang_getNullCursor();
    RunCompileRunStageForDecl(ctx, decl, meCompileRunStage_Finalize, entryFns);
    return true;
}

// =======================================================


