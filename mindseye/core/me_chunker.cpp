#include "core/me_chunker.h"

// TODO: Get rid of json. It's not a great format for engine data assets.
#include <external/json.hpp>
using json = nlohmann::json;

STATIC_ASSERT(sizeof(bool) == 1, "meChunker: unexpected bool size");

namespace
{
    static json& TextJson(void* ptr)
    {
        ME_ASSERT(ptr);
        return *(json*)ptr;
    }

    static std::string StringViewToStdString(StringView str)
    {
        return std::string(str.data, str.len);
    }

    static bool JsonValuesEqual(const json& j, void* data, meChunker::PrimitiveKind kind)
    {
        switch (kind)
        {
            case meChunker::PrimitiveKind::S8:   return j.get<s8>() == *(s8*)data;
            case meChunker::PrimitiveKind::U8:   return j.get<u8>() == *(u8*)data;
            case meChunker::PrimitiveKind::S16:  return j.get<s16>() == *(s16*)data;
            case meChunker::PrimitiveKind::U16:  return j.get<u16>() == *(u16*)data;
            case meChunker::PrimitiveKind::S32:  return j.get<s32>() == *(s32*)data;
            case meChunker::PrimitiveKind::U32:  return j.get<u32>() == *(u32*)data;
            case meChunker::PrimitiveKind::S64:  return j.get<s64>() == *(s64*)data;
            case meChunker::PrimitiveKind::U64:  return j.get<u64>() == *(u64*)data;
            case meChunker::PrimitiveKind::F32:  return j.get<f32>() == *(f32*)data;
            case meChunker::PrimitiveKind::F64:  return j.get<f64>() == *(f64*)data;
            case meChunker::PrimitiveKind::Bool: return j.get<bool>() == *(bool*)data;
            case meChunker::PrimitiveKind::Long: return j.get<long>() == *(long*)data;
            case meChunker::PrimitiveKind::ULong: return j.get<unsigned long>() == *(unsigned long*)data;
            case meChunker::PrimitiveKind::WChar: return j.get<u32>() == (u32)*(wchar_t*)data;
            default: break;
        }
        return false;
    }
}

meChunker::meChunker(meSerializationMode serializationMode, Pass pass, meAllocator* allocator, meSpan data)
    : m_serializationMode(serializationMode)
    , m_pass(pass)
{
    ME_ASSERT(m_serializationMode == meSerializationMode_Binary || m_serializationMode == meSerializationMode_Text);
    if (m_serializationMode == meSerializationMode_Binary)
    {
        InitBinary(data);
        return;
    }

    InitText(allocator, data);
}

bool meChunker::IsReadMode() const
{
    return m_pass == Pass::Read;
}

bool meChunker::IsWriteMode() const
{
    return m_pass == Pass::Write;
}

bool meChunker::IsMeasureMode() const
{
    return m_pass == Pass::Measure;
}

bool meChunker::IsVerifyMode() const
{
    return m_pass == Pass::Verify;
}

bool meChunker::IsValid() const
{
    return m_isValid;
}

void meChunker::SetMeasureMode()
{
    m_pass = Pass::Measure;
}

meSerializationMode meChunker::GetSerializationMode() const
{
    return m_serializationMode;
}

u64 meChunker::BytesProcessed() const
{
    return m_serializationMode == meSerializationMode_Binary ? (u64)(m_binary.cursor - m_binary.start) : 0;
}

void meChunker::TextStateInitForWrite(TextState& state, meAllocator* allocator, meSpan outputBuffer)
{
    ME_ASSERT(allocator);
    state.allocator = allocator;
    state.root = MENEW(allocator, json, json::object());
    state.current = state.root;
    state.currentStackSize = 0;
    state.outputBuffer = outputBuffer;
}

bool meChunker::TextStateInitForRead(TextState& state, meAllocator* allocator, meSpan input)
{
    ME_ASSERT(allocator);
    state.allocator = allocator;
    state.root = MENEW(allocator, json);
    state.current = state.root;
    state.currentStackSize = 0;
    state.outputBuffer = {};
    try
    {
        TextJson(state.root) = json::parse(input.data, input.data + input.size);
    }
    catch (const json::parse_error& e)
    {
        LOG_ERROR("JSON parse error: %s", e.what());
        TextStateDestroy(state);
        return false;
    }
    return true;
}

void meChunker::TextStateDestroy(TextState& state)
{
    if (state.root)
    {
        ME_ASSERT(state.allocator);
        json* root = (json*)state.root;
        MEDELETE(state.allocator, json, root);
    }
    state.root = nullptr;
    state.current = nullptr;
    state.currentStackSize = 0;
    state.outputBuffer = {};
    state.allocator = nullptr;
}

