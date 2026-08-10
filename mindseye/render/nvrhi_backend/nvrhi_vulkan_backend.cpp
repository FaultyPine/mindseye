#include "nvrhi_vulkan_backend.h"

#include "core/me_core.h"
#include "core/me_log.h"
#include "core/me_profile.h"
#include "core/containers/dynarray.h"
#include "platform/me_os.h"
#include "core/me_scope_exit.h"

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#define NOMINMAX
#include <windows.h>
#define VOLK_IMPLEMENTATION
#include "Volk/volk.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

#include <vulkan/vulkan.hpp>

#include "nvrhi/nvrhi.h"
#include "nvrhi/vulkan.h"
#include "nvrhi/validation.h"

struct NvrhiMessageCallback : public nvrhi::IMessageCallback
{
    void message(nvrhi::MessageSeverity severity, const char* messageText) override
    {
        switch (severity)
        {
            case nvrhi::MessageSeverity::Fatal:
            case nvrhi::MessageSeverity::Error:
                LOG_ERROR("NVRHI: %s", messageText);
                break;
            case nvrhi::MessageSeverity::Warning:
                LOG_WARN("NVRHI: %s", messageText);
                break;
            default:
                LOG_INFO("NVRHI: %s", messageText);
                break;
        }
    }
};

struct NvrhiGpuBuffer
{
    nvrhi::IBuffer* buffer = nullptr;
    meMeshVertexLayoutType layout = 0;
};

struct NvrhiGpuTexture
{
    nvrhi::ITexture* texture = nullptr;
    nvrhi::ISampler* sampler = nullptr;
};

struct NvrhiVulkanState
{
    meAllocator* allocator = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent = {};
    VkFormat imguiColorAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkFence imageAcquireFence = VK_NULL_HANDLE;
    u32 graphicsQueueFamily = 0;
    u32 currentImageIndex = U32_INVALID_ID;

    NvrhiMessageCallback messages;
    nvrhi::DeviceHandle nvrhiDevice;
    nvrhi::CommandListHandle commandList;

    DynArray<VkImage> swapchainImages;
    DynArray<nvrhi::ITexture*> swapchainTextures;
    DynArray<NvrhiGpuBuffer> buffers;
    DynArray<NvrhiGpuTexture> textures;

    u32 width = 0;
    u32 height = 0;

    // TODO: proper "capabilities" struct
    bool rayTracingSupported = false;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*)
{
    const char* message = callbackData && callbackData->pMessage ? callbackData->pMessage : "No validation message";

    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        LOG_ERROR("Vulkan validation: %s", message);
    }
    else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        LOG_WARN("Vulkan validation: %s", message);
    }
    else
    {
        LOG_INFO("Vulkan validation: %s", message);
    }

    return VK_FALSE;
}

static const char* VkResultName(VkResult result)
{
    switch (result)
    {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
        case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
        default: return "VkResult error";
    }
}

static PFN_vkVoidFunction ImGuiVulkanLoadFunction(const char* functionName, void* userData)
{
    NvrhiVulkanState* state = (NvrhiVulkanState*)userData;
    PFN_vkVoidFunction result = vkGetDeviceProcAddr(state->device, functionName);
    if (!result)
    {
        result = vkGetInstanceProcAddr(state->instance, functionName);
    }
    return result;
}

static bool HasInstanceLayer(NvrhiVulkanState& state, const char* layerName)
{
    u32 layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    if (layerCount == 0)
    {
        return false;
    }

    DynArray<VkLayerProperties> layers = DynArrayCreateWithReserved<VkLayerProperties>(state.allocator, layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data);

    bool found = false;
    for (DynArray_Foreach(layers, i))
    {
        if (StringCompare(StringFromCString(layers[i].layerName), StringFromCString(layerName)))
        {
            found = true;
            break;
        }
    }

    DynArrayDestroy(layers);
    return found;
}

static bool HasInstanceExtension(NvrhiVulkanState& state, const char* extensionName)
{
    u32 extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    if (extensionCount == 0)
    {
        return false;
    }

    DynArray<VkExtensionProperties> extensions = DynArrayCreateWithReserved<VkExtensionProperties>(state.allocator, extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data);

    bool found = false;
    for (DynArray_Foreach(extensions, i))
    {
        if (StringCompare(StringFromCString(extensions[i].extensionName), StringFromCString(extensionName)))
        {
            found = true;
            break;
        }
    }

    DynArrayDestroy(extensions);
    return found;
}

