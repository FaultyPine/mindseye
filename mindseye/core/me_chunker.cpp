#include "core/me_chunker.h"

STATIC_ASSERT(sizeof(bool) == 1, "meChunker: unexpected bool size");

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

    meOwningSpan blob = meChunkerSave(GetSystemAllocator(), [&](meChunker& w){ _DoSmokeState(original, w); });
    bool ok = meChunkerRead(blob, [&](meChunker& w){ _DoSmokeState(restored, w); });
    ME_ASSERT(ok);
    ME_ASSERT(restored.a == original.a && restored.b == original.b
           && restored.c == original.c && restored.d == original.d);
    ME_ASSERT(meChunkerVerify(blob, [&](meChunker& w){ _DoSmokeState(original, w); }));

    MEFREE(GetSystemAllocator(), blob.data);
    return true;
}