meChunker::~meChunker()
{
    if (m_serializationMode == meSerializationMode_Text)
    {
        TextStateDestroy(m_text);
    }
}

void meChunker::InitBinary(meSpan buf)
{
    m_binary.start = (u8*)buf.data;
    m_binary.cursor = (u8*)buf.data;
    m_binary.end = (u8*)buf.data + buf.size;
    m_isValid = true;
}

void meChunker::InitText(meAllocator* allocator, meSpan data)
{
    if (IsReadMode())
    {
        m_isValid = TextStateInitForRead(m_text, allocator, data);
    }
    else
    {
        TextStateInitForWrite(m_text, allocator, data);
        m_isValid = true;
    }
}

void meChunker::DoBinaryVoid(void* data, u32 size)
{
    if (!IsMeasureMode() && (m_binary.cursor + size) > m_binary.end)
    {
        SetMeasureMode();
    }

    switch (m_pass)
    {
        case Pass::Write:   ME_MEMCPY(m_binary.cursor, data, size); break;
        case Pass::Read:    ME_MEMCPY(data, m_binary.cursor, size); break;
        case Pass::Measure: break;
        case Pass::Verify:  ME_ASSERT(ME_MEMCMP(data, m_binary.cursor, size) == 0); break;
    }

    m_binary.cursor += size;
}

void meChunker::DoPrimitive(void* data, u32 size, PrimitiveKind kind)
{
    if (m_serializationMode == meSerializationMode_Binary)
    {
        DoBinaryVoid(data, size);
        return;
    }
    DoTextPrimitive(data, size, kind);
}

void meChunker::Do(bool& x)
{
    DoPrimitive(&x, (u32)sizeof(x), PrimitiveKind::Bool);
}

void meChunker::DoBytes(meSpan& span, meAllocator* readAllocator)
{
    if (m_serializationMode == meSerializationMode_Binary)
    {
        u32 size = (u32)span.size;
        Do(size);
        if (IsReadMode())
        {
            ME_ASSERT(readAllocator);
            span = MEALLOC(readAllocator, size);
        }
        DoBinaryVoid(span.data, size);
        return;
    }

    DoTextBytes(span, readAllocator);
}

void meChunker::DoTextPrimitive(void* data, u32 size, PrimitiveKind kind)
{
    ME_ASSERT(m_text.current);
    json& current = TextJson(m_text.current);
    if (IsWriteMode() || IsMeasureMode())
    {
        switch (kind)
        {
            case PrimitiveKind::S8:   current = *(s8*)data; return;
            case PrimitiveKind::U8:   current = *(u8*)data; return;
            case PrimitiveKind::S16:  current = *(s16*)data; return;
            case PrimitiveKind::U16:  current = *(u16*)data; return;
            case PrimitiveKind::S32:  current = *(s32*)data; return;
            case PrimitiveKind::U32:  current = *(u32*)data; return;
            case PrimitiveKind::S64:  current = *(s64*)data; return;
            case PrimitiveKind::U64:  current = *(u64*)data; return;
            case PrimitiveKind::F32:  current = *(f32*)data; return;
            case PrimitiveKind::F64:  current = *(f64*)data; return;
            case PrimitiveKind::Bool: current = *(bool*)data; return;
            case PrimitiveKind::Long:  current = *(long*)data; return;
            case PrimitiveKind::ULong: current = *(unsigned long*)data; return;
            case PrimitiveKind::WChar: current = (u32)*(wchar_t*)data; return;
            default: break;
        }
    }
    if (IsReadMode())
    {
        switch (kind)
        {
            case PrimitiveKind::S8:   *(s8*)data = current.get<s8>(); return;
            case PrimitiveKind::U8:   *(u8*)data = current.get<u8>(); return;
            case PrimitiveKind::S16:  *(s16*)data = current.get<s16>(); return;
            case PrimitiveKind::U16:  *(u16*)data = current.get<u16>(); return;
            case PrimitiveKind::S32:  *(s32*)data = current.get<s32>(); return;
            case PrimitiveKind::U32:  *(u32*)data = current.get<u32>(); return;
            case PrimitiveKind::S64:  *(s64*)data = current.get<s64>(); return;
            case PrimitiveKind::U64:  *(u64*)data = current.get<u64>(); return;
            case PrimitiveKind::F32:  *(f32*)data = current.get<f32>(); return;
            case PrimitiveKind::F64:  *(f64*)data = current.get<f64>(); return;
            case PrimitiveKind::Bool: *(bool*)data = current.get<bool>(); return;
            case PrimitiveKind::Long:  *(long*)data = current.get<long>(); return;
            case PrimitiveKind::ULong: *(unsigned long*)data = current.get<unsigned long>(); return;
            case PrimitiveKind::WChar: *(wchar_t*)data = (wchar_t)current.get<u32>(); return;
            default: break;
        }
    }
    if (IsVerifyMode())
    {
        ME_ASSERT(JsonValuesEqual(current, data, kind));
        return;
    }
    ME_ASSERT(!"Unsupported text primitive kind");
}

