#include "editor/ui/interface.hpp"

#include <imgui.h>
#include <rlImGui.h>

#include "editor/ui/style.hpp"

namespace redgem::ui {

Interface::Interface(const Window&, Layout layout) {
  rlImGuiSetLoadFontsCallback(load_fonts);
  rlImGuiSetup(true);
  apply_theme();
  ImGuiIO& io = ImGui::GetIO();
  if (layout == Layout::Dockable) io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = nullptr;  // nothing is remembered between runs: the layout is rebuilt on every start
}

Interface::~Interface() { rlImGuiShutdown(); }

void Interface::begin_frame() { rlImGuiBegin(); }

void Interface::end_frame() { rlImGuiEnd(); }

}  // namespace redgem::ui
