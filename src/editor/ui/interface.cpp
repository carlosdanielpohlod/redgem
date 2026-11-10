#include "editor/ui/interface.hpp"

#include <imgui.h>
#include <rlImGui.h>
#include <raylib.h>
#include <utility>

#include "editor/ui/style.hpp"
#include "editor/ui/dpi.hpp"

namespace redgem::ui {

Interface::Interface(const Window&, Layout layout, ScaleReader read_scale) : read_scale_(std::move(read_scale)) {
  rlImGuiSetLoadFontsCallback(load_fonts);
  rlImGuiSetup(true);
  ImGuiIO& io = ImGui::GetIO();
  if (layout == Layout::Dockable) io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename = nullptr;  // nothing is remembered between runs: the layout is rebuilt on every start
}

Interface::~Interface() { rlImGuiShutdown(); }

void Interface::begin_frame() {
  const auto asked = read_scale_ ? read_scale_() : GetWindowScaleDPI().y;
  const bool logical = !read_scale_ && native_dpi_window();
  const auto scale = interface_scale(asked, logical);
  if (scale != scale_) {
    scale_ = scale;
    apply_interface_scale(scale_);
  }
  rlImGuiBegin();
}

void Interface::end_frame() { rlImGuiEnd(); }

}  // namespace redgem::ui
