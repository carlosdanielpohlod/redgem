#pragma once

#include <mruby.h>

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "net/network_session.hpp"
#include "physics/physics_world.hpp"
#include "script/ruby_vm.hpp"
#include "script/script_context.hpp"

namespace redgem {

// Runs the Ruby side of the game: loads scripts, instantiates behaviours and calls their callbacks.
class ScriptHost {
 public:
  ScriptHost(ScriptContext& context, std::filesystem::path scripts_dir);
  ScriptHost(const ScriptHost&) = delete;
  ScriptHost& operator=(const ScriptHost&) = delete;

  // Instantiates the behaviours declared by the `script` property of every alive object.
  void attach_scripts();
  // Same, for one object (e.g. one just spawned).
  void attach_object(ObjectId id);
  // Adds a behaviour of a Ruby class to an object from code (GameObject#add_component). Returns
  // the instance, or nil after reporting why it could not be created.
  mrb_value add_behaviour(ObjectId object, mrb_value klass);
  // Loads the script a class should be defined in ("CameraFollow" -> camera_follow.rb), if the
  // game has it. Returns whether the class exists afterwards.
  bool require_class(const std::string& class_name);
  // Runs the scene's setup script (scripts/<scene>_scene.rb), if the game has one.
  void run_scene_script(const std::string& scene_name);
  // Forgets every behaviour and object wrapper before a new scene is loaded (Ruby state survives).
  void reset_scene();
  // Calls start on behaviours that did not start yet (before their first update).
  void start_pending();
  void fixed_update(float dt);
  void update(float dt);
  void dispatch(const ContactEvent& event);
  // Hands something that happened on the network to the scripts listening for it (Network.on...).
  void dispatch(const net::NetworkEvent& event);
  // Tells Ruby's own bookkeeping (Network handlers...) that the scene's objects are gone.
  void scene_was_reset();
  // Runs on_destroy on the object's behaviours (they stay attached until detach()).
  void on_destroyed(ObjectId object);
  // Disables the object's behaviours and removes them from its GameObject.
  void detach(ObjectId object);

  RubyResult eval(const std::string& code, const std::string& file_name);
  RubyResult load_file(const std::filesystem::path& path);

  // Cached Ruby GameObject wrapper for a scene object (stable identity).
  mrb_value object_value(ObjectId object);

  ScriptContext& context() { return context_; }
  RubyVm& vm() { return vm_; }
  int error_count() const { return error_count_; }
  std::size_t behaviour_count() const { return behaviours_.size(); }

  // Logs a Ruby error and counts it (test runs fail when the count is not zero).
  void report_error(const std::string& where, const std::string& error);

 private:
  struct BehaviourSlot {
    ObjectId object = kNoObject;
    mrb_value instance = mrb_nil_value();
    std::string class_name;
    bool started = false;
    bool enabled = true;
  };

  void load_prelude();
  void attach(ObjectId object, const std::string& script);
  mrb_value instantiate(ObjectId object, RClass* klass, const std::string& class_name);
  bool require_script(const std::string& script);
  // Whether a script's name stays inside the game's scripts folder (an error is reported when not).
  bool in_scripts_folder(const std::string& script);
  void invoke(std::size_t index, const char* method, std::initializer_list<mrb_value> args);
  void invoke_on_object(ObjectId object, const char* method, std::initializer_list<mrb_value> args);
  bool runnable(const BehaviourSlot& slot) const;
  // Starts an empty cache of GameObject wrappers, which Ruby's garbage collector leaves alone.
  void start_objects_cache();
  // Whether a Ruby class is Behaviour or inherits from it.
  bool is_behaviour_class(RClass* klass);
  // Reports why a behaviour could not be created; returns nil, which stands for it.
  mrb_value not_created(const std::string& where, const std::string& error);
  void disable_failed(std::size_t index, const char* method, const std::string& error);

  ScriptContext& context_;
  std::filesystem::path scripts_dir_;
  RubyVm vm_;
  mrb_value objects_cache_ = mrb_nil_value();
  std::vector<BehaviourSlot> behaviours_;
  std::set<std::string> loaded_scripts_;
  std::set<std::string> loading_scripts_;
  int error_count_ = 0;
};

ScriptHost& script_host(mrb_state* mrb);

}  // namespace redgem
