#define NVRHI_WITH_AFTERMATH 0
#define VK_USE_PLATFORM_WIN32_KHR
#define NOMINMAX
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1

#define VOLK_IMPLEMENTATION
#include "Volk/volk.h"

#include "nvrhi/src/common/format-info.cpp"
#include "nvrhi/src/common/misc.cpp"
#include "nvrhi/src/common/state-tracking.cpp"
#include "nvrhi/src/common/utils.cpp"
#include "nvrhi/src/common/aftermath.cpp"

#include "nvrhi/src/validation/validation-commandlist.cpp"
#include "nvrhi/src/validation/validation-device.cpp"

#include "nvrhi/src/vulkan/vulkan-allocator.cpp"
#include "nvrhi/src/vulkan/vulkan-buffer.cpp"
#include "nvrhi/src/vulkan/vulkan-commandlist.cpp"
#include "nvrhi/src/vulkan/vulkan-compute.cpp"
#include "nvrhi/src/vulkan/vulkan-constants.cpp"
#include "nvrhi/src/vulkan/vulkan-device.cpp"
#include "nvrhi/src/vulkan/vulkan-graphics.cpp"

#define VKViewportWithDXCoords VKViewportWithDXCoords_Meshlets
#include "nvrhi/src/vulkan/vulkan-meshlets.cpp"
#undef VKViewportWithDXCoords

#include "nvrhi/src/vulkan/vulkan-queries.cpp"
#include "nvrhi/src/vulkan/vulkan-queue.cpp"
#include "nvrhi/src/vulkan/vulkan-raytracing.cpp"
#include "nvrhi/src/vulkan/vulkan-resource-bindings.cpp"
#include "nvrhi/src/vulkan/vulkan-shader.cpp"
#include "nvrhi/src/vulkan/vulkan-staging-texture.cpp"
#include "nvrhi/src/vulkan/vulkan-state-tracking.cpp"
#include "nvrhi/src/vulkan/vulkan-texture.cpp"
#include "nvrhi/src/vulkan/vulkan-upload.cpp"
