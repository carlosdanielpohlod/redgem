#include "project/editor_window_tools.hpp"

#include <stdexcept>

#include "editor/editor_app.hpp"
#include "editor/object_clipboard.hpp"
#include "project/editor_live_tools.hpp"

namespace redgem::tools {
namespace {
constexpr int kDefaultCaptureSide = 512;

mcp::Json capture(EditorApp& app, const mcp::Json& args) {
  std::string error;
  const auto picture = app.capture_viewport(args.value("max_size", kDefaultCaptureSide), error);
  if (!picture) return mcp::tool_result(error, true);
  const auto encoded = object_clipboard::base64_encode(picture->bytes);
  return {{"content",
           {{{"type", "image"}, {"data", encoded}, {"mimeType", "image/png"}},
            {{"type", "text"},
             {"text", "viewport " + std::to_string(picture->width) + "x" + std::to_string(picture->height)}}}},
          {"isError", false}};
}

void play(EditorApp& app) {
  if (!app.session().editing()) throw std::invalid_argument("the game is already running");
  app.play();
  if (app.session().editing()) throw std::runtime_error("cannot start play; read editor events for details");
}

void paused_action(EditorSession& session, const std::string& action) {
  if (action == "pause") return session.pause(true);
  if (action == "resume") return session.pause(false);
  if (action != "step" || !session.step()) throw std::invalid_argument("step requires a paused game");
}

void advance(EditorApp& app, const std::string& action) {
  if (action == "play") return play(app);
  if (action == "stop") return app.stop();
  if (app.session().editing()) throw std::invalid_argument("start the game before controlling play");
  paused_action(app.session(), action);
}

void frame(EditorApp& app) {
  if (app.session().selected().ids().empty()) throw std::invalid_argument("frame needs an object, or a selection");
  app.frame_selection();
}
}  // namespace

mcp::Json editor_window_tool(EditorApp& app, const std::string& action, const mcp::Json& args) {
  if (action == "capture") return capture(app, args);
  if (action == "frame") {
    frame(app);
  } else {
    advance(app, action);
  }
  return invoke_editor(app.session(), "editor", {{"action", "status"}});
}
}  // namespace redgem::tools
