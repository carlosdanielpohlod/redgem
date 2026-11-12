#include "editor/editor_session.hpp"

#include "core/log.hpp"

namespace redgem {
void EditorSession::autosave(float seconds) {
  const bool allowed = editing() && !external_change_pending_;
  if (!allowed) return;
  std::string error;
  if (!recovery_.poll(document_, seconds, error)) log::warning("scene autosave: " + error);
}
bool EditorSession::recover_scene(std::string& error) {
  const bool allowed = editing() && recovery_.pending();
  if (!allowed) return false;
  if (document_.dirty()) {
    error = "save or discard current edits before recovering the scene";
    return false;
  }
  if (!recovery_.recover(document_, error)) return false;
  refresh_preview();
  selection_.clear();
  log::info("recovered unsaved scene; Save to replace the original");
  return true;
}
}  // namespace redgem
