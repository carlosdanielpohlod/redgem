#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "script/script_profile.hpp"

using namespace redgem;

namespace {
constexpr const char* kGame = "Game#update";
constexpr const char* kEnemy = "Enemy#think";
constexpr const char* kPath = "Path.find";
}  // namespace

TEST_CASE("a sample counts its innermost method as self and every method on it once as total") {
  ScriptProfile profile;
  profile.record({kGame, kEnemy, kPath});
  profile.record({kGame, kEnemy});
  profile.record({kGame, kEnemy, kGame, kEnemy});  // recursion: once a sample in total
  profile.record({});                              // nothing ran: no sample
  CHECK(profile.samples() == 3);
  const std::vector<ScriptMethodStat> top = profile.top(kMaxProfiledMethods);
  REQUIRE(top.size() == 3);
  CHECK(top[0].name == kEnemy);
  CHECK(top[0].self == 2);
  CHECK(top[0].total == 3);
  CHECK(top[1].name == kPath);
  CHECK(top[1].self == 1);
  CHECK(top[2].name == kGame);
  CHECK(top[2].self == 0);
  CHECK(top[2].total == 3);
}

TEST_CASE("the top methods are cut to the count asked, ties going by total then name") {
  ScriptProfile profile;
  profile.record({"B#b"});
  profile.record({"A#a"});
  profile.record({"C#c", "D#d"});
  profile.record({"D#d"});
  const std::vector<ScriptMethodStat> top = profile.top(2);
  REQUIRE(top.size() == 2);
  CHECK(top[0].name == "D#d");
  CHECK(top[1].name == "A#a");
  CHECK(profile.top(0).empty());
}

TEST_CASE("methods past the limit are not kept and their samples are counted as dropped") {
  ScriptProfile profile;
  for (std::size_t i = 0; i < kMaxProfiledMethods; ++i) profile.record({"M#m" + std::to_string(i)});
  CHECK(profile.dropped() == 0);
  profile.record({"M#m0", "Late#one"});
  CHECK(profile.dropped() == 1);
  CHECK(profile.samples() == kMaxProfiledMethods + 1);
  CHECK(profile.top(kMaxProfiledMethods + 1).size() == kMaxProfiledMethods);
  profile.clear();
  CHECK(profile.samples() == 0);
  CHECK(profile.dropped() == 0);
  CHECK(profile.top(1).empty());
}

TEST_CASE("shares are percentages of the samples, and nothing of none") {
  CHECK(share_percent(1, 4) == doctest::Approx(25.0));
  CHECK(share_percent(3, 0) == 0.0);
  const ScriptMethodStat method{"Player#update", 83, 104};
  CHECK(script_method_line(method, 200) == "Player#update 41.5% self, 52.0% total");
}

TEST_CASE("the exported report lists every method as tab-separated columns, most self first") {
  ScriptProfile profile;
  profile.record({kGame, kEnemy});
  profile.record({kGame});
  const std::string report = script_profile_report(profile);
  CHECK(report ==
        "# samples: 2\n"
        "# dropped: 0\n"
        "self_percent\ttotal_percent\tself_samples\ttotal_samples\tmethod\n"
        "50.0\t100.0\t1\t2\tGame#update\n"
        "50.0\t50.0\t1\t1\tEnemy#think\n");
}