bool meChunker::BeginObject()
{
    if (m_serializationMode == meSerializationMode_Binary)
    {
        return true;
    }
    return DoTextBeginObject();
}

void meChunker::EndObject()
{
    if (m_serializationMode == meSerializationMode_Text)
    {
        DoTextEndObject();
    }
}

bool meChunker::Field(StringView name)
{
    if (m_serializationMode == meSerializationMode_Binary)
    {
        return true;
    }
    return DoTextField(name);
}

void meChunker::EndField()
{
    if (m_serializationMode == meSerializationMode_Text)
    {
        DoTextEndField();
    }
}

bool meChunker::BeginArray(u32& count)
{
    if (m_serializationMode == meSerializationMode_Binary)
    {
        Do(count);
        return true;
    }
    return DoTextBeginArray(count);
}

bool meChunker::BeginFixedArray(u32& count)
{
    if (m_serializationMode == meSerializationMode_Binary)
    {
        return true;
    }
    return DoTextBeginArray(count);
}

void meChunker::EndArray()
{
    if (m_serializationMode == meSerializationMode_Text)
    {
        DoTextEndArray();
    }
}

bool meChunker::Element(u32 index)
{
    if (m_serializationMode == meSerializationMode_Binary)
    {
        return true;
    }
    return DoTextElement(index);
}

void meChunker::EndElement()
{
    if (m_serializationMode == meSerializationMode_Text)
    {
        DoTextEndElement();
    }
}

void meChunker::DoTextBytes(meSpan& span, meAllocator* readAllocator)
{
    ME_ASSERT(m_text.current);
    json& current = TextJson(m_text.current);
    if (IsWriteMode() || IsMeasureMode())
    {
        current = std::string(span.data, span.size);
        return;
    }
    if (IsReadMode())
    {
        if (!current.is_string())
        {
            SetMeasureMode();
            span = {};
            return;
        }
        ME_ASSERT(readAllocator);
        std::string value = current.get<std::string>();
        span = MEALLOC(readAllocator, value.size());
        ME_MEMCPY(span.data, value.data(), value.size());
        return;
    }
    if (IsVerifyMode())
    {
        ME_ASSERT(current.is_string());
        std::string value = current.get<std::string>();
        ME_ASSERT(value.size() == span.size);
        ME_ASSERT(ME_MEMCMP(value.data(), span.data, span.size) == 0);
    }
}

void meChunker::DoString(String& str, meAllocator* readAllocator)
{
    if (m_serializationMode == meSerializationMode_Binary)
    {
        u32 len = (u32)str.len;
        Do(len);
        if (IsReadMode())
        {
            if (len == 0)
            {
                str = String();
                return;
            }
            ME_ASSERT(readAllocator);
            Allocation mem = MEALLOC(readAllocator, len + 1);
            DoArray((char*)mem.data, len);
            ((char*)mem.data)[len] = '\0';
            String readString = {};
            readString.data = (char*)mem.data;
            readString.len = len;
            readString.allocator = readAllocator;
            str = meMove(readString);
            return;
        }
        if (len > 0)
        {
            DoArray(str.data, len);
        }
        return;
    }

    ME_ASSERT(m_text.current);
    json& current = TextJson(m_text.current);
    if (IsWriteMode() || IsMeasureMode())
    {
        current = str.data ? std::string(str.data, str.len) : std::string();
        return;
    }
    if (IsReadMode())
    {
        ME_ASSERT(readAllocator);
        std::string value = current.get<std::string>();
        str = String(StringView(value.data(), value.size()), readAllocator);
        return;
    }
    if (IsVerifyMode())
    {
        std::string value = current.get<std::string>();
        ME_ASSERT(value.size() == str.len);
        ME_ASSERT(ME_MEMCMP(value.data(), str.data, str.len) == 0);
    }
}

bool meChunker::DoTextBeginObject()
{
    ME_ASSERT(m_text.current);
    json& current = TextJson(m_text.current);
    if (IsWriteMode() || IsMeasureMode())
    {
        current = json::object();
        return true;
    }
    return current.is_object();
}

void meChunker::DoTextEndObject()
{
}

bool meChunker::DoTextField(StringView name)
{
    ME_ASSERT(m_text.current);
    json& current = TextJson(m_text.current);
    std::string fieldName = StringViewToStdString(name);
    if (IsReadMode() && !current.contains(fieldName))
    {
        return false;
    }
    PushTextCurrent();
    m_text.current = &current[fieldName];
    return true;
}

