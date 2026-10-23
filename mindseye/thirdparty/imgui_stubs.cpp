// Compatibility hooks used by bgfx's example ImGui integration.
namespace ImGuizmo {
void Create() {}
void Destroy() {}
void BeginFrame() {}
}
namespace ImGui {
void InitDockContext() {}
void ShutdownDockContext() {}
}
