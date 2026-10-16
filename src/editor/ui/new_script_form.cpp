#include <imgui.h>
#include <imgui_stdlib.h>

#include "editor/editor_config.hpp"
#include "editor/scripts/script_files.hpp"
#include "editor/ui/panels.hpp"
#include "editor/ui/style.hpp"

namespace redgem::ui {
namespace {

// The button was pressed, or Enter on a name a script can have.
bool asked_for_script(bool pressed, bool entered, bool valid) { return pressed || (entered && valid); }

// Something was typed that no script can be called.
bool breaks_the_rule(const std::string& name, bool valid) { return !name.empty() && !valid; }

}  // namespace

bool draw_new_script_form(const char* id, const char* hint, const char* button_label, std::string& name) {
  const bool entered = ImGui::InputTextWithHint(id, hint, &name, ImGuiInputTextFlags_EnterReturnsTrue);
  const bool valid = valid_script_name(name);
  ImGui::SameLine();
  ImGui::BeginDisabled(!valid);
  const bool asked = asked_for_script(ImGui::Button(button_label), entered, valid);
  ImGui::EndDisabled();
  if (breaks_the_rule(name, valid)) ImGui::TextColored(muted_color(), "%s", editor_config::kScriptNameRule);
  return asked;
}

}  // namespace redgem::ui