void meChunker::DoTextEndField()
{
    PopTextCurrent();
}

bool meChunker::DoTextBeginArray(u32& count)
{
    ME_ASSERT(m_text.current);
    json& current = TextJson(m_text.current);
    if (IsWriteMode() || IsMeasureMode())
    {
        current = json::array();
        return true;
    }
    if (!current.is_array())
    {
        return false;
    }
    count = (u32)current.size();
    return true;
}

void meChunker::DoTextEndArray()
{
}

bool meChunker::DoTextElement(u32 index)
{
    ME_ASSERT(m_text.current);
    json& current = TextJson(m_text.current);
    if (IsReadMode() && index >= current.size())
    {
        return false;
    }
    PushTextCurrent();
    m_text.current = &current[index];
    return true;
}

void meChunker::DoTextEndElement()
{
    PopTextCurrent();
}

meOwningSpan meChunker::Finalize(meAllocator* allocator)
{
    if (m_serializationMode == meSerializationMode_Text)
    {
        return DoTextFinalize(allocator);
    }
    return meOwningSpan(m_binary.start, BytesProcessed());
}

void meChunker::PushTextCurrent()
{
    ME_ASSERT(m_serializationMode == meSerializationMode_Text);
    ME_ASSERT(m_text.currentStackSize < ARRAY_SIZE(m_text.currentStack));
    m_text.currentStack[m_text.currentStackSize++] = m_text.current;
}

void meChunker::PopTextCurrent()
{
    ME_ASSERT(m_serializationMode == meSerializationMode_Text);
    ME_ASSERT(m_text.currentStackSize > 0);
    m_text.current = m_text.currentStack[--m_text.currentStackSize];
    m_text.currentStack[m_text.currentStackSize] = nullptr;
}

meOwningSpan meChunker::DoTextFinalize(meAllocator* allocator)
{
    ME_ASSERT(m_text.root);
    if (!m_isValid)
    {
        return {};
    }
    std::string jsonStr = {};
    try
    {
        jsonStr = TextJson(m_text.root).dump(4);
    }
    catch (const json::exception& e)
    {
        LOG_ERROR("Text serialization failed while writing document: %s", e.what());
        m_isValid = false;
        return {};
    }
    if (IsMeasureMode())
    {
        return meOwningSpan((char*)nullptr, jsonStr.size());
    }
    if (m_text.outputBuffer)
    {
        ME_ASSERT(m_text.outputBuffer.size >= jsonStr.size());
        ME_MEMCPY(m_text.outputBuffer.data, jsonStr.data(), jsonStr.size());
        return meOwningSpan(m_text.outputBuffer.data, jsonStr.size());
    }
    ME_ASSERT(allocator);
    Allocation mem = MEALLOC(allocator, jsonStr.size() + 1);
    ME_MEMCPY(mem.data, jsonStr.data(), jsonStr.size());
    ((char*)mem.data)[jsonStr.size()] = '\0';
    return meOwningSpan(mem.data, jsonStr.size());
}

namespace
{
    struct _SmokeState { u32 a; f32 b; bool c; s64 d; };

    static void _DoSmokeState(_SmokeState& s, meChunker& w)
    {
        w.Do(s.a); w.Do(s.b); w.Do(s.c); w.Do(s.d);
        w.DoMarker("SmokeState");
    }
}

bool meChunkerTests()
{
    _SmokeState original = { 0xDEADBEEF, 3.14f, true, -1234567890LL };
    _SmokeState restored = {};

    meChunker measure(meSerializationMode_Binary, meChunker::Pass::Measure, nullptr, meSpan((char*)nullptr, 0));
    _DoSmokeState(original, measure);
    u64 needed = measure.BytesProcessed();
    ME_ASSERT(needed > 0);

    meOwningSpan blob = MEALLOC(GetSystemAllocator(), needed);
    meChunker write(meSerializationMode_Binary, meChunker::Pass::Write, nullptr, blob);
    _DoSmokeState(original, write);
    ME_ASSERT(write.IsWriteMode());
    ME_ASSERT(write.BytesProcessed() == needed);

    meChunker read(meSerializationMode_Binary, meChunker::Pass::Read, nullptr, blob);
    _DoSmokeState(restored, read);
    ME_ASSERT(read.IsReadMode());
    ME_ASSERT(restored.a == original.a && restored.b == original.b
           && restored.c == original.c && restored.d == original.d);

    meChunker verify(meSerializationMode_Binary, meChunker::Pass::Verify, nullptr, blob);
    _DoSmokeState(original, verify);
    ME_ASSERT(verify.IsVerifyMode());

    MEFREE(GetSystemAllocator(), blob.data);
    return true;
}
