#include <imgui.h>
#include <imgui_stdlib.h>

#include "core/translations.hpp"
#include "editor/game_scenes.hpp"
#include "editor/scripts/script_files.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

// The button was pressed, or Enter on a name that can be used.
bool asked_for_name(bool pressed, bool entered, bool valid) { return pressed || (entered && valid); }

// Something was typed that cannot be used.
bool breaks_the_rule(const std::string& name, bool valid) { return !name.empty() && !valid; }

}  // namespace

bool draw_name_form(const char* id, const char* hint, const char* button_label, const NameRule& rule,
                    std::string& name) {
  const bool entered = ImGui::InputTextWithHint(id, hint, &name, ImGuiInputTextFlags_EnterReturnsTrue);
  const bool valid = rule.usable(name);
  ImGui::SameLine();
  ImGui::BeginDisabled(!valid);
  const bool asked = asked_for_name(ImGui::Button(button_label), entered, valid);
  ImGui::EndDisabled();
  if (breaks_the_rule(name, valid)) ImGui::TextColored(muted_color(), "%s", tr(rule.rule));
  return asked;
}

bool draw_new_script_form(const char* id, const char* hint, const char* button_label, std::string& name) {
  return draw_name_form(id, hint, button_label, {valid_script_name, "editor.script.name_rule"}, name);
}

bool draw_new_scene_form(const char* id, const char* hint, const char* button_label, std::string& name) {
  return draw_name_form(id, hint, button_label, {valid_scene_name, "editor.scenes.name_rule"}, name);
}

}  // namespace redgem::ui
