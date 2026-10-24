#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "editor/document/scene_document.hpp"
#include "scene/shapes/terrain.hpp"
#include "input/input.hpp"
#include "scene/scene.hpp"

namespace redgem {

enum class PlayState { Editing, Playing, Paused };

// What a game played from the editor is played with. Everything but the input may be missing:
// the game is then silent, or shown by nobody.
struct PlayDevices {
  InputSource& input;              // feeds the game (the editor gates it by viewport focus)
  AudioSystem* audio = nullptr;    // what the game sounds through
  Presenter* presenter = nullptr;  // who shows the game (the editor's Scene view)
};

// Everything the editor does, minus the UI: the document, selection, preview scene and play mode.
// Play runs a copy of the edited scene in an Engine; Stop throws that runtime away, so the document
// is exactly as it was before Play.
class EditorSession {
 public:
  // The game's scene.glb, the scene it starts with, open for editing.
  static std::variant<std::unique_ptr<EditorSession>, std::string> open(const std::filesystem::path& game_dir);

  SceneDocument& document() { return document_; }
  const std::filesystem::path& game_dir() const { return game_dir_; }
  // The scene being edited, by the name scripts load it with ("main" is scene.glb, see
  // editor/game_scenes.hpp).
  const std::string& scene_name() const { return scene_name_; }
  // Opens another scene of the game in place of this one, with its own undo history; its unsaved
  // edits are dropped, so ask first. False, and why in `error`, when it cannot be read, or while
  // the game runs.
  bool open_scene(const std::string& name, std::string& error);

  // The scene to draw: the live runtime while playing, otherwise the edited scene.
  const Scene& scene() const;
  const SceneDesc& scene_desc() const { return document_.desc(); }
  // After every document edit, one of these so the preview reflects it: refresh_preview makes it
  // all again (anything that adds, removes or moves nodes in the hierarchy, undo, redo, a changed
  // prop); refresh_node makes one node's object again, for an edit to that node only (its
  // placement, light or camera), which is what a drag repeats every frame.
  void refresh_preview();
  void refresh_node(ObjectId node);

  ObjectId selection() const { return selection_; }
  void select(ObjectId id);
  void select_by_name(const std::string& name);

  // Editing commands (ignored while playing). They keep the selection meaningful.
  void duplicate_selection();
  void delete_selection();

  // Creating objects at a world position (usually in front of the editor camera). The new object
  // becomes the selection. Return kNoObject while playing.
  ObjectId create_empty(Vec3 position);
  ObjectId create_primitive(PrimitiveShape shape, Vec3 position);
  ObjectId create_light(LightType type, Vec3 position);
  ObjectId create_camera(Vec3 position);
  // Flat ground to sculpt, centered under `position`.
  ObjectId create_terrain(Vec3 position);
  bool is_terrain(ObjectId id) const;
  // Where a ray in the world meets a terrain's ground (in the world).
  std::optional<Vec3> terrain_hit(ObjectId terrain, Vec3 origin, Vec3 direction) const;
  // One brush stroke on a terrain around a point of the world: `radius` in meters, `amount` in
  // meters (Raise, Lower) or as a 0..1 share (Smooth, Flatten). Call document().checkpoint() once
  // when a stroke begins. False when nothing changed or while playing.
  bool sculpt_terrain(ObjectId terrain, Vec3 world_point, float radius, float amount, SculptMode mode);
  // An object whose properties set the scene's sky, fog and effects (see render/environment.hpp),
  // filled with a sunset to start from. Selects the existing one if the scene already has it.
  ObjectId create_environment();
  // Adds a .glb model (e.g. from models/) to the scene at a world position, optionally under
  // another object. One undo step.
  ObjectId import_model(const std::filesystem::path& path, Vec3 position, std::string& error,
                        ObjectId parent = kNoObject);
  // Moves `child` under `parent` (kNoObject = top level), keeping where it is in the world.
  void reparent(ObjectId child, ObjectId parent);
  // Adds a script file (scripts/<file>) to the object's script list.
  void attach_script(ObjectId id, const std::string& file);
  void undo();
  void redo();
  // Writes the scene being edited back to its file.
  bool save(std::string& error);
  // Writes it as a new scene of the game (scenes/<name>.glb), which is then the one edited.
  // Refused when the scene uses files next to it (images, buffers) and would move to another
  // folder, where it would not find them.
  bool save_as(const std::string& name, std::string& error);

  PlayState play_state() const { return play_state_; }
  // No game runs: the scene can be edited.
  bool editing() const { return play_state_ == PlayState::Editing; }
  // Starts the game on the scene as it is edited now. What `devices` names has to outlive the play.
  bool play(const PlayDevices& devices, std::string& error);
  void pause(bool paused);
  void stop();
  // Advances the running game; does nothing while editing or paused.
  void tick(float frame_seconds);
  Engine* runtime() { return runtime_.get(); }

  // Reloads the edited scene's file when it changed on disk (e.g. re-exported from Blender) and there are no
  // unsaved edits. With unsaved edits the change stays pending for the user to decide.
  // Returns true when it reloaded.
  bool reload_if_changed_on_disk();
  bool external_change_pending() const { return external_change_pending_; }
  // Throws away the edits and loads the file from disk.
  bool reload_from_disk();
  // Keeps the edits; the next save overwrites the outside change.
  void keep_my_edits() { external_change_pending_ = false; }

 private:
  EditorSession(std::filesystem::path game_dir, SceneDocument document);
  // The document just read from its file is edited from now on: nothing selected, and the file
  // followed for changes others make to it.
  void begin_editing();
  // Whatever is saved last is what the file on disk holds.
  void note_saved();
  // Whether the scene can be saved as `file` and still find the files it uses (see save_as).
  bool files_found_from(const std::filesystem::path& file, std::string& error) const;
  // Whether an object can be edited now: the game is stopped, and there is one.
  bool can_edit(ObjectId id) const { return editing() && id != kNoObject; }
  bool can_sculpt(ObjectId terrain) const { return editing() && is_terrain(terrain); }
  // A game runs and is not paused.
  bool playing() const { return play_state_ == PlayState::Playing && runtime_ != nullptr; }
  // The file on disk is another than the one loaded, and no game runs that reads the loaded one.
  bool reload_wanted(std::filesystem::file_time_type write_time) const {
    return write_time != loaded_write_time_ && editing();
  }
  // The first object carrying the environment's properties, or kNoObject.
  ObjectId find_environment() const;
  // The selection after an edit that renumbers the objects: the one called `name`.
  void refresh_and_reselect(const std::string& name);
  ObjectId place_new(int node, Vec3 position);
  // What every creation does: `add` makes the object (one undo step) and it is placed and
  // selected, while the game is stopped (kNoObject while it runs).
  ObjectId place_created(const std::function<int()>& add, Vec3 position);
  int add_light(LightType type);
  // Name of the selected object ("" without one): how the selection is found again after an edit
  // that renumbers the objects.
  std::string selected_name() const;

  std::filesystem::path game_dir_;
  SceneDocument document_;
  std::string scene_name_;  // of the document's file, kept as it opens or is saved as another
  Scene preview_;
  ObjectId selection_ = kNoObject;
  PlayState play_state_ = PlayState::Editing;
  std::unique_ptr<Engine> runtime_;
  AudioSystem* game_audio_ = nullptr;  // what the running game sounds through, when it has something
  std::filesystem::file_time_type loaded_write_time_{};
  bool external_change_pending_ = false;
};

}  // namespace redgem
