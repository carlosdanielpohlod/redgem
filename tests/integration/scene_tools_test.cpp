// `redgem scene`: a game's scene read and changed from the command line, saved as the editor
// saves it, and still a scene the game and the editor open.

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "app/engine.hpp"
#include "core/exit_codes.hpp"
#include "editor/document/scene_document.hpp"
#include "project/closest_words.hpp"
#include "project/scene_command.hpp"
#include "project/scene_lookup.hpp"
#include "project/scene_text.hpp"
#include "project/tool_commands.hpp"
#include "scene/gltf/gltf_loader.hpp"
#include "test_support.hpp"

using namespace redgem;
using namespace redgem::tools;
namespace fs = std::filesystem;

namespace {

const fs::path kPlatformer = REDGEM_PLATFORMER_DIR;
constexpr std::size_t kPlatformerObjects = 27;
constexpr int kPlayerId = 5;  // games/platformer/scene.glb, as Blender exported it
constexpr int kVisorId = 6;   // the Player's child
constexpr std::size_t kFewLines = 3;
constexpr int kFrames = 30;

// A copy of the platformer of the test's own, gone with it.
class GameCopy {
 public:
  explicit GameCopy(const std::string& name) : dir_(fs::temp_directory_path() / ("redgem_scene_tools_" + name)) {
    fs::remove_all(dir_);
    fs::copy(kPlatformer, dir_, fs::copy_options::recursive);
  }
  ~GameCopy() { fs::remove_all(dir_); }
  GameCopy(const GameCopy&) = delete;
  GameCopy& operator=(const GameCopy&) = delete;

  std::string dir() const { return dir_.string(); }
  fs::path scene() const { return dir_ / "scene.glb"; }

