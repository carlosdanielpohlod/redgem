#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

#include "app/game_settings.hpp"
#include "core/files.hpp"
#include "physics/body_limit.hpp"
#include "physics/physics_config.hpp"

using namespace redgem;

namespace {

constexpr std::uint32_t kCustomMaxBodies = 16384;
constexpr std::size_t kCustomScale = kCustomMaxBodies / kDefaultMaxBodies;

BodyLimitReading read(const std::string& physics) { return body_limit_from_json(nlohmann::json::parse(physics)); }

// A game of nothing but its game.json, gone with the test.
class GameFile {
 public:
  explicit GameFile(const std::string& text) : dir_(std::filesystem::temp_directory_path() / "redgem_body_limit") {
    std::filesystem::remove_all(dir_);
    std::filesystem::create_directories(dir_);
    std::string error;
    REQUIRE_MESSAGE(write_text_file(dir_ / kGameSettingsFile, text, error), error);
  }
  ~GameFile() { std::filesystem::remove_all(dir_); }
  GameFile(const GameFile&) = delete;
  GameFile& operator=(const GameFile&) = delete;
  const std::filesystem::path& dir() const { return dir_; }

 private:
  std::filesystem::path dir_;
};

}  // namespace

TEST_CASE("a game that says nothing of its bodies holds the default number of them") {
  for (const char* physics : {"null", "{}", R"({"layers": ["player"]})", "[]"}) {
    const BodyLimitReading reading = read(physics);
    CHECK_MESSAGE(reading.max_bodies == kDefaultMaxBodies, physics);
    CHECK_MESSAGE(reading.error.empty(), physics);
  }
  const GameFile game("{}");
  const GameManifest manifest = read_game_manifest(game.dir());
  CHECK(manifest.settings.max_bodies == kDefaultMaxBodies);
  CHECK(manifest.max_bodies_error.empty());
}

TEST_CASE("game.json's max_bodies sizes the world, its bounds included") {
  for (const std::uint32_t asked : {kFewestMaxBodies, kCustomMaxBodies, kMostMaxBodies}) {
    const BodyLimitReading reading = read(R"({"max_bodies": )" + std::to_string(asked) + "}");
    CHECK(reading.max_bodies == asked);
    CHECK(reading.error.empty());
  }
  const GameFile game(R"({"physics": {"max_bodies": )" + std::to_string(kCustomMaxBodies) + "}}");
  const GameManifest manifest = read_game_manifest(game.dir());
  CHECK(manifest.settings.max_bodies == kCustomMaxBodies);
  CHECK(manifest.max_bodies_error.empty());
}

TEST_CASE("a max_bodies out of bounds, or no whole number, is refused and the default is kept") {
  const std::string below = std::to_string(kFewestMaxBodies - 1);
  const std::string above = std::to_string(kMostMaxBodies + 1);
  const std::string kBoundsSaid = std::string(kMaxBodiesKey) + " must be a whole number from " +
                                  std::to_string(kFewestMaxBodies) + " to " + std::to_string(kMostMaxBodies);
  for (const std::string& asked : {below, above, std::string("0"), std::string("-5"), std::string("100.5"),
                                   std::string("\"many\""), std::string("99999999999")}) {
    const BodyLimitReading reading = read(R"({"max_bodies": )" + asked + "}");
    CHECK_MESSAGE(reading.max_bodies == kDefaultMaxBodies, asked);
    CHECK_MESSAGE(reading.error.find(kBoundsSaid) != std::string::npos, asked);
  }
  const GameFile game(R"({"physics": {"max_bodies": )" + above + "}}");
  const GameManifest manifest = read_game_manifest(game.dir());
  CHECK(manifest.settings.max_bodies == kDefaultMaxBodies);
  CHECK_FALSE(manifest.max_bodies_error.empty());
}

TEST_CASE("the pairs, contacts and scratch memory of a step grow with the bodies, never below the default's") {
  const BodyCapacity standard = body_capacity(kDefaultMaxBodies);
  CHECK(standard.bodies == kDefaultMaxBodies);
  CHECK(standard.body_pairs == kDefaultMaxBodies);
  CHECK(standard.contact_constraints == kDefaultMaxBodies);
  CHECK(standard.scratch_bytes == config::kPhysicsTempAllocatorBytes);
  const BodyCapacity large = body_capacity(kCustomMaxBodies);
  CHECK(large.body_pairs == kCustomMaxBodies);
  CHECK(large.scratch_bytes == config::kPhysicsTempAllocatorBytes * kCustomScale);
  // A world smaller than the default's handles as much in a step; one between two multiples of the
  // default gets the scratch memory of the larger.
  const BodyCapacity small = body_capacity(kFewestMaxBodies);
  CHECK(small.bodies == kFewestMaxBodies);
  CHECK(small.body_pairs == kDefaultMaxBodies);
  CHECK(small.contact_constraints == kDefaultMaxBodies);
  CHECK(small.scratch_bytes == config::kPhysicsTempAllocatorBytes);
  CHECK(body_capacity(kDefaultMaxBodies + 1).scratch_bytes == config::kPhysicsTempAllocatorBytes * 2);
  // What no reading lets through is kept within the bounds all the same.
  CHECK(body_capacity(0).bodies == kFewestMaxBodies);
  CHECK(body_capacity(kMostMaxBodies * 2).bodies == kMostMaxBodies);
}