static nvrhi::Format NvrhiFormatFromVk(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_B8G8R8A8_UNORM: return nvrhi::Format::BGRA8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB: return nvrhi::Format::SBGRA8_UNORM;
        case VK_FORMAT_R8G8B8A8_UNORM: return nvrhi::Format::RGBA8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB: return nvrhi::Format::SRGBA8_UNORM;
        default: return nvrhi::Format::BGRA8_UNORM;
    }
}

static void ReleaseSwapchainTextures(NvrhiVulkanState& state)
{
    for (DynArray_Foreach(state.swapchainTextures, i))
    {
        if (state.swapchainTextures[i])
        {
            state.swapchainTextures[i]->Release();
            state.swapchainTextures[i] = nullptr;
        }
    }
    DynArrayClear(state.swapchainTextures);
}

static void ReleaseBuffers(NvrhiVulkanState& state)
{
    for (DynArray_Foreach(state.buffers, i))
    {
        if (state.buffers[i].buffer)
        {
            state.buffers[i].buffer->Release();
            state.buffers[i].buffer = nullptr;
        }
    }
    DynArrayClear(state.buffers);
}

static void ReleaseTextures(NvrhiVulkanState& state)
{
    for (DynArray_Foreach(state.textures, i))
    {
        if (state.textures[i].sampler)
        {
            state.textures[i].sampler->Release();
            state.textures[i].sampler = nullptr;
        }
        if (state.textures[i].texture)
        {
            state.textures[i].texture->Release();
            state.textures[i].texture = nullptr;
        }
    }
    DynArrayClear(state.textures);
}

static bool HasDeviceExtensions(NvrhiVulkanState& state, VkPhysicalDevice physicalDevice, const char* const* requiredExtensions, u32 requiredExtensionCount)
{
    u32 extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    if (extensionCount == 0)
    {
        return requiredExtensionCount == 0;
    }

    DynArray<VkExtensionProperties> extensions = DynArrayCreateWithReserved<VkExtensionProperties>(state.allocator, extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data);

    bool result = true;
    for (u32 requiredIdx = 0; requiredIdx < requiredExtensionCount; ++requiredIdx)
    {
        bool found = false;
        for (DynArray_Foreach(extensions, extensionIdx))
        {
            const VkExtensionProperties& extension = extensions[extensionIdx];
            if (StringCompare(StringFromCString(extension.extensionName), StringFromCString(requiredExtensions[requiredIdx])))
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            result = false;
            break;
        }
    }

    DynArrayDestroy(extensions);
    return result;
}

static bool FindGraphicsPresentQueue(NvrhiVulkanState& state, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, u32* outQueueFamily)
{
    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0)
    {
        return false;
    }

    DynArray<VkQueueFamilyProperties> queueFamilies = DynArrayCreateWithReserved<VkQueueFamilyProperties>(state.allocator, queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data);

    bool result = false;
    for (u32 queueFamily = 0; queueFamily < queueFamilyCount; ++queueFamily)
    {
        VkBool32 presentSupported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFamily, surface, &presentSupported);

        if ((queueFamilies[queueFamily].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupported)
        {
            *outQueueFamily = queueFamily;
            result = true;
            break;
        }
    }

    DynArrayDestroy(queueFamilies);
    return result;
}

