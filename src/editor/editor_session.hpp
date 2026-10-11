#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>

#include "app/engine.hpp"
#include "editor/scene_document.hpp"
#include "scene/terrain.hpp"
#include "input/input.hpp"
#include "scene/scene.hpp"

namespace redgem {

enum class PlayState { Editing, Playing, Paused };

// Everything the editor does, minus the UI: the document, selection, preview scene and play mode.
// Play runs the edited scene in a headless Engine; Stop throws that runtime away, so the document
// is exactly as it was before Play.
class EditorSession {
 public:
  static std::variant<std::unique_ptr<EditorSession>, std::string> open(const std::filesystem::path& game_dir);

  SceneDocument& document() { return document_; }
  const std::filesystem::path& game_dir() const { return game_dir_; }

  // The scene to draw: the live runtime while playing, otherwise the edited scene.
  const Scene& scene() const;
  const SceneDesc& scene_desc() const { return document_.desc(); }
  // Call after every document edit so the preview reflects it.
  void refresh_preview();

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
  bool save(std::string& error);

  PlayState play_state() const { return play_state_; }
  // `input` feeds the game while playing (the editor gates it by viewport focus).
  // `audio` may be null (the game then records sounds instead of playing them).
  bool play(const InputSource& input, std::string& error, AudioSystem* audio = nullptr);
  void pause(bool paused);
  void stop();
  // Advances the running game; does nothing while editing or paused.
  void tick(float frame_seconds);
  Engine* runtime() { return runtime_.get(); }

  // Reloads scene.glb when it changed on disk (e.g. re-exported from Blender) and there are no
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
  // The first object carrying the environment's properties, or kNoObject.
  ObjectId find_environment() const;
  // The selection after an edit that renumbers the objects: the one called `name`.
  void refresh_and_reselect(const std::string& name);
  ObjectId place_new(int node, Vec3 position);
  // Name of the selected object ("" without one): how the selection is found again after an edit
  // that renumbers the objects.
  std::string selected_name() const;

  std::filesystem::path game_dir_;
  SceneDocument document_;
  Scene preview_;
  ObjectId selection_ = kNoObject;
  PlayState play_state_ = PlayState::Editing;
  std::unique_ptr<Engine> runtime_;
  std::filesystem::file_time_type loaded_write_time_{};
  bool external_change_pending_ = false;
};

}  // namespace redgem
