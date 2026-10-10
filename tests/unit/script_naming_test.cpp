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
