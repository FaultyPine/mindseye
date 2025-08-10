#include "vulkan_backend.h"
#include "render/renderer_frontend.h"

#include "core/me_core.h"
#include "platform/me_os.h"
#include "core/me_log.h"
#include "core/containers/dynarray.h"

#define VK_USE_PLATFORM_WIN32_KHR
#define VOLK_IMPLEMENTATION 
#include "volk/volk.h"
#include "vkBootstrap/VkBootstrap.cpp"

struct RendererKitchen 
{
    VkQueue graphics_queue;
    VkQueue present_queue;

    DynArray(VkImage) swapchain_images;
    DynArray(VkImageView) swapchain_image_views;
    DynArray(VkFramebuffer) framebuffers;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;

    VkCommandPool command_pool;
    DynArray(VkCommandBuffer) command_buffers;

    DynArray(VkSemaphore) available_semaphores;
    DynArray(VkSemaphore) finished_semaphore;
    DynArray(VkFence) in_flight_fences;
    DynArray(VkFence) image_in_flight;
    size_t current_frame = 0;
};

void VulkanBackendInitialize(EngineContext* engine)
{
    Renderer*& renderer = engine->renderer;
    renderer = ArenaAllocType(&engine->engineArena, Renderer, 1);
    constexpr u64 RENDERER_ARENA_SIZE = MEGABYTES_BYTES(200);
    renderer->rendererPersistentArena = ArenaInit(RENDERER_ARENA_SIZE, "Renderer", *engine->engineArena);
    Arena* arena = &renderer->rendererPersistentArena;
    vkb::InstanceBuilder builder;
    auto inst_ret = builder.set_app_name (engine->appName)
                        .request_validation_layers ()
                        .use_default_debug_messenger ()
                        .build ();
    if (!inst_ret) { /* report */ }
    vkb::Instance vkb_inst = inst_ret.value ();

    OSStateView* osData = engine->osData;
    VkSurfaceKHR surface = {};
#ifdef OS_WINDOWS
    VkWin32SurfaceCreateInfoKHR surfCreateInfo = {};
    surfCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfCreateInfo.pNext = nullptr;
    surfCreateInfo.hinstance = (HINSTANCE)osData->hinstance;
    surfCreateInfo.hwnd = (HWND)osData->hwnd;
    vkCreateWin32SurfaceKHR(vkb_inst.instance, &surfCreateInfo, nullptr, &surface);
#else
#error vulkan surface fetch for unknown platform
#endif

    vkb::PhysicalDeviceSelector selector{ vkb_inst };
    auto phys_ret = selector.set_surface (surface)
                        .set_minimum_version (1, 1)
                        .require_dedicated_transfer_queue ()
                        .select ();
    if (!phys_ret) { /* report */ }

    vkb::DeviceBuilder device_builder{ phys_ret.value () };
    auto dev_ret = device_builder.build ();
    if (!dev_ret) { /* report */ }
    vkb::Device vkb_device = dev_ret.value ();

    auto graphics_queue_ret = vkb_device.get_queue (vkb::QueueType::graphics);
    if (!graphics_queue_ret)  { /* report */ }
    VkQueue graphics_queue = graphics_queue_ret.value ();
    vkb::SwapchainBuilder swapchain_builder{ vkb_device };
    auto swap_ret = swapchain_builder.build();
    if (!swap_ret) 
    {
        LOG_ERROR("Error building swapchain %s", swap_ret.error().message().c_str());
        return;
    }
    vkb::Swapchain vkb_swapchain = swap_ret.value();

    RendererKitchen kitchen = {};
    // TODO: fill out the kitchen
    kitchen.graphics_queue = graphics_queue;
    //kitchen.swapchain_images = DynArrayCreate( vkb_swapchain.get_images()->data());
}

void VulkanBackendTeardown()
{
    
}