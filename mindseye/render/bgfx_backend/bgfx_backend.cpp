
#include "bgfx_backend.h"

#include "external/bgfx/bgfx/include/bgfx/bgfx.h"

#include "core/me_core.h"

#define ME_TREEMAP_IMPLEMENTATION
#include "algo/me_treemap.h"
#include "shaders/fs_rect.h"
#include "shaders/vs_rect.h"

#include "core/me_math.h"

#include "bgfx/bgfx/include/bgfx/bgfx.h"
#include "bgfx/bx/include/bx/bx.h"
#include "external/bgfx/bgfx/examples/common/imgui/bgfx_imgui.cpp"

struct PosColorVertex
{
    float x, y, z;
    uint32_t rgba;
};


static PosColorVertex s_rectVertices[] =
{
    { -0.5f,  0.5f, 0.0f, 0xff00ff00 }, // Top-left
    {  0.5f,  0.5f, 0.0f, 0xff00ff00 }, // Top-right
    { -0.5f, -0.5f, 0.0f, 0xff00ff00 }, // Bottom-left
    {  0.5f, -0.5f, 0.0f, 0xff00ff00 }, // Bottom-right
};

void SetRectVerts(glm::vec2 min, glm::vec2 max, u32 color)
{
    s_rectVertices[0].x = min.x;
    s_rectVertices[0].y = max.y;
    s_rectVertices[0].rgba = color;
    
    s_rectVertices[1].x = max.x;
    s_rectVertices[1].y = max.y;
    s_rectVertices[1].rgba = color;
    
    s_rectVertices[2].x = min.x;
    s_rectVertices[2].y = min.y;
    s_rectVertices[2].rgba = color;
    
    s_rectVertices[3].x = max.x;
    s_rectVertices[3].y = min.y;
    s_rectVertices[3].rgba = color;
}

static const uint16_t s_rectIndices[] =
{
    0, 1, 2,
    1, 3, 2,
};

static bgfx::VertexLayout s_layout;

void InitRectLayout()
{
    s_layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
        .end();
}

void OnWindowResize(int width, int height)
{
    bgfx::reset(width, height);
    bgfx::setViewRect(0, 0, 0, width, height);
}


struct BgfxCallback : public bgfx::CallbackI
{
	virtual ~BgfxCallback()
	{
	}

	virtual void fatal(const char* _filePath, uint16_t _line, bgfx::Fatal::Enum _code, const char* _str) override
	{
		BX_UNUSED(_filePath, _line);

		// Something unexpected happened, inform user and bail out.
		bx::debugPrintf("Fatal error: 0x%08x: %s", _code, _str);

		// Must terminate, continuing will cause crash anyway.
		abort();
	}

	virtual void traceVargs(const char* _filePath, uint16_t _line, const char* _format, va_list _argList) override
	{
        if (GetEngineCtx()->renderer->rendererLoggingEnabled)
        {
            bx::debugPrintf("%s (%d): ", _filePath, _line);
            bx::debugPrintfVargs(_format, _argList);
        }
	}

	virtual void profilerBegin(const char* /*_name*/, uint32_t /*_abgr*/, const char* /*_filePath*/, uint16_t /*_line*/) override
	{
	}

	virtual void profilerBeginLiteral(const char* /*_name*/, uint32_t /*_abgr*/, const char* /*_filePath*/, uint16_t /*_line*/) override
	{
	}

	virtual void profilerEnd() override
	{
	}

	virtual uint32_t cacheReadSize(uint64_t _id) override
	{
        return 0;
	}

	virtual bool cacheRead(uint64_t _id, void* _data, uint32_t _size) override
	{
        return false;
	}

	virtual void cacheWrite(uint64_t _id, const void* _data, uint32_t _size) override
	{
	}

	virtual void screenShot(const char* _filePath, uint32_t _width, uint32_t _height, uint32_t _pitch, const void* _data, uint32_t /*_size*/, bool _yflip) override
	{
	}

