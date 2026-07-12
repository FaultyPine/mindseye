#pragma once

// binary serialization. The same DoState(wrap) code path handles
// save, load, measure, and verify by switching the mode on the wrap object.
// inspired by Dolphin Emulator's PointerWrap

// Measure: walk the data and count how many bytes are needed
// Write: copy data to output buffer
// Read: copy data from input buffer back into the struct
// Verify: compare expected data against bytes in a buffer
// 'meChunkerSave' first does a 'Measure' pass to allocate that much, then 'Write'
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

#include <type_traits>
#include <functional>

bool meChunkerTests();

struct meChunker
{
    enum class Mode : u8 { Write, Read, Measure, Verify };

    meChunker(meSpan buf, Mode mode)
        : m_start((u8*)buf.data)
        , m_cursor((u8*)buf.data)
        , m_end((u8*)buf.data + buf.size)
        , m_mode(mode)
    {}

    bool IsReadMode()    const { return m_mode == Mode::Read;    }
    bool IsWriteMode()   const { return m_mode == Mode::Write;   }
    bool IsMeasureMode() const { return m_mode == Mode::Measure; }
    bool IsVerifyMode()  const { return m_mode == Mode::Verify;  }
    void SetMeasureMode()      { m_mode = Mode::Measure; }

    u64 BytesProcessed() const { return (u64)(m_cursor - m_start); }

    template<typename T>
    requires (std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
    void Do(T& x) { DoVoid(&x, (u32)sizeof(x)); }

    void Do(bool& x)
    {
        u8 as_byte = x ? 1 : 0;
        DoVoid(&as_byte, 1);
        if (IsReadMode()) x = (as_byte != 0);
    }

    template<typename T>
    requires std::is_trivially_copyable_v<T>
    void DoArray(T* arr, u32 count) { DoVoid(arr, count * (u32)sizeof(T)); }

    template<typename T, u32 N>
    void DoArray(T (&arr)[N]) { DoArray(arr, N); }

    // Size stored as u32 prefix. read_allocator required in Read mode.
    void DoBytes(meSpan& span, meAllocator* read_allocator = nullptr)
    {
        u32 size = (u32)span.size;
        Do(size);
        if (IsReadMode())
        {
            ME_ASSERT(read_allocator);
            span = MEALLOC(read_allocator, size);
        }
        DoVoid(span.data, size);
    }

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

private:
    void DoVoid(void* data, u32 size)
    {
        if (!IsMeasureMode() && (m_cursor + size) > m_end)
            SetMeasureMode();

        switch (m_mode)
        {
			case Mode::Write:   ME_MEMCPY(m_cursor, data, size); break;
			case Mode::Read:    ME_MEMCPY(data, m_cursor, size); break;
			case Mode::Measure: break;
			case Mode::Verify:  ME_ASSERT(ME_MEMCMP(data, m_cursor, size) == 0); break;
        }

        m_cursor += size;
    }

    u8*  m_start  = nullptr;
    u8*  m_cursor = nullptr;
    u8*  m_end    = nullptr;
    Mode m_mode   = Mode::Measure;
};


template<typename Fn> u64 meChunkerMeasure(Fn&& fn)
{
    meChunker w(meSpan((char*)nullptr, 0), meChunker::Mode::Measure);
    fn(w);
    return w.BytesProcessed();
}

template<typename Fn> u64 meChunkerWrite(meSpan buf, Fn&& fn)
{
    meChunker w(buf, meChunker::Mode::Write);
    fn(w);
    return w.IsWriteMode() ? w.BytesProcessed() : 0;
}

template<typename Fn> bool meChunkerRead(meSpan buf, Fn&& fn)
{
    meChunker w(buf, meChunker::Mode::Read);
    fn(w);
    return w.IsReadMode();
}

template<typename Fn> bool meChunkerVerify(meSpan buf, Fn&& fn)
{
    meChunker w(buf, meChunker::Mode::Verify);
    fn(w);
    return w.IsVerifyMode();
}

// Allocate, write, return owned buffer
template<typename Fn> meOwningSpan meChunkerSave(meAllocator* allocator, Fn&& fn)
{
    u64 needed = meChunkerMeasure(fn);
    if (needed == 0) return {};
    meOwningSpan buf = MEALLOC(allocator, needed);
    u64 written = meChunkerWrite(buf, fn);
    ME_ASSERT(written == needed);
    return buf;
}
