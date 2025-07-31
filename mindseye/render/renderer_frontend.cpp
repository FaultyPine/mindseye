#include "renderer_frontend.h"


#include "vulkan_backend/vulkan_backend.h"


void RendererInitialize(EngineContext* engine)
{
    VulkanBackendInitialize(engine);
}

void RendererTeardown()
{
    VulkanBackendTeardown();
}