	virtual void captureBegin(uint32_t _width, uint32_t _height, uint32_t /*_pitch*/, bgfx::TextureFormat::Enum /*_format*/, bool _yflip) override
	{
	}

	virtual void captureEnd() override
	{
	}

	virtual void captureFrame(const void* _data, uint32_t /*_size*/) override
	{
	}

};

void BgfxRendererBackend::Initialize(EngineContext* engine)
{
    rendererPersistentArena = ArenaInit(MEGABYTES_BYTES(50), "Renderer Persistent", &engine->engineArena);
    rendererFrameArena = ArenaInit(MEGABYTES_BYTES(10), "Renderer Frame", &rendererPersistentArena);
    // If multiple systems are trying to subscribe here, it's time to make this an actual event, rather than one fn ptr
    ME_ASSERT(!engine->osData->onResizeCB);
    engine->osData->onResizeCB = OnWindowResize;
    bgfx::Init init;
    init.type = bgfx::RendererType::OpenGL;
    init.vendorId = BGFX_PCI_ID_NONE; // prioritize integrated? discrete? microsft/nvidia/amd adapter? None means do it automatically
    init.platformData.ndt = nullptr;
    init.platformData.nwh = engine->osData->hwnd; // bgfx renderer backend needs platform window handle, this is hardcoded to windows rn. If another platform is supported in the future, this'll throw a compiler error
    init.resolution.width = engine->osData->windowWidth;
    init.resolution.height = engine->osData->windowHeight;
    init.resolution.reset = BGFX_RESET_VSYNC;
    init.callback = MENEW(&rendererPersistentArena, BgfxCallback);
    bgfx::init(init);
    bgfx::setDebug(BGFX_DEBUG_TEXT);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, init.resolution.width, init.resolution.height);
    imguiCreate();
    engine->renderer->rendererLoggingEnabled = false; // tmp
}


void BgfxRendererBackend::Teardown(EngineContext* engine)
{
    imguiDestroy();
    bgfx::shutdown();
}

uint32_t float4_to_u32_argb(const glm::vec4& float_color) {
    uint8_t a = static_cast<uint8_t>(std::floor(Math::Clamp(float_color.a * 255.0f, 0.0f, 255.0f)));
    uint8_t r = static_cast<uint8_t>(std::floor(Math::Clamp(float_color.r * 255.0f, 0.0f, 255.0f)));
    uint8_t g = static_cast<uint8_t>(std::floor(Math::Clamp(float_color.g * 255.0f, 0.0f, 255.0f)));
    uint8_t b = static_cast<uint8_t>(std::floor(Math::Clamp(float_color.b * 255.0f, 0.0f, 255.0f)));

    uint32_t res =  (static_cast<uint32_t>(a) << 24) |
           (static_cast<uint32_t>(r)) |
           (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16);
    return res;
}

int compare_fn(const void *a, const void *b) {
    return (int)(*(float*)b - *(float*)a);
}

double snap_to_increment(double value, double increment) {
    if (increment == 0.0) {
        return value; // Avoid division by zero
    }
    return std::round(value / increment) * increment;
}

