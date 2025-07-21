#include "vulkan_backend.h"

#include "platform/me_os.h"

void VulkanBackendInitialize()
{
    void* vulkanDllHandle = LoadDynamicLibrary("vulkan-1.dll");
    ME_ASSERT(vulkanDllHandle != nullptr);
}

void VulkanBackendTeardown()
{
    
}