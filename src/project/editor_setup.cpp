#include "project/editor_setup.hpp"

#include <string>

#include "editor/ui/labels.hpp"
#include "editor/ui/style.hpp"
#include "project/game_build.hpp"

namespace redgem {
namespace {

// The systems a game can be built for, as the Build menus offer them.
struct BuildOption {
  BuildTarget target;
  const char* icon;
  const char* label;  // the key of its text
};
constexpr BuildOption kBuildOptions[] = {
    {BuildTarget::Linux, ICON_FA_DESKTOP, "project.build.for_linux"},
    {BuildTarget::Windows, ICON_FA_DESKTOP, "project.build.for_windows"},
    {BuildTarget::Android, ICON_FA_MOBILE_SCREEN, "project.build.for_android"},
    {BuildTarget::MacOS, ICON_FA_DESKTOP, "project.build.for_macos"},
};

// Whether the editor here offers a build for `target`: the Mac's player only comes with the
// Mac's editor, so only there is a game built for the Mac.
bool offered_here(BuildTarget target) { return target != BuildTarget::MacOS || target == host_build_target(); }

BuildChoice choice_for(const BuildOption& option) {
  const BuildTarget target = option.target;
  BuildChoice choice;
  choice.name = build_target_name(target);
  choice.label = ui::icon_label(option.icon, option.label);
  choice.for_this_system = target == host_build_target();
  choice.build = [target](const std::filesystem::path& game_dir, const std::filesystem::path& out_dir,
                          std::string& error) { return build_game_for(game_dir, out_dir, target, error); };
  return choice;
}

}  // namespace

std::vector<BuildChoice> build_choices() {
  std::vector<BuildChoice> choices;
  for (const BuildOption& option : kBuildOptions) {
    if (offered_here(option.target)) choices.push_back(choice_for(option));
  }
  return choices;
}

EditorOptions editor_options(GpuChoice gpu) {
  EditorOptions options;
  options.gpu = gpu;
  options.builds = build_choices();
  return options;
}

}  // namespace redgem
