#pragma once

// binary serialization. The same DoState(wrap) code path handles
// save, load, measure, and verify by switching the mode on the wrap object.
// inspired by Dolphin Emulator's PointerWrap

// Measure: walk the data and count how many bytes are needed
// Write: copy data to output buffer
// Read: copy data from input buffer back into the struct
// Verify: compare expected data against bytes in a buffer
// For writing, we first does a 'Measure' pass to figure out how much to allocate, then 'Write' into that allocated buffer
//
// If someone happens to write asymmetric serialization code using this, it is detected
// and the meChunker is switched to (harmless) 'Measure' mode. If we're in measure mode at the end
// of read/write call, we know we've had an error and can return an error to the caller.
//
// A 'marker" is a hash cookie - if it doesn't match on read, same thing as above happens to indicate failure

#include "core/me_defines.h"
#include "core/me_memory.h"
#include "core/me_core.h"
#include "core/containers/me_span.h"
#include "core/me_string.h"
#include "reflector/reflection_types.h"

#include <type_traits>
#include <functional>

bool meChunkerTests();

struct meChunker
{
    enum class Pass : u8 { Write, Read, Measure, Verify };
    enum class PrimitiveKind : u8
    {
        S8,
        U8,
        S16,
        U16,
        S32,
        U32,
        S64,
        U64,
        F32,
        F64,
        Bool,
        Long,
        ULong,
        WChar,
        Raw,
    };

    meChunker(meSerializationMode serializationMode, Pass pass, meAllocator* allocator, meSpan data);

    ~meChunker();
    meChunker(const meChunker&) = delete;
    meChunker& operator=(const meChunker&) = delete;

    bool IsReadMode() const;
    bool IsWriteMode() const;
    bool IsMeasureMode() const;
    bool IsVerifyMode() const;
    bool IsValid() const;
    void SetMeasureMode();
    meSerializationMode GetSerializationMode() const;
    u64 BytesProcessed() const;

    template<typename T>
    requires (std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
    void Do(T& x) { DoPrimitive(&x, (u32)sizeof(x), PrimitiveKindFor<T>()); }

    void Do(bool& x);

    template<typename T>
    requires std::is_trivially_copyable_v<T>
    void DoArray(T* arr, u32 count)
    {
        ME_ASSERT(m_serializationMode == meSerializationMode_Binary);
        DoBinaryVoid(arr, count * (u32)sizeof(T));
    }

    template<typename T, u32 N>
    void DoArray(T (&arr)[N]) { DoArray(arr, N); }

    // Size stored as u32 prefix. read_allocator required in Read mode.
    void DoBytes(meSpan& span, meAllocator* read_allocator = nullptr);

    // Stores ptr as a byte offset from base. Reconstructs on load.
    template<typename T>
    void DoPointerOffset(T*& ptr, const void* base)
    {
        s64 offset = ptr ? (s64)((const u8*)ptr - (const u8*)base) : -1;
        Do(offset);
        if (IsReadMode())
            ptr = (offset >= 0) ? (T*)((const u8*)base + offset) : nullptr;
    }

    // Writes a u32 cookie derived from the tag; on load asserts it matches.
    // Accepts only string literals - w.DoMarker("Tag")
    template<u32 N>
    void DoMarker(const char (&tag)[N])
    {
        u32 cookie = HashBytes((u8*)tag, N);
        Do(cookie);
        if (IsReadMode() && cookie != HashBytes((u8*)tag, N))
        {
            ME_ASSERT(!"meChunker: marker mismatch");
            SetMeasureMode();
        }
    }

    bool BeginObject();
    void EndObject();
    bool Field(StringView name);
    void EndField();
    bool BeginArray(u32& count);
    bool BeginFixedArray(u32& count);
    void EndArray();
    bool Element(u32 index);
    void EndElement();

    void DoString(String& str, meAllocator* readAllocator = nullptr);
    meOwningSpan Finalize(meAllocator* allocator);

private:
    void InitBinary(meSpan buf);
    void InitText(meAllocator* allocator, meSpan data);

    template<typename T>
    static constexpr PrimitiveKind PrimitiveKindFor()
    {
        if constexpr (std::is_same_v<T, s8>) return PrimitiveKind::S8;
        else if constexpr (std::is_same_v<T, u8>) return PrimitiveKind::U8;
        else if constexpr (std::is_same_v<T, s16>) return PrimitiveKind::S16;
        else if constexpr (std::is_same_v<T, u16>) return PrimitiveKind::U16;
        else if constexpr (std::is_same_v<T, s32>) return PrimitiveKind::S32;
        else if constexpr (std::is_same_v<T, u32>) return PrimitiveKind::U32;
        else if constexpr (std::is_same_v<T, s64>) return PrimitiveKind::S64;
        else if constexpr (std::is_same_v<T, u64>) return PrimitiveKind::U64;
        else if constexpr (std::is_same_v<T, f32>) return PrimitiveKind::F32;
        else if constexpr (std::is_same_v<T, f64>) return PrimitiveKind::F64;
        else if constexpr (std::is_same_v<T, bool>) return PrimitiveKind::Bool;
        else if constexpr (std::is_same_v<T, long>) return PrimitiveKind::Long;
        else if constexpr (std::is_same_v<T, unsigned long>) return PrimitiveKind::ULong;
        else if constexpr (std::is_same_v<T, wchar_t>) return PrimitiveKind::WChar;
        else return PrimitiveKind::Raw;
    }

    void DoPrimitive(void* data, u32 size, PrimitiveKind kind);

    void DoBinaryVoid(void* data, u32 size);

    void DoTextPrimitive(void* data, u32 size, PrimitiveKind kind);
    void DoTextBytes(meSpan& span, meAllocator* readAllocator);
    bool DoTextBeginObject();
    void DoTextEndObject();
    bool DoTextField(StringView name);
    void DoTextEndField();
    bool DoTextBeginArray(u32& count);
    void DoTextEndArray();
    bool DoTextElement(u32 index);
    void DoTextEndElement();
    meOwningSpan DoTextFinalize(meAllocator* allocator);
    void PushTextCurrent();
    void PopTextCurrent();

    struct BinaryState
    {
        u8* start  = nullptr;
        u8* cursor = nullptr;
        u8* end    = nullptr;
    };

    struct TextState
    {
        void* root = nullptr;
        void* current = nullptr;
        void* currentStack[128] = {};
        u32 currentStackSize = 0;
        meSpan outputBuffer = {};
        meAllocator* allocator = nullptr;
    };

    static void TextStateInitForWrite(TextState& state, meAllocator* allocator, meSpan outputBuffer);
    static bool TextStateInitForRead(TextState& state, meAllocator* allocator, meSpan input);
    static void TextStateDestroy(TextState& state);

    meSerializationMode m_serializationMode = meSerializationMode_Binary;
    Pass m_pass = Pass::Measure;
    bool m_isValid = true;
    BinaryState m_binary = {};
    TextState m_text = {};
};
