#pragma once

#include <mruby.h>

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "net/http_client.hpp"
#include "net/network_session.hpp"
#include "physics/physics_world.hpp"
#include "script/ruby_vm.hpp"
#include "script/script_context.hpp"

namespace redgem {

// Runs the Ruby side of the game: loads scripts, instantiates behaviours and calls their callbacks.
// Implemented in parts:
//   script_host.cpp            the interpreter, its prelude, the objects as Ruby sees them
//   script_host_scripts.cpp    loading the game's scripts and making behaviours of their classes
//   script_host_callbacks.cpp  calling the behaviours: start, update, contacts, destruction
//   script_host_network.cpp    handing network events to Ruby
//   script_host_http.cpp       handing the answers of web requests to Ruby
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
  // Hands the answer of a web request to the block waiting for it (HTTP).
  void dispatch(const net::HttpResult& result);
  // Tells Ruby's own bookkeeping (Network handlers, the blocks of HTTP requests) that the scene's
  // objects are gone.
  void scene_was_reset();
  // Runs on_destroy on the object's behaviours (they stay attached until detach()).
  void on_destroyed(ObjectId object);
  // Forgets the object's behaviours and its Ruby GameObject, which from then on stands for nothing.
  void detach(ObjectId object);

  RubyResult eval(const std::string& code, const std::string& file_name);
  RubyResult load_file(const std::filesystem::path& path);

  // Cached Ruby GameObject wrapper for a scene object (stable identity).
  mrb_value object_value(ObjectId object);

  ScriptContext& context() { return context_; }
  RubyVm& vm() { return vm_; }
  int error_count() const { return error_count_; }

  // Logs a Ruby error and counts it (error_count() says how many there were).
  void report_error(const std::string& where, const std::string& error);

 private:
  struct BehaviourSlot {
    ObjectId object = kNoObject;
    mrb_value instance = mrb_nil_value();
    std::string class_name;
    bool started = false;
    bool enabled = true;
    // Whether its class has a fixed_update of its own: Behaviour's does nothing, and calling it on
    // every behaviour every physics step would cost a protected Ruby call each for nothing.
    bool fixed_updates = true;

    // Whether its callbacks are still called: it started, and no error of its own disabled it.
    bool active() const { return started && enabled; }
  };

  void load_prelude();
  // A new Ruby GameObject for an object; stands_for says whether one still stands for it.
  mrb_value new_wrapper(const GameObject& object);
  bool stands_for(mrb_value wrapper, const GameObject& object);
  void set_variable(mrb_value object, const char* name, mrb_value value);
  // Starts an empty cache of GameObject wrappers (by id), which Ruby's garbage collector leaves alone.
  void start_objects_cache();

  // Scripts and the behaviours made of their classes.
  void attach(ObjectId object, const std::string& script);
  mrb_value instantiate(ObjectId object, RClass* klass, const std::string& class_name);
  bool require_script(const std::string& script);
  // Whether the game has a script of that name.
  bool has_script(const std::string& script) const;
  // Whether a script's name stays inside the game's scripts folder (an error is reported when not).
  bool in_scripts_folder(const std::string& script);
  // Whether a loaded script defined the class named after it (an error is reported, as one of
  // `where`, when not).
  bool defines_its_class(const std::string& script, const std::string& class_name, const std::string& where);
  // Whether a Ruby class is Behaviour or inherits from it.
  bool is_behaviour_class(RClass* klass);
  // Whether a behaviour class, or a module it includes, defines `method` rather than Behaviour.
  bool overrides(RClass* klass, const char* method);
  // Reports why a behaviour could not be created; returns nil, which stands for it.
  mrb_value not_created(const std::string& where, const std::string& error);

  // Calling the behaviours.
  void invoke(std::size_t index, const char* method, std::initializer_list<mrb_value> args);
  void invoke_on_object(ObjectId object, const char* method, std::initializer_list<mrb_value> args);
  // Calls `entry` with the frame's seconds on every behaviour that is running (and, for
  // fixed_update, has one of its own).
  void update_running(const char* entry, float dt, bool fixed);
  // Enabled, on an object that is still there.
  bool runnable(const BehaviourSlot& slot) const;
  // Runnable and started: its callbacks are called.
  bool running(const BehaviourSlot& slot) const;
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
