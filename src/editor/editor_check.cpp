// Part of EditorApp: File > Check Game, which puts what `redgem check` finds in the Console.

#include "editor/editor_app.hpp"

#include "core/log.hpp"
#include "core/translations.hpp"
#include "editor/console_log.hpp"

namespace redgem {

bool EditorApp::check_game() {
  if (!check_ || !editing()) return false;
  if (unsaved_work()) log::warning(tr("editor.check.unsaved"));
  const GameCheck found = check_(session_->game_dir());
  for (const std::string& error : found.errors) log::error(check_message(error, session_->game_dir()));
  for (const std::string& warning : found.warnings) log::warning(warning);
  log::info(found.summary);
  return true;
}

}  // namespace redgem
