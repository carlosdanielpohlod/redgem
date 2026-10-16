// Inspector cards of the scripts attached to an object, and the menu that attaches one.

#include <algorithm>

#include "core/log.hpp"
#include "core/translations.hpp"
#include "editor/editor_logic.hpp"
#include "editor/inspector_logic.hpp"
#include "editor/scripts/script_files.hpp"
#include "editor/ui/inspector_cards.hpp"
#include "editor/ui/style.hpp"
#include "scene/props.hpp"
#include "script/script_naming.hpp"

namespace redgem::ui::inspector {
namespace {

constexpr float kPopupFieldWidth = 160.0f;  // pixels: the name field inside the menu
constexpr const char* kNewScriptId = "##new_script";

// The button that makes a new script: its icon, then its text (made once: see editor/ui/labels.hpp).
const char* new_script_label() {
  static const std::string label = std::string(ICON_FA_PLUS " ") + tr("editor.inspector.script.new");
  return label.c_str();
}

bool listed(const std::vector<std::string>& scripts, const std::string& script) {
  return std::find(scripts.begin(), scripts.end(), script) != scripts.end();
}

void write_scripts(EditorSession& session, int index, const std::vector<std::string>& scripts) {
  set_or_remove(session, index, props::kScript, inspector_logic::text_or_nothing(join_script_list(scripts)));
}

// The card of one attached script: a button that opens it, or a warning when its file is gone.
void draw_script_card(PanelContext& context, const std::string& script) {
  property_label(tr("editor.inspector.script.label"));
  if (!listed(context.files.scripts, script)) {
    const std::string missing = tr("editor.inspector.script.missing", {{"script", script}});
    ImGui::TextColored(error_color(), ICON_FA_TRIANGLE_EXCLAMATION "  %s", missing.c_str());
    return;
  }
  if (ImGui::Button((std::string(ICON_FA_PEN_TO_SQUARE "  ") + script).c_str())) open_script(context.scripts, script);
  tooltip(tr("editor.inspector.script.edit_tooltip"));
}

// One attached script as a card of its own. True when its close button was pressed.
bool draw_script(PanelContext& context, const std::string& script) {
  bool removed = false;
  const std::string title = class_name_for_script(script);
  const bool open = component_header(ICON_FA_GEM, title.c_str(), &removed);
  if (removed) return true;
  if (open) draw_script_card(context, script);
  return false;
}

// Makes scripts/<name>.rb, attaches it to the object and opens it in a tab.
void create_and_attach_script(PanelContext& context, int index, std::string& name) {
  std::string error;
  const auto file = create_script(context.scripts.scripts_dir(), name, error);
  if (!file) {
    log::error(error);
    return;
  }
  context.session.attach_script(index, *file);
  open_script(context.scripts, *file);
  context.files_changed = true;
  name.clear();
  log::info("created " + *file);
  ImGui::CloseCurrentPopup();
}

// The game's scripts the object does not have yet.
void draw_attachable_scripts(PanelContext& context, int index, const std::vector<std::string>& attached) {
  for (const std::string& script : context.files.scripts) {
    if (listed(attached, script)) continue;
    if (ImGui::MenuItem((std::string(ICON_FA_GEM "  ") + script).c_str())) context.session.attach_script(index, script);
  }
}

}  // namespace

void draw_scripts(PanelContext& context, int index) {
  const std::vector<std::string> scripts = scripts_of(context.session, index);
  for (std::size_t i = 0; i < scripts.size(); ++i) {
    ImGui::PushID(static_cast<int>(i));
    const bool removed = draw_script(context, scripts[i]);
    ImGui::PopID();
    if (!removed) continue;
    write_scripts(context.session, index, editor_logic::without_item(scripts, i));
    return;  // the cards after it are drawn from the new list on the next frame
  }
}

void add_script_menu(PanelContext& context, int index) {
  const std::vector<std::string> attached = scripts_of(context.session, index);
  std::string& new_name = context.state.inspector.new_script_name;
  ImGui::SetNextItemWidth(kPopupFieldWidth);
  if (draw_new_script_form(kNewScriptId, tr("editor.inspector.script.name_hint"), new_script_label(), new_name)) {
    create_and_attach_script(context, index, new_name);
  }
  if (!context.files.scripts.empty()) ImGui::Separator();
  draw_attachable_scripts(context, index, attached);
}

}  // namespace redgem::ui::inspector