static bool SelectPhysicalDevice(NvrhiVulkanState& state, const char* const* deviceExtensions, u32 deviceExtensionCount)
{
    u32 physicalDeviceCount = 0;
    vkEnumeratePhysicalDevices(state.instance, &physicalDeviceCount, nullptr);
    if (physicalDeviceCount == 0)
    {
        return false;
    }

    DynArray<VkPhysicalDevice> physicalDevices = DynArrayCreateWithReserved<VkPhysicalDevice>(state.allocator, physicalDeviceCount);
    vkEnumeratePhysicalDevices(state.instance, &physicalDeviceCount, physicalDevices.data);

    bool result = false;
    for (DynArray_Foreach(physicalDevices, physicalDeviceIdx))
    {
        VkPhysicalDevice physicalDevice = physicalDevices[physicalDeviceIdx];
        u32 queueFamily = U32_INVALID_ID;
        if (!FindGraphicsPresentQueue(state, physicalDevice, state.surface, &queueFamily))
        {
            continue;
        }
        if (!HasDeviceExtensions(state, physicalDevice, deviceExtensions, deviceExtensionCount))
        {
            continue;
        }

        VkPhysicalDeviceRayQueryFeaturesKHR rayQuery = {};
        rayQuery.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipeline = {};
        rtPipeline.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        rtPipeline.pNext = &rayQuery;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration = {};
        acceleration.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        acceleration.pNext = &rtPipeline;

        VkPhysicalDeviceVulkan13Features features13 = {};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.pNext = &acceleration;

        VkPhysicalDeviceVulkan12Features features12 = {};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.pNext = &features13;

        VkPhysicalDeviceFeatures2 features2 = {};
        features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features2.pNext = &features12;
        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);

        if (!features12.bufferDeviceAddress ||
            !features12.descriptorIndexing ||
            !features12.runtimeDescriptorArray ||
            !features13.dynamicRendering ||
            !features13.synchronization2 ||
            !acceleration.accelerationStructure ||
            !rtPipeline.rayTracingPipeline ||
            !rayQuery.rayQuery)
        {
            continue;
        }

        state.physicalDevice = physicalDevice;
        state.graphicsQueueFamily = queueFamily;
        result = true;
        break;
    }

    DynArrayDestroy(physicalDevices);
    return result;
}

