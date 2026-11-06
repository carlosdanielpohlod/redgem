#include "editor/ui/panels.hpp"

#include <imgui.h>
#include "core/log.hpp"
#include "core/translations.hpp"
#include "editor/prefab_files.hpp"
#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {
constexpr const char* kPopup = "save_prefab";
constexpr const char* kName = "##prefab_name";
constexpr float kNameWidth = 180.0f;  // pixels available for the new file's stem

void finish_export(PanelContext& context, std::string& name) {
  if (!save_prefab(context, name)) return;
  name.clear();
  ImGui::CloseCurrentPopup();
}
void prefab_popup(PanelContext& context) {
  if (!ImGui::BeginPopup(kPopup)) return;
  auto& name = context.state.project.new_prefab_name;
  ImGui::TextUnformatted(tr("editor.prefab.save_selection"));
  ImGui::SetNextItemWidth(kNameWidth);
  if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
  const NameRule rule{prefab_files::valid_name, "editor.prefab.name_rule"};
  if (draw_name_form(kName, tr("editor.prefab.name_hint"), tr("editor.prefab.save"), rule, name)) {
    finish_export(context, name);
  }
  ImGui::EndPopup();
}
}  // namespace
bool save_prefab(PanelContext& context, const std::string& name) {
  std::string error;
  if (!context.session.export_prefab(name, error)) {
    log::error("cannot save prefab: " + error);
    return false;
  }
  context.files_changed = true;
  log::info("saved prefab " + name);
  return true;
}
void draw_prefab_tools(PanelContext& context) {
  if (context.state.project.folder != ProjectFolder::Prefabs) return;
  ImGui::BeginDisabled(!context.session.can_export_prefab());
  if (ImGui::SmallButton(icon_label(ICON_FA_FLOPPY_DISK, "editor.prefab.save_selection"))) ImGui::OpenPopup(kPopup);
  ImGui::EndDisabled();
  prefab_popup(context);
  ImGui::SameLine();
}
}  // namespace redgem::ui
