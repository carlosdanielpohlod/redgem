#include <doctest/doctest.h>

#include "script/script_naming.hpp"

using namespace redgem;

TEST_CASE("script files map to Ruby class names") {
  CHECK(class_name_for_script("player.rb") == "Player");
  CHECK(class_name_for_script("camera_follow.rb") == "CameraFollow");
  CHECK(class_name_for_script("enemies/slime_king.rb") == "SlimeKing");
  CHECK(class_name_for_script("Door") == "Door");
}

TEST_CASE("script lists are comma separated") {
  CHECK(split_script_list("player.rb") == std::vector<std::string>{"player.rb"});
  CHECK(split_script_list(" a.rb , b.rb,,") == std::vector<std::string>{"a.rb", "b.rb"});
  CHECK(split_script_list("").empty());
}

TEST_CASE("script lists join back into the property format") {
  CHECK(join_script_list({}) == "");
  CHECK(join_script_list({"player.rb"}) == "player.rb");
  CHECK(join_script_list({"player.rb", "health.rb"}) == "player.rb, health.rb");
  CHECK(split_script_list(join_script_list({"a.rb", "b.rb"})) == std::vector<std::string>{"a.rb", "b.rb"});
}

TEST_CASE("a script in a list loses the spaces around its name, and only those") {
  CHECK(split_script_list("\t my script.rb \n") == std::vector<std::string>{"my script.rb"});
  CHECK(split_script_list("   ").empty());
  CHECK(split_script_list("a.rb,   ,b.rb") == std::vector<std::string>{"a.rb", "b.rb"});
}

TEST_CASE("a class name maps back to its script, a word of it at a time") {
  CHECK(script_for_class_name("Player") == "player.rb");
  CHECK(script_for_class_name("SlimeKing") == "slime_king.rb");
  CHECK(scene_script_for("main") == "main_scene.rb");
}