static VkExtent2D ChooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities, u32 width, u32 height)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }

    VkExtent2D extent = { width, height };
    extent.width = CLAMP(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height = CLAMP(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

static VkSurfaceFormatKHR ChooseSwapchainFormat(const DynArray<VkSurfaceFormatKHR>& formats)
{
    for (DynArray_Foreach(formats, i))
    {
        const VkSurfaceFormatKHR& format = formats[i];
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }

    return formats[0];
}

static bool CreateSwapchain(NvrhiVulkanState& state, u32 width, u32 height)
{
    if (state.swapchain)
    {
        state.nvrhiDevice->waitForIdle();
        ReleaseSwapchainTextures(state);
        DynArrayClear(state.swapchainImages);
        vkDestroySwapchainKHR(state.device, state.swapchain, nullptr);
        state.swapchain = VK_NULL_HANDLE;
    }

    VkSurfaceCapabilitiesKHR capabilities = {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.physicalDevice, state.surface, &capabilities);

    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(state.physicalDevice, state.surface, &formatCount, nullptr);
    if (formatCount == 0)
    {
        LOG_ERROR("Vulkan surface does not expose any swapchain formats");
        return false;
    }
    DynArray<VkSurfaceFormatKHR> formats = DynArrayCreateWithReserved<VkSurfaceFormatKHR>(state.allocator, formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(state.physicalDevice, state.surface, &formatCount, formats.data);

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapchainFormat(formats);
    VkExtent2D extent = ChooseSwapchainExtent(capabilities, width, height);
    DynArrayDestroy(formats);

    u32 imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
    {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = state.surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped = VK_TRUE;

    VkResult swapchainResult = vkCreateSwapchainKHR(state.device, &swapchainInfo, nullptr, &state.swapchain);
    if (swapchainResult != VK_SUCCESS)
    {
        LOG_ERROR("vkCreateSwapchainKHR failed: %s", VkResultName(swapchainResult));
        return false;
    }

    state.swapchainFormat = surfaceFormat.format;
    state.swapchainExtent = extent;
    state.width = extent.width;
    state.height = extent.height;

    u32 swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(state.device, state.swapchain, &swapchainImageCount, nullptr);
    if (swapchainImageCount > DynArrayGetCapacity(state.swapchainImages))
    {
        DynArrayDestroy(state.swapchainImages);
        state.swapchainImages = DynArrayCreate<VkImage>(state.allocator, swapchainImageCount);
    }
    DynArrayClear(state.swapchainImages);
    state.swapchainImages.header.size = swapchainImageCount;
    vkGetSwapchainImagesKHR(state.device, state.swapchain, &swapchainImageCount, state.swapchainImages.data);

    if (swapchainImageCount > DynArrayGetCapacity(state.swapchainTextures))
    {
        ReleaseSwapchainTextures(state);
        DynArrayDestroy(state.swapchainTextures);
        state.swapchainTextures = DynArrayCreate<nvrhi::ITexture*>(state.allocator, swapchainImageCount);
    }

    for (u32 i = 0; i < DynArrayGetSize(state.swapchainImages); ++i)
    {
        nvrhi::TextureDesc desc;
        desc.width = state.width;
        desc.height = state.height;
        desc.format = NvrhiFormatFromVk(state.swapchainFormat);
        desc.debugName = "SwapchainImage";
        desc.isRenderTarget = true;
        desc.initialState = nvrhi::ResourceStates::Present;
        desc.keepInitialState = true;

        nvrhi::TextureHandle texture = state.nvrhiDevice->createHandleForNativeTexture(
            nvrhi::ObjectTypes::VK_Image,
            nvrhi::Object(state.swapchainImages[i]),
            desc);

        if (!texture)
        {
            LOG_ERROR("Failed to wrap swapchain image %u with NVRHI", i);
            return false;
        }

        DynArrayPush(state.swapchainTextures, texture.Detach());
    }

    return true;
}

static void OnNvrhiWindowResize(int, int)
{
    // The NVRHI backend rebuilds its swapchain lazily at the start of RenderScene.
}

meAllocator* ImguiGetMeAllocator()
{
    return GetDefaultAllocator();
}

static void* imguiAlloc(size_t sz, void* usrdata)
{
    return ImguiGetMeAllocator()->meAlloc(sz);
}
static void imguiFree(void* ptr, void* usrdata)
{
    ImguiGetMeAllocator()->meFree(ptr);
}

void NvrhiVulkanRendererBackend::Initialize(EngineContext* engine)
{
    ME_PROFILE_FUNCTION();

    backendType = RendererBackendType::NVRHI_VULKAN;
    rendererPersistentAllocator = MENEW(&engine->engineArena, Arena, MEGABYTES_BYTES(64), "NVRHI Renderer Persistent", &engine->engineArena);
    rendererFrameArena = ArenaInit(MEGABYTES_BYTES(16), "NVRHI Renderer Frame", rendererPersistentAllocator);
    
    IMGUI_CHECKVERSION();
    ImGui::SetAllocatorFunctions(imguiAlloc, imguiFree, NULL);
    ImGuiContext* imguiCtx = ImGui::CreateContext();
    ImGui::SetCurrentContext(imguiCtx);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(engine->osData->windowWidth, engine->osData->windowHeight);
    io.DeltaTime   = 1.0f / 60.0f;
    io.IniFilename = NULL;

    state = MENEW(&engine->engineArena, NvrhiVulkanState);
    state->allocator = rendererPersistentAllocator;
    state->swapchainImages = DynArrayCreate<VkImage>(state->allocator, 8);
    state->swapchainTextures = DynArrayCreate<nvrhi::ITexture*>(state->allocator, 8);
    state->buffers = DynArrayCreate<NvrhiGpuBuffer>(state->allocator, 4096);
    state->textures = DynArrayCreate<NvrhiGpuTexture>(state->allocator, 4096);
    engine->osData->onResizeCB = OnNvrhiWindowResize;

    VkResult volkResult = volkInitialize();
    if (volkResult != VK_SUCCESS)
    {
        LOG_ERROR("volkInitialize failed: %s", VkResultName(volkResult));
        return;
    }

    const char* instanceExtensions[3] =
    {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };
    u32 instanceExtensionCount = 2;

    const char* validationLayers[] =
    {
        "VK_LAYER_KHRONOS_validation",
    };

    bool validationLayerAvailable = HasInstanceLayer(*state, validationLayers[0]);
    bool debugUtilsAvailable = HasInstanceExtension(*state, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    bool validationAvailable = validationLayerAvailable && debugUtilsAvailable;

    VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo = {};
    if (validationAvailable)
    {
        instanceExtensions[instanceExtensionCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        debugMessengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugMessengerInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugMessengerInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugMessengerInfo.pfnUserCallback = VulkanDebugCallback;
    }
    else
    {
        LOG_WARN(
            "Vulkan validation disabled: %s=%s, %s=%s",
            validationLayers[0],
            validationLayerAvailable ? "available" : "missing",
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
            debugUtilsAvailable ? "available" : "missing");
    }

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = engine->appName.cstr();
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "Mindseye";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = instanceExtensionCount;
    instanceInfo.ppEnabledExtensionNames = instanceExtensions;
    if (validationAvailable)
    {
        instanceInfo.enabledLayerCount = ARRAY_SIZE(validationLayers);
        instanceInfo.ppEnabledLayerNames = validationLayers;
        instanceInfo.pNext = &debugMessengerInfo;
    }

    VkResult instanceResult = vkCreateInstance(&instanceInfo, nullptr, &state->instance);
    if (instanceResult != VK_SUCCESS)
    {
        LOG_ERROR("vkCreateInstance failed: %s", VkResultName(instanceResult));
        return;
    }

    volkLoadInstance(state->instance);
    if (validationAvailable)
    {
        VkResult debugMessengerResult = vkCreateDebugUtilsMessengerEXT(state->instance, &debugMessengerInfo, nullptr, &state->debugMessenger);
        if (debugMessengerResult != VK_SUCCESS)
        {
            LOG_WARN("vkCreateDebugUtilsMessengerEXT failed: %s", VkResultName(debugMessengerResult));
        }
    }

#ifdef OS_WINDOWS
    VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hinstance = (HINSTANCE)engine->osData->hinstance;
    surfaceInfo.hwnd = (HWND)engine->osData->hwnd;

    VkResult surfaceResult = vkCreateWin32SurfaceKHR(state->instance, &surfaceInfo, nullptr, &state->surface);
    if (surfaceResult != VK_SUCCESS)
    {
        LOG_ERROR("vkCreateWin32SurfaceKHR failed: %s", VkResultName(surfaceResult));
        return;
    }
#else
#error Unsupported/todo NVRHI Vulkan platform
#endif

    static const char* deviceExtensions[] =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    };

    if (!SelectPhysicalDevice(*state, deviceExtensions, ARRAY_SIZE(deviceExtensions)))
    {
        LOG_ERROR("No Vulkan device supports graphics, presentation, and the required ray tracing features");
        return;
    }

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {};
    rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    rayQueryFeatures.rayQuery = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {};
    rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipelineFeatures.pNext = &rayQueryFeatures;
    rtPipelineFeatures.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeatures = {};
    accelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelFeatures.pNext = &rtPipelineFeatures;
    accelFeatures.accelerationStructure = VK_TRUE;
    accelFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;

    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.pNext = &accelFeatures;
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.pNext = &features13;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = state->graphicsQueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.pNext = &features12;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = ARRAY_SIZE(deviceExtensions);
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    VkResult deviceResult = vkCreateDevice(state->physicalDevice, &deviceInfo, nullptr, &state->device);
    if (deviceResult != VK_SUCCESS)
    {
        LOG_ERROR("vkCreateDevice failed: %s", VkResultName(deviceResult));
        return;
    }

    volkLoadDevice(state->device);
    VULKAN_HPP_DEFAULT_DISPATCHER.init(state->instance, vkGetInstanceProcAddr, state->device);
    vkGetDeviceQueue(state->device, state->graphicsQueueFamily, 0, &state->graphicsQueue);

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkResult fenceResult = vkCreateFence(state->device, &fenceInfo, nullptr, &state->imageAcquireFence);
    if (fenceResult != VK_SUCCESS)
    {
        LOG_ERROR("vkCreateFence failed: %s", VkResultName(fenceResult));
        return;
    }

    nvrhi::vulkan::DeviceDesc deviceDesc = {};
    deviceDesc.errorCB = &state->messages;
    deviceDesc.instance = state->instance;
    deviceDesc.physicalDevice = state->physicalDevice;
    deviceDesc.device = state->device;
    deviceDesc.graphicsQueue = state->graphicsQueue;
    deviceDesc.graphicsQueueIndex = state->graphicsQueueFamily;
    deviceDesc.deviceExtensions = deviceExtensions;
    deviceDesc.numDeviceExtensions = ARRAY_SIZE(deviceExtensions);
    deviceDesc.bufferDeviceAddressSupported = true;

    state->nvrhiDevice = nvrhi::vulkan::createDevice(deviceDesc);
    if (!state->nvrhiDevice)
    {
        LOG_ERROR("nvrhi::vulkan::createDevice failed");
        return;
    }

    state->nvrhiDevice = nvrhi::validation::createValidationLayer(state->nvrhiDevice);
    state->commandList = state->nvrhiDevice->createCommandList();
    state->rayTracingSupported = state->nvrhiDevice->queryFeatureSupport(nvrhi::Feature::RayTracingPipeline);

    if (!CreateSwapchain(*state, engine->osData->windowWidth, engine->osData->windowHeight))
    {
        return;
    }

    
    { // IMGUI
        VkDescriptorPoolSize imguiPoolSize = {};
        imguiPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        imguiPoolSize.descriptorCount = 128;
    
        VkDescriptorPoolCreateInfo imguiPoolInfo = {};
        imguiPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        imguiPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        imguiPoolInfo.maxSets = 128;
        imguiPoolInfo.poolSizeCount = 1;
        imguiPoolInfo.pPoolSizes = &imguiPoolSize;
    
        VkResult imguiPoolResult = vkCreateDescriptorPool(state->device, &imguiPoolInfo, nullptr, &state->imguiDescriptorPool);
        if (imguiPoolResult != VK_SUCCESS)
        {
            LOG_ERROR("vkCreateDescriptorPool for ImGui failed: %s", VkResultName(imguiPoolResult));
            return;
        }
    
        if (!ImGui_ImplVulkan_LoadFunctions(ImGuiVulkanLoadFunction, state))
        {
            LOG_ERROR("ImGui_ImplVulkan_LoadFunctions failed");
            return;
        }
    
        state->imguiColorAttachmentFormat = state->swapchainFormat;
        ImGui_ImplVulkan_InitInfo imguiInit = {};
        imguiInit.Instance = state->instance;
        imguiInit.PhysicalDevice = state->physicalDevice;
        imguiInit.Device = state->device;
        imguiInit.QueueFamily = state->graphicsQueueFamily;
        imguiInit.Queue = state->graphicsQueue;
        imguiInit.DescriptorPool = state->imguiDescriptorPool;
        imguiInit.MinImageCount = 2;
        imguiInit.ImageCount = DynArrayGetSize(state->swapchainImages);
        imguiInit.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        imguiInit.UseDynamicRendering = true;
        imguiInit.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        imguiInit.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        imguiInit.PipelineRenderingCreateInfo.pColorAttachmentFormats = &state->imguiColorAttachmentFormat;
    
        if (!ImGui_ImplVulkan_Init(&imguiInit))
        {
            LOG_ERROR("ImGui_ImplVulkan_Init failed");
            return;
        }
    } // END IMGUI
    
}

void NvrhiVulkanRendererBackend::Teardown(EngineContext*)
{
    ME_PROFILE_FUNCTION();

    if (!state)
    {
        return;
    }

    if (state->nvrhiDevice)
    {
        state->nvrhiDevice->waitForIdle();
    }

    ImGui_ImplVulkan_Shutdown();

    state->commandList = nullptr;
    ReleaseSwapchainTextures(*state);
    ReleaseTextures(*state);
    ReleaseBuffers(*state);
    DynArrayDestroy(state->swapchainTextures);
    DynArrayDestroy(state->swapchainImages);
    DynArrayDestroy(state->textures);
    DynArrayDestroy(state->buffers);
    state->nvrhiDevice = nullptr;

    if (state->imageAcquireFence)
    {
        vkDestroyFence(state->device, state->imageAcquireFence, nullptr);
    }
    if (state->swapchain)
    {
        vkDestroySwapchainKHR(state->device, state->swapchain, nullptr);
    }
    if (state->imguiDescriptorPool)
    {
        vkDestroyDescriptorPool(state->device, state->imguiDescriptorPool, nullptr);
    }
    if (state->surface)
    {
        vkDestroySurfaceKHR(state->instance, state->surface, nullptr);
    }
    if (state->device)
    {
        vkDestroyDevice(state->device, nullptr);
    }
    if (state->debugMessenger)
    {
        vkDestroyDebugUtilsMessengerEXT(state->instance, state->debugMessenger, nullptr);
    }
    if (state->instance)
    {
        vkDestroyInstance(state->instance, nullptr);
    }

    delete state;
    state = nullptr;
}

void* NvrhiVulkanRendererBackend::RenderScene(RenderInput* input)
{
    ME_PROFILE_FUNCTION();

    if (!state || !state->nvrhiDevice || !state->commandList)
    {
        return nullptr;
    }
    state->currentImageIndex = U32_INVALID_ID;

    const u32 windowWidth = input->osData.windowWidth;
    const u32 windowHeight = input->osData.windowHeight;
    if (windowWidth != state->width || windowHeight != state->height)
    {
        CreateSwapchain(*state, windowWidth, windowHeight);
    }

    u32 imageIndex = 0;
    vkResetFences(state->device, 1, &state->imageAcquireFence);
    VkResult acquireResult = vkAcquireNextImageKHR(
        state->device,
        state->swapchain,
        UINT64_MAX,
        VK_NULL_HANDLE,
        state->imageAcquireFence,
        &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR)
    {
        CreateSwapchain(*state, windowWidth, windowHeight);
        return nullptr;
    }
    if (acquireResult != VK_SUCCESS)
    {
        LOG_ERROR("vkAcquireNextImageKHR failed: %s", VkResultName(acquireResult));
        return nullptr;
    }

    vkWaitForFences(state->device, 1, &state->imageAcquireFence, VK_TRUE, UINT64_MAX);

    nvrhi::ITexture* swapchainImage = state->swapchainTextures[imageIndex];
    state->currentImageIndex = imageIndex;

    state->commandList->open();
    state->commandList->clearTextureFloat(swapchainImage, nvrhi::AllSubresources, nvrhi::Color(0.10f, 0.08f, 0.14f, 1.0f));
    state->commandList->commitBarriers();
    // TODO: actually render the meshes and use shaders and all that....
    return nullptr;
}

void NvrhiVulkanRendererBackend::PresentFrame()
{
    ME_ON_SCOPE_EXIT([this]()
    {
        ArenaClear(&rendererFrameArena);
    });
    if (!state || state->currentImageIndex >= DynArrayGetSize(state->swapchainTextures))
    {
        return;
    }

    nvrhi::ITexture* swapchainImage = state->swapchainTextures[state->currentImageIndex];
    nvrhi::Object cmdqueue = state->commandList->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer);
    VkCommandBuffer cmd = (VkCommandBuffer)cmdqueue.pointer;
    VkImageView imageView = (VkImageView)swapchainImage->getNativeView(
        nvrhi::ObjectTypes::VK_ImageView,
        nvrhi::Format::UNKNOWN,
        nvrhi::AllSubresources,
        nvrhi::TextureDimension::Texture2D).pointer;

    state->commandList->setTextureState(swapchainImage, nvrhi::AllSubresources, nvrhi::ResourceStates::RenderTarget);
    state->commandList->commitBarriers();

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = imageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.extent = state->swapchainExtent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);

    state->commandList->setTextureState(swapchainImage, nvrhi::AllSubresources, nvrhi::ResourceStates::Present);
    state->commandList->commitBarriers();
    state->commandList->close();

    state->nvrhiDevice->executeCommandList(state->commandList);
    // we have a "serialized" renderer rn for simplicity of the initial impl.
    // TODO: better pipelining of the renderer
    state->nvrhiDevice->waitForIdle();

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &state->swapchain;
    presentInfo.pImageIndices = &state->currentImageIndex;

    VkResult presentResult = vkQueuePresentKHR(state->graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
    {
        CreateSwapchain(*state, state->width, state->height);
    }
    else if (presentResult != VK_SUCCESS)
    {
        LOG_ERROR("vkQueuePresentKHR failed: %s", VkResultName(presentResult));
    }
}

u64 NvrhiVulkanRendererBackend::CreateVertexBuffer(meSpan bufferMem, meMeshVertexLayoutType layout)
{
    if (!state || !state->nvrhiDevice || !bufferMem)
    {
        return U64_INVALID_ID;
    }

    nvrhi::BufferDesc desc;
    desc.byteSize = bufferMem.size;
    desc.debugName = "MindseyeVertexBuffer";
    desc.canHaveUAVs = true;
    desc.canHaveRawViews = true;
    desc.isVertexBuffer = !TEST_BIT(layout, meMeshVertexLayoutType_Index16) && !TEST_BIT(layout, meMeshVertexLayoutType_Index32);
    desc.isIndexBuffer = !desc.isVertexBuffer;
    desc.initialState = nvrhi::ResourceStates::Common;
    desc.keepInitialState = true;

    nvrhi::BufferHandle buffer = state->nvrhiDevice->createBuffer(desc);
    if (!buffer)
    {
        return U64_INVALID_ID;
    }

    state->commandList->open();
    state->commandList->writeBuffer(buffer, bufferMem.data, bufferMem.size);
    state->commandList->setPermanentBufferState(buffer, nvrhi::ResourceStates::ShaderResource);
    state->commandList->close();
    state->nvrhiDevice->executeCommandList(state->commandList);

    if (DynArrayGetSize(state->buffers) >= DynArrayGetCapacity(state->buffers))
    {
        LOG_ERROR("NVRHI buffer handle table is full");
        return U64_INVALID_ID;
    }

    NvrhiGpuBuffer gpuBuffer = { buffer.Detach(), layout };
    DynArrayPush(state->buffers, gpuBuffer);
    return DynArrayGetSize(state->buffers) - 1;
}

u64 NvrhiVulkanRendererBackend::UploadTextureToGPU(meSpan textureMem, u32 channels, u32 width, u32 height)
{
    if (!state || !state->nvrhiDevice || !textureMem)
    {
        return U64_INVALID_ID;
    }
    if (channels != 4)
    {
        LOG_WARN("NVRHI backend currently expects RGBA8 textures; got %u channels", channels);
        return U64_INVALID_ID;
    }

    nvrhi::TextureDesc textureDesc;
    textureDesc.width = width;
    textureDesc.height = height;
    textureDesc.format = nvrhi::Format::RGBA8_UNORM;
    textureDesc.debugName = "MindseyeTexture";
    textureDesc.initialState = nvrhi::ResourceStates::Common;
    textureDesc.keepInitialState = true;

    nvrhi::TextureHandle texture = state->nvrhiDevice->createTexture(textureDesc);
    if (!texture)
    {
        return U64_INVALID_ID;
    }

    state->commandList->open();
    state->commandList->writeTexture(texture, 0, 0, textureMem.data, width * channels);
    state->commandList->setPermanentTextureState(texture, nvrhi::ResourceStates::ShaderResource);
    state->commandList->close();
    state->nvrhiDevice->executeCommandList(state->commandList);

    nvrhi::SamplerDesc samplerDesc;
    samplerDesc.setAllFilters(true);
    nvrhi::SamplerHandle sampler = state->nvrhiDevice->createSampler(samplerDesc);

    if (DynArrayGetSize(state->textures) >= DynArrayGetCapacity(state->textures))
    {
        LOG_ERROR("NVRHI texture handle table is full");
        return U64_INVALID_ID;
    }

    NvrhiGpuTexture gpuTexture = { texture.Detach(), sampler.Detach() };
    DynArrayPush(state->textures, gpuTexture);
    return DynArrayGetSize(state->textures) - 1;
}

void NvrhiVulkanRendererBackend::DestroyGPUTexture(u64 textureHandle)
{
    if (!state || textureHandle >= DynArrayGetSize(state->textures))
    {
        return;
    }

    NvrhiGpuTexture& texture = state->textures[textureHandle];
    if (texture.sampler)
    {
        texture.sampler->Release();
        texture.sampler = nullptr;
    }
    if (texture.texture)
    {
        texture.texture->Release();
        texture.texture = nullptr;
    }
}

void NvrhiVulkanRendererBackend::BeginImguiContext()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
}

void NvrhiVulkanRendererBackend::EndImguiContext()
{
    ImGui::Render();
}