void* BgfxRendererBackend::RenderScene(RenderInput* input)
{
    static bool initialized = false;
    if (!initialized)
    {
        InitRectLayout();
        initialized = true;
    }
    const MouseState& mouseState = input->osData.mouseState;
    const bgfx::Stats* stats = bgfx::getStats();
    // Set view and clear
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x443355FF, 1.0f, 0);
 
    // Use a simple shader (replace with your own shader handles)
    const bgfx::Memory* fsmem = bgfx::alloc(sizeof(fs_rect)+1);
    ME_MEMCPY(fsmem->data, fs_rect, sizeof(fs_rect));
    fsmem->data[fsmem->size-1] = '\0';

    const bgfx::Memory* vsmem = bgfx::alloc(sizeof(vs_rect)+1);
    ME_MEMCPY(vsmem->data, vs_rect, sizeof(vs_rect));
    vsmem->data[vsmem->size-1] = '\0';

    bgfx::ShaderHandle fsHandle = bgfx::createShader(fsmem);
    bgfx::ShaderHandle vsHandle = bgfx::createShader(vsmem);
    bgfx::ProgramHandle program = bgfx::createProgram(vsHandle, fsHandle); // Load or reference a valid bgfx shader program

    float data[] = {5,546,346,354,6634,763,457,357,4536,7456,7,4567,435,2345,234,52,3452,4,523};
    const char* data_names[] = {"hi","hi","hi","hi","hi","hi","hi","hi","hi","hi","hi","hi","hi","hi","hi","hi","hi","hi","hi"};
    STATIC_ASSERT(ARRAY_SIZE(data) == ARRAY_SIZE(data_names));
    auto bounds = meTreemapRect{0,0,(float)stats->width, (float)stats->height};
    u32 numItems = ARRAY_SIZE(data);
    meTreemapItem* items = ArenaAllocType(&rendererFrameArena, meTreemapItem, numItems);
    qsort(data, numItems, sizeof(float), compare_fn);
    me_treemap_normalize_sizes(data, numItems, bounds.w, bounds.h);
    me_treemap_squarify(data, numItems, (void**)data_names, bounds, items, true);

    float biggest = 0.0f;
    for (u32 i = 0; i < numItems; i++) biggest = data[i] > biggest ? data[i] : biggest;

    for (u32 i = 0; i < numItems; i++)
    {
        const meTreemapItem& item = items[i];
        // glm::vec4 color = glm::vec4(item.value, 0, 0, 1);
        float colMag = (float)(snap_to_increment(item.value, 2)) / (float)biggest;
        glm::vec4 color = glm::vec4(colMag, 0, 0, 1);
        glm::vec2 min = glm::vec2(item.rect.x, item.rect.y);
        glm::vec2 max = min + glm::vec2(item.rect.w, item.rect.h);
        bgfx::dbgTextPrintf((u16)min.x, (u16)min.y, 0x0f, (char*)item.userData);

        // glm::vec2 min = glm::vec2(i * 50, i * 50);
        // glm::vec2 max = min + 50.0f;
        min /= glm::vec2(bounds.w, bounds.h); // make it 0-1
        min -= 0.5f; // make it [-0.5, 0.5]
        max /= glm::vec2(bounds.w, bounds.h);
        max -= 0.5f; // make it [-0.5, 0.5]
        SetRectVerts(min, max, float4_to_u32_argb(color));

        auto mem = bgfx::copy(s_rectVertices, sizeof(s_rectVertices));
        bgfx::VertexBufferHandle vbh = bgfx::createVertexBuffer(
            mem,
            s_layout
        );
        auto imem = bgfx::copy(s_rectIndices, sizeof(s_rectIndices));
        bgfx::IndexBufferHandle ibh = bgfx::createIndexBuffer(imem);
        bgfx::setVertexBuffer(0, vbh);
        bgfx::setIndexBuffer(ibh);
        bgfx::setState(BGFX_STATE_WRITE_RGB
            | BGFX_STATE_WRITE_A
            | BGFX_STATE_WRITE_Z
            | BGFX_STATE_DEPTH_TEST_LESS
            | BGFX_STATE_CULL_CCW
            | BGFX_STATE_MSAA);
        bgfx::submit(0, program);
        bgfx::destroy(vbh);
        bgfx::destroy(ibh);
    }
    imguiBeginFrame(mouseState.mouseX
        ,  mouseState.mouseY
        ,  (TEST_BIT(mouseState.buttons, MouseState::LBUTTON) ? IMGUI_MBUT_LEFT   : 0)
			| (TEST_BIT(mouseState.buttons, MouseState::RBUTTON) ? IMGUI_MBUT_RIGHT  : 0)
			| (TEST_BIT(mouseState.buttons, MouseState::MBUTTON) ? IMGUI_MBUT_MIDDLE : 0)
        , mouseState.scroll
        , u16(input->osData.windowWidth)
        , u16(input->osData.windowHeight)
        );

    imguiEndFrame();


    ArenaClear(&rendererFrameArena);
    bgfx::frame();
    return nullptr;
}