 private:
  fs::path dir_;
};

ToolResult scene_tool(std::vector<std::string> words) {
  words.insert(words.begin(), kSceneCommand);
  const auto result = run_tool_command(words);
  REQUIRE(result.has_value());
  return *result;
}

SceneDesc load(const fs::path& file) {
  LoadResult loaded = load_gltf_scene(file.string());
  REQUIRE_MESSAGE(loaded.ok, loaded.error);
  return std::move(loaded.scene);
}

const NodeDesc& node_named(const SceneDesc& scene, const std::string& name) {
  const int index = scene.find_node(name);
  REQUIRE(index != kNoIndex);
  return scene.nodes[index];
}

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("a scene lists one short line per object, children under their parent") {
  const ToolResult listed = scene_tool({"list", kPlatformer.string()});
  CHECK(listed.exit_code == kExitSuccess);
  CHECK(has(listed.out, "scene main: 27 objects"));
  // Only what differs from an object's defaults: no rotation, no scale of 1, no mesh named as it is.
  CHECK(has(listed.out, "\n#5 Player pos=0,1.1,0 scripts=Player body=character jump_speed=9 speed=6 tag=player\n"));
  CHECK(has(listed.out, "\n  #6 Visor pos=0,0.45,-0.35 scale=0.5,0.2,0.12\n"));
  CHECK(has(listed.out, "#24 Camera pos=0,6,9 rot=-28.6,0,0 camera scripts=CameraFollow"));
  CHECK(has(listed.out, "#25 Sun pos=5,12,5 rot=-44.8,24.4,0 light=sun"));
}

TEST_CASE("a list can stop short, and says what it left out") {
  const ToolResult top = scene_tool({"list", kPlatformer.string(), "--depth", "0"});
  CHECK_FALSE(has(top.out, "Visor"));
  CHECK(has(top.out, "+5 more not shown"));
  const ToolResult few = scene_tool({"list", kPlatformer.string(), "--limit", "3"});
  CHECK(has(few.out, "#2 Step2"));
  CHECK_FALSE(has(few.out, "#3 GoalPad"));
  CHECK(has(few.out, "+" + std::to_string(kPlatformerObjects - kFewLines) + " more"));
  CHECK(scene_tool({"list", kPlatformer.string(), "--depth", "many"}).exit_code == kExitFailure);
}

TEST_CASE("numbers are rounded and lose their trailing zeros") {
  CHECK(number_text(1.5, kListDecimals) == "1.5");
  CHECK(number_text(2.0, kListDecimals) == "2");
  CHECK(number_text(-0.001, kListDecimals) == "0");
  CHECK(number_text(0.12345, kDetailDecimals) == "0.1235");
  CHECK(vector_text({1.0f, -2.25f, 3.0f}, kListDecimals) == "1,-2.25,3");
}

TEST_CASE("an object is found by id, by path or by a name only it has") {
  const SceneDesc scene = load(kPlatformer / "scene.glb");
  CHECK(find_object(scene, "#5").index == kPlayerId);
  CHECK(find_object(scene, "Player/Visor").index == kVisorId);
  CHECK(find_object(scene, "Visor").index == kVisorId);
  CHECK(find_object(scene, "#999").index == kNoIndex);
  CHECK(has(find_object(scene, "#999").error, "#0 to #27"));
  // A mistyped name is answered with the names it is near.
  CHECK(has(find_object(scene, "Plyer").error, "did you mean: Player"));
  CHECK(id_number("#12") == 12);
  CHECK(id_number("#x") == kNoIndex);
  CHECK(id_number("12") == kNoIndex);
}

TEST_CASE("a name several objects share is refused with each of them") {
  GameCopy game("ambiguous");
  REQUIRE(scene_tool({"add", game.dir(), "Wall"}).exit_code == kExitSuccess);
  REQUIRE(scene_tool({"add", game.dir(), "Wall", "--parent", "Player"}).exit_code == kExitSuccess);
  REQUIRE(scene_tool({"set", game.dir(), "Wall.001", "name=Wall"}).exit_code == kExitSuccess);
  const ToolResult refused = scene_tool({"get", game.dir(), "Wall"});
  CHECK(refused.exit_code == kExitFailure);
  CHECK(has(refused.err, "2 objects are called Wall"));
  CHECK(has(refused.err, "Player/Wall"));
  CHECK(scene_tool({"get", game.dir(), "Player/Wall"}).exit_code == kExitSuccess);
}

TEST_CASE("set changes an object and saves the scene as the editor does") {
  GameCopy game("set");
  const ToolResult set = scene_tool({"set", game.dir(), "Coin1", "position=1,2,3", "rotation=0,90,0", "scale=2",
                                     "prop.value=5", "prop.label=\"big coin\"", "prop.trigger=", "script+=Spinner"});
  REQUIRE_MESSAGE(set.exit_code == kExitSuccess, set.err);
  CHECK(has(set.out, "#7 Coin1 pos=1,2,3 rot=0,90,0 scale=2 scripts=Coin,Spinner"));
  CHECK(has(set.err, "scripts/spinner.rb does not exist yet"));
  const SceneDesc scene = load(game.scene());
  const NodeDesc& coin = node_named(scene, "Coin1");
  CHECK(approx_equal(coin.local.position, Vec3{1.0f, 2.0f, 3.0f}, test::kTolerance));
  CHECK(approx_equal(coin.local.scale, Vec3::one() * 2.0f, test::kTolerance));
  CHECK(get_number(coin.props, "value") == doctest::Approx(5.0));
  CHECK(get_string(coin.props, "label") == "big coin");
  CHECK_FALSE(coin.props.count("trigger"));
  CHECK(get_string(coin.props, "script") == "coin.rb, spinner.rb");
  // The editor opens what was saved.
  CHECK(std::holds_alternative<SceneDocument>(SceneDocument::open(game.scene())));
}

TEST_CASE("a text that reads as a number is listed in quotes, so that set takes it back as a text") {
  GameCopy game("quoted");
  REQUIRE(scene_tool({"set", game.dir(), "Coin1", "prop.code=\"12\"", "prop.count=12"}).exit_code == kExitSuccess);
  const NodeDesc& coin = node_named(load(game.scene()), "Coin1");
  CHECK(get_string(coin.props, "code") == "12");
  CHECK(get_number(coin.props, "count") == doctest::Approx(12.0));
  CHECK(has(scene_tool({"get", game.dir(), "Coin1"}).out, "code=\"12\"\n  count=12\n"));
}

TEST_CASE("a hierarchy as deep as a file makes it is listed without running out of stack") {
  constexpr int kDepth = 100000;
  SceneDesc scene;
  for (int index = 0; index < kDepth; ++index) {
    NodeDesc node;
    node.name = "N" + std::to_string(index);
    node.parent = index - 1;
    if (index + 1 < kDepth) node.children = {index + 1};
    scene.nodes.push_back(node);
  }
  CHECK(has(scene_list(scene, "deep", ListLimits{kEveryLevel, 2}), "#0 N0\n  #1 N1\n+99998 more"));
  CHECK(saved_id(scene, kDepth - 1) == kDepth - 1);
}

TEST_CASE("scripts are taken off by name, and only those that are on") {
  GameCopy game("scripts");
  CHECK(scene_tool({"set", game.dir(), "Coin1", "script-=Coin"}).exit_code == kExitSuccess);
  CHECK_FALSE(node_named(load(game.scene()), "Coin1").props.count("script"));
  const ToolResult refused = scene_tool({"set", game.dir(), "Coin2", "script-=Door"});
  CHECK(refused.exit_code == kExitFailure);
  CHECK(has(refused.err, "door.rb is not attached; the object has: coin.rb"));
  REQUIRE(scene_tool({"set", game.dir(), "Coin3", "script+=Spinner,Door"}).exit_code == kExitSuccess);
  CHECK(scene_tool({"set", game.dir(), "Coin3", "script-=Coin,Door"}).exit_code == kExitSuccess);
  CHECK(get_string(node_named(load(game.scene()), "Coin3").props, "script") == "spinner.rb");
}

TEST_CASE("what set cannot do is refused, with what it can, and nothing is saved") {
  GameCopy game("refused");
  const auto before = fs::last_write_time(game.scene());
  CHECK(has(scene_tool({"set", game.dir(), "Coin1", "positon=1,2,3"}).err, "did you mean position?"));
  CHECK(has(scene_tool({"set", game.dir(), "Coin1", "position=1,2"}).err, "position needs x,y,z"));
  CHECK(has(scene_tool({"set", game.dir(), "Coin1", "speed"}).err, "is not key=value"));
  CHECK(has(scene_tool({"set", game.dir(), "Coin1", "position+=1,2,3"}).err, "takes = only"));
  CHECK(has(scene_tool({"set", game.dir(), "Player", "parent=Visor"}).err, "cannot move an object under itself"));
  CHECK(has(scene_tool({"set", game.dir(), "Coin1"}).err, "nothing to set"));
  CHECK(fs::last_write_time(game.scene()) == before);
}

TEST_CASE("add makes an empty object or a copy of another, where it is asked for") {
  GameCopy game("add");
  const ToolResult copy = scene_tool({"add", game.dir(), "Coin6", "--from", "Coin5", "position=3,3,-31"});
  REQUIRE_MESSAGE(copy.exit_code == kExitSuccess, copy.err);
  CHECK(has(copy.out, "#27 Coin6 pos=3,3,-31 mesh=Coin5 scripts=Coin tag=coin trigger=true"));
  const ToolResult child = scene_tool({"add", game.dir(), "Marker", "--parent", "Player"});
  // The id is the one the saved file gives it: right after the Player's other child.
  CHECK(has(child.out, "#7 Marker"));
  const SceneDesc scene = load(game.scene());
  CHECK(scene.nodes.size() == kPlatformerObjects + 2);
  CHECK(scene.nodes[node_named(scene, "Marker").parent].name == "Player");
  CHECK(has(scene_tool({"add", game.dir(), "Copy", "--from", "Nothing"}).err, "no object called Nothing"));
}

TEST_CASE("remove takes an object out with its children") {
  GameCopy game("remove");
  const ToolResult removed = scene_tool({"remove", game.dir(), "Tree1"});
  CHECK(has(removed.out, "removed #12 Tree1 (2 objects with its children)"));
  const SceneDesc scene = load(game.scene());
  CHECK(scene.nodes.size() == kPlatformerObjects - 2);
  CHECK(scene.find_node("Canopy1") == kNoIndex);
}

TEST_CASE("a scene that was changed from the command line still plays") {
  GameCopy game("plays");
  REQUIRE(scene_tool({"set", game.dir(), "Coin1", "position=0,1,-5", "prop.speed=3"}).exit_code == kExitSuccess);
  REQUIRE(scene_tool({"add", game.dir(), "Coin6", "--from", "Coin5"}).exit_code == kExitSuccess);
  test::LogCapture log;
  EngineOptions options;
  options.game_dir = game.dir();
  options.fixed_step = true;
  options.save_file = fs::path(game.dir()) / "save.json";
  auto created = Engine::create(options);
  REQUIRE(std::holds_alternative<std::unique_ptr<Engine>>(created));
  Engine& engine = *std::get<std::unique_ptr<Engine>>(created);
  for (int frame = 0; frame < kFrames; ++frame) engine.frame(config::kFixedDeltaSeconds);
  CHECK(engine.scripts().error_count() == 0);
  CHECK(engine.scene().find("Coin6") != kNoObject);
}

TEST_CASE("a scene the game does not have is answered with those it has") {
  const ToolResult missing = scene_tool({"list", kPlatformer.string(), "--scene", "level9"});
  CHECK(missing.exit_code == kExitFailure);
  CHECK(has(missing.err, "the game has no scene level9; its scenes are: main"));
  CHECK(has(scene_tool({"list", "no/such/game"}).err, "is not a game"));
}

TEST_CASE("the scene command explains itself") {
  CHECK(scene_tool({}).exit_code == kExitUsage);
  CHECK(has(scene_tool({"--help"}).out, "redgem scene list"));
  CHECK(scene_tool({"--help"}).exit_code == kExitSuccess);
  CHECK(has(scene_tool({"list", "--help"}).out, "redgem scene list"));
  // A word too many is refused, never left unread: only one object is removed at a time.
  CHECK(scene_tool({"remove", kPlatformer.string(), "Coin1", "Coin2"}).exit_code == kExitUsage);
  CHECK(scene_tool({"get", kPlatformer.string(), "Coin1", "position=1,2,3"}).exit_code == kExitUsage);
  CHECK(scene_tool({"get", kPlatformer.string()}).exit_code == kExitUsage);
  CHECK(scene_tool({"list", kPlatformer.string(), "--bogus"}).exit_code == kExitUsage);
  CHECK_FALSE(run_tool_command({"games/demo", "--play"}).has_value());  // the editor's line
}

TEST_CASE("names near a mistyped one") {
  const std::vector<std::string> known{"position", "rotation", "scale", "parent"};
  CHECK(closest_words("positon", known, 1) == std::vector<std::string>{"position"});
  CHECK(closest_words("scal", known, 3) == std::vector<std::string>{"scale"});
  CHECK(closest_words("zzzzzz", known, 3).empty());
  CHECK(edit_distance("Kitten", "sitting") == 3);
  CHECK(joined({"a", "b"}) == "a, b");
}
