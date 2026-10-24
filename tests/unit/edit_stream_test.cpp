#include <doctest/doctest.h>

#include <filesystem>

#include "editor/edit_stream.hpp"
#include "editor/document/scene_document.hpp"
#include "editor/ui/panel_state.hpp"

using namespace redgem;

namespace {

constexpr int kDragFrames = 5;
constexpr float kStep = 0.5f;

// A document with two objects and no undo steps yet.
struct TwoObjects {
  SceneDocument doc = SceneDocument::create_empty(std::filesystem::temp_directory_path() / "redgem_edit_stream.glb");
  int first = doc.add_empty("First");
  int second = doc.add_empty("Second");
  int steps_before = undo_steps(doc);

  // How many times the document can be undone (it is left as it was).
  static int undo_steps(SceneDocument& doc) {
    int steps = 0;
    while (doc.undo()) ++steps;
    for (int i = 0; i < steps; ++i) doc.redo();
    return steps;
  }
  int new_steps() { return undo_steps(doc) - steps_before; }

  // One frame of a drag on an object: the widget is held and moves it a little.
  void drag(EditStream& stream, int object) {
    Transform local = doc.node(object).local;
    local.position.x += kStep;
    stream.before_change(doc, object);
    doc.set_local(object, local, false);
    stream.end_frame(true);
  }
};

}  // namespace

TEST_CASE("a drag is one undo step, however many frames it takes") {
  TwoObjects scene;
  EditStream stream;
  for (int i = 0; i < kDragFrames; ++i) scene.drag(stream, scene.first);
  CHECK(stream.open());
  stream.end_frame(false);  // the widget was let go
  CHECK_FALSE(stream.open());
  CHECK(scene.new_steps() == 1);
  REQUIRE(scene.doc.undo());
  CHECK(scene.doc.node(scene.first).local.position.x == doctest::Approx(0.0f));
}

TEST_CASE("a drag that ends where it started leaves no undo step") {
  TwoObjects scene;
  EditStream stream;
  stream.before_change(scene.doc, scene.first);
  scene.doc.set_local(scene.first, scene.doc.node(scene.first).local, false);
  stream.end_frame(false);
  CHECK(scene.new_steps() == 0);
}

TEST_CASE("a drag that goes on after the selection moved is a step for each object") {
  TwoObjects scene;
  EditStream stream;
  scene.drag(stream, scene.first);
  scene.drag(stream, scene.second);  // the widget is still held: it now edits another object
  stream.end_frame(false);
  CHECK(scene.new_steps() == 2);
  // Undoing once takes back the second object's change alone.
  REQUIRE(scene.doc.undo());
  CHECK(scene.doc.node(scene.second).local.position.x == doctest::Approx(0.0f));
  CHECK(scene.doc.node(scene.first).local.position.x == doctest::Approx(kStep));
}

TEST_CASE("a drag let go with nothing selected is over: the next one is a step of its own") {
  TwoObjects scene;
  EditStream stream;
  scene.drag(stream, scene.first);
  // The selection went away mid-drag and the widget with it: frames go by with nothing to draw.
  stream.end_frame(false);
  stream.end_frame(false);
  CHECK_FALSE(stream.open());
  scene.drag(stream, scene.second);
  stream.end_frame(false);
  CHECK(scene.new_steps() == 2);
}

TEST_CASE("every editor starts with panels that remember nothing, and a console that lists everything") {
  ui::PanelState state;
  CHECK(state.hierarchy.filter.empty());
  CHECK(state.project.folder == ui::ProjectFolder::Scripts);
  CHECK(state.script_tabs.pending_close.empty());
  CHECK_FALSE(state.inspector.edits.open());
  for (const log::Level level : {log::Level::Debug, log::Level::Info, log::Level::Warning, log::Level::Error}) {
    CHECK(state.console.filters.shows(level));
  }
  state.console.filters.errors = false;
  CHECK_FALSE(state.console.filters.shows(log::Level::Error));
  CHECK(state.console.filters.shows(log::Level::Warning));
  state.console.filters.info = false;
  CHECK_FALSE(state.console.filters.shows(log::Level::Debug));  // listed with the information
  // Another editor's is its own.
  CHECK(ui::PanelState().console.filters.shows(log::Level::Error));
}

TEST_CASE("opening another scene ends what the panels held of the last one, without touching the new one") {
  constexpr unsigned int kNameField = 7;  // a widget id
  TwoObjects old_scene;
  TwoObjects new_scene;
  ui::PanelState state;
  old_scene.drag(state.inspector.edits, old_scene.first);
  REQUIRE(state.inspector.edits.open());
  ui::TextFieldState& typing = state.inspector.text_fields[kNameField];
  typing.object = old_scene.second;
  typing.editing = true;
  typing.buffer = "Half typ";
  state.hierarchy.filter = "Fir";
  state.project.new_script_name = "enemy";

  ui::forget_scene(state);
  CHECK_FALSE(state.inspector.edits.open());
  CHECK(state.inspector.text_fields.empty());
  CHECK(state.hierarchy.filter.empty());
  CHECK(state.project.new_script_name == "enemy");  // the game's, not the scene's

  // A drag in the new scene is a step of its own, in its own history.
  new_scene.drag(state.inspector.edits, new_scene.first);
  state.inspector.edits.end_frame(false);
  CHECK(new_scene.new_steps() == 1);
  CHECK(old_scene.new_steps() == 1);  // the old scene's drag, left as it was
}
