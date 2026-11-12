#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "app/json_file.hpp"
#include "core/files.hpp"
#include "core/game_archive.hpp"
#include "core/game_files.hpp"
#include "core/safe_path.hpp"
#include "test_support.hpp"

using namespace redgem;

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t kTextRepeats = 400;          // a script long enough for deflate to shrink
constexpr std::size_t kNoiseBytes = 4096;          // bytes deflate cannot shrink
constexpr std::uint32_t kNoiseSeed = 2463534242u;  // xorshift's: any seed but 0
constexpr unsigned kShiftA = 13;
constexpr unsigned kShiftB = 17;
constexpr unsigned kShiftC = 5;
constexpr std::size_t kHeaderBytes = 32;
constexpr std::size_t kRecordBytes = 32;
constexpr std::size_t kVersionAt = 8;
constexpr std::size_t kCountAt = 12;
constexpr std::size_t kSizeInRecord = 16;
constexpr std::uint8_t kFlipped = 0xFF;
constexpr std::uint64_t kFewBytes = 4;
constexpr std::size_t kSeedSceneBytes = 8;  // the stand-in scene of the fuzzer's seed

Bytes bytes_of(const std::string& text) { return Bytes(text.begin(), text.end()); }

std::string repeated_script() {
  std::string text;
  for (std::size_t i = 0; i < kTextRepeats; ++i) text += "class Player < Behaviour\n  def update = jump\nend\n";
  return text;
}

// Bytes no compressor shrinks.
Bytes noise() {
  Bytes bytes;
  std::uint32_t state = kNoiseSeed;
  for (std::size_t i = 0; i < kNoiseBytes; ++i) {
    state ^= state << kShiftA;
    state ^= state >> kShiftB;
    state ^= state << kShiftC;
    bytes.push_back(static_cast<std::uint8_t>(state));
  }
  return bytes;
}

Bytes read_all(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  return Bytes(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// A folder of the test's own, removed when the test ends; nothing stays mounted after it.
struct Scratch {
  std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem-game-archive-test";
  Scratch() {
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
  }
  ~Scratch() {
    game_files::unmount();
    std::filesystem::remove_all(dir);
  }
};

std::vector<ArchiveInput> sample_inputs() {
  return {archive_input_from_bytes("scene.glb", noise()),
          archive_input_from_bytes("scripts/player.rb", bytes_of(repeated_script())),
          archive_input_from_bytes("locales/en.json", bytes_of(R"({"hello": "Hello"})")),
          archive_input_from_bytes("empty.txt", {})};
}

// The bytes of an archive of `inputs`, written as a build writes it.
Bytes archive_bytes(const Scratch& scratch, const std::vector<ArchiveInput>& inputs,
                    ArchiveCompression compression = ArchiveCompression::Deflate) {
  const std::filesystem::path file = scratch.dir / "made.rgpak";
  std::string error;
  REQUIRE_MESSAGE(write_game_archive(file, inputs, compression, error), error);
  return read_all(file);
}

std::string refusal_of(Bytes bytes) {
  std::string error;
  CHECK_FALSE(GameArchive::from_bytes(std::move(bytes), error).has_value());
  return error;
}

Bytes read_entry(const GameArchive& archive, const std::string& name) {
  const ArchiveEntry* entry = archive.find(name);
  REQUIRE(entry != nullptr);
  std::string error;
  const auto bytes = archive.read(*entry, kMaxArchiveEntryBytes, error);
  REQUIRE_MESSAGE(bytes.has_value(), error);
  return *bytes;
}

// Where the n-th name of an archive's table starts, the names before it being `before`.
std::size_t name_at(const std::vector<std::string>& before) {
  std::size_t at = kHeaderBytes + kRecordBytes;
  for (const std::string& name : before) at += name.size() + kRecordBytes;
  return at;
}

void put(Bytes& bytes, std::size_t at, const std::string& text) {
  std::copy(text.begin(), text.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at));
}

void put_number(Bytes& bytes, std::size_t at, std::uint64_t value, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}

}  // namespace

TEST_CASE("an archive gives back every file as it was written, compressed or stored") {
  const Scratch scratch;
  std::string error;
  for (const ArchiveCompression compression : {ArchiveCompression::Deflate, ArchiveCompression::None}) {
    const auto archive = GameArchive::from_bytes(archive_bytes(scratch, sample_inputs(), compression), error);
    REQUIRE_MESSAGE(archive.has_value(), error);
    CHECK(archive->entries().size() == sample_inputs().size());
    CHECK(read_entry(*archive, "scene.glb") == noise());
    CHECK(read_entry(*archive, "scripts/player.rb") == bytes_of(repeated_script()));
    CHECK(read_entry(*archive, "locales/en.json") == bytes_of(R"({"hello": "Hello"})"));
    CHECK(read_entry(*archive, "empty.txt").empty());
    const ArchiveEntry* script = archive->find("scripts/player.rb");
    const bool compressed = compression == ArchiveCompression::Deflate;
    CHECK((script->method == ArchiveMethod::Deflate) == compressed);
    CHECK((script->stored_size < script->size) == compressed);
    // What deflate cannot shrink is stored as it is, whatever was asked.
    CHECK(archive->find("scene.glb")->method == ArchiveMethod::Stored);
  }
}

// The fuzzer's seed was written from the documented format by a program of its own (Python's
// zlib): what the engine reads is the format as written down, not only what its own writer makes.
TEST_CASE("an archive written from the documented format by another program is read") {
  std::string error;
  const auto archive =
      GameArchive::open(std::string(REDGEM_SOURCE_DIR) + "/tests/fuzz/corpus/game_archive/two_files.rgpak", error);
  REQUIRE_MESSAGE(archive.has_value(), error);
  CHECK(read_entry(*archive, "scene.glb") == bytes_of(std::string("glTF\x02\x00\x00\x00", kSeedSceneBytes)));
  CHECK(archive->find("scripts/player.rb")->method == ArchiveMethod::Deflate);
  const Bytes script = read_entry(*archive, "scripts/player.rb");
  CHECK(std::string(script.begin(), script.end()).rfind("class Player < Behaviour\n", 0) == 0);
}

TEST_CASE("an archive is opened from its file, and knows its folders and what is in each") {
  const Scratch scratch;
  std::string error;
  archive_bytes(scratch, sample_inputs());
  const auto archive = GameArchive::open(scratch.dir / "made.rgpak", error);
  REQUIRE_MESSAGE(archive.has_value(), error);
  CHECK(archive->find("missing.rb") == nullptr);
  CHECK(archive->has_folder("scripts"));
  CHECK(archive->has_folder(""));
  CHECK_FALSE(archive->has_folder("script"));  // a name's start is not a folder
  CHECK_FALSE(archive->has_folder("scene.glb"));
  CHECK(archive->files_in("") == std::vector<std::string>{"empty.txt", "scene.glb"});
  CHECK(archive->files_in("scripts") == std::vector<std::string>{"player.rb"});
  CHECK(archive->files_in("nowhere").empty());
  // A file is read only up to what the caller allows.
  const auto bounded = archive->read(*archive->find("scene.glb"), kFewBytes, error);
  CHECK_FALSE(bounded.has_value());
  CHECK(error.find("larger than") != std::string::npos);
  CHECK_FALSE(GameArchive::open(scratch.dir / "nowhere.rgpak", error).has_value());
  CHECK(error.find("nowhere.rgpak") != std::string::npos);
}

TEST_CASE("only names a game may give go into an archive, each once") {
  const Scratch scratch;
  for (const std::string& name : {"../escape.rb", "/etc/passwd", "a//b.rb", "./a.rb", "a\\b.rb", "c:/x.rb", "dir/",
                                  std::string(kMaxArchiveNameBytes + 1, 'a'), std::string()}) {
    CAPTURE(name);
    std::string error;
    CHECK_FALSE(write_game_archive(scratch.dir / "bad.rgpak", {archive_input_from_bytes(name, {})},
                                   ArchiveCompression::Deflate, error));
    CHECK(error.find("cannot go into a game archive") != std::string::npos);
    CHECK_FALSE(std::filesystem::exists(scratch.dir / "bad.rgpak"));
  }
  std::string error;
  CHECK_FALSE(write_game_archive(scratch.dir / "twice.rgpak",
                                 {archive_input_from_bytes("a.rb", {}), archive_input_from_bytes("a.rb", {})},
                                 ArchiveCompression::Deflate, error));
  CHECK(error.find("given twice") != std::string::npos);
  CHECK(safe_archive_name("scripts/ui/heart.rb"));
  // A file that cannot be read fails the archive, and leaves no half of one behind.
  CHECK_FALSE(write_game_archive(scratch.dir / "unread.rgpak",
                                 {archive_input_from_file("gone.rb", scratch.dir / "gone.rb")},
                                 ArchiveCompression::Deflate, error));
  CHECK(error.find("gone.rb") != std::string::npos);
  CHECK_FALSE(std::filesystem::exists(scratch.dir / "unread.rgpak"));
}

TEST_CASE("an archive with a damaged header is refused with a reason") {
  const Scratch scratch;
  const Bytes good = archive_bytes(scratch, sample_inputs());
  CHECK(refusal_of({}).find("too short") != std::string::npos);
  CHECK(refusal_of(Bytes(good.begin(), good.begin() + kHeaderBytes - 1)).find("too short") != std::string::npos);
  Bytes magic = good;
  magic[0] = 'X';
  CHECK(refusal_of(magic).find("not a game archive") != std::string::npos);
  Bytes version = good;
  put_number(version, kVersionAt, kArchiveVersion + 1, sizeof(std::uint32_t));
  CHECK(refusal_of(version).find("version") != std::string::npos);
  Bytes count = good;
  put_number(count, kCountAt, kMaxArchiveEntries + 1, sizeof(std::uint32_t));
  CHECK(refusal_of(count).find("too many files") != std::string::npos);
  Bytes more = good;
  put_number(more, kCountAt, sample_inputs().size() + 1, sizeof(std::uint32_t));
  CHECK_FALSE(refusal_of(more).empty());
  CHECK(refusal_of(more).find("damaged") != std::string::npos);
}

TEST_CASE("a truncated archive is refused, wherever it was cut") {
  const Scratch scratch;
  const Bytes good = archive_bytes(scratch, sample_inputs());
  const std::array<std::size_t, 4> cuts = {kHeaderBytes, kHeaderBytes + kRecordBytes / 2, name_at({}) + 1,
                                           good.size() - 1};
  for (const std::size_t cut : cuts) {
    CAPTURE(cut);
    CHECK_FALSE(refusal_of(Bytes(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(cut))).empty());
  }
}

TEST_CASE("a file whose bytes do not match its CRC32 is refused when read") {
  const Scratch scratch;
  std::string error;
  Bytes bytes = archive_bytes(scratch, sample_inputs());
  auto archive = GameArchive::from_bytes(bytes, error);
  REQUIRE(archive.has_value());
  for (const char* name : {"scene.glb", "scripts/player.rb"}) {  // one stored, one deflated
    CAPTURE(name);
    Bytes damaged = bytes;
    const ArchiveEntry* entry = archive->find(name);
    damaged[static_cast<std::size_t>(entry->offset + entry->stored_size / 2)] ^= kFlipped;
    const auto opened = GameArchive::from_bytes(damaged, error);
    REQUIRE_MESSAGE(opened.has_value(), error);  // (the table is fine: the data is checked when read)
    CHECK_FALSE(opened->read(*opened->find(name), kMaxArchiveEntryBytes, error).has_value());
    CHECK(error.find(name) != std::string::npos);
  }
}

TEST_CASE("an archive that names a file outside the game, or one file twice, is refused") {
  const Scratch scratch;
  const std::vector<ArchiveInput> inputs = {archive_input_from_bytes("aa/one.rb", bytes_of("1")),
                                            archive_input_from_bytes("aa/two.rb", bytes_of("2"))};
  const Bytes good = archive_bytes(scratch, inputs);
  Bytes escaping = good;
  put(escaping, name_at({}), "../one.rb");
  CHECK(refusal_of(escaping).find("not one a game may give") != std::string::npos);
  Bytes twice = good;
  put(twice, name_at({"aa/one.rb"}), "aa/one.rb");
  CHECK(refusal_of(twice).find("twice") != std::string::npos);
  Bytes absolute = good;
  put(absolute, name_at({}), "/a/one.rb");
  CHECK(refusal_of(absolute).find("not one a game may give") != std::string::npos);
}

TEST_CASE("an archive whose sizes lie is refused before anything is read") {
  const Scratch scratch;
  // A file "a.rb" beside "a.rb/b.rb" would be a file and a folder at once.
  const Bytes clash = archive_bytes(
      scratch, {archive_input_from_bytes("a.rb", bytes_of("a")), archive_input_from_bytes("a.rb/b.rb", bytes_of("b"))});
  CHECK(refusal_of(clash).find("both a file and a folder") != std::string::npos);
  const Bytes good = archive_bytes(scratch, {archive_input_from_bytes("a.rb", bytes_of(repeated_script())),
                                             archive_input_from_bytes("a/b.rb", bytes_of("b"))});
  Bytes huge = good;
  put_number(huge, kHeaderBytes + kSizeInRecord, kMaxArchiveEntryBytes + 1, sizeof(std::uint64_t));
  CHECK(refusal_of(huge).find("too large") != std::string::npos);
  Bytes inflated = good;  // more than deflate could ever make of so few bytes
  put_number(inflated, kHeaderBytes + kSizeInRecord, kMaxArchiveEntryBytes, sizeof(std::uint64_t));
  CHECK(refusal_of(inflated).find("do not agree") != std::string::npos);
}

TEST_CASE("a mounted archive is the game's folder: its files are read from it, and only from it") {
  const Scratch scratch;
  std::string error;
  archive_bytes(scratch, sample_inputs());
  auto archive = GameArchive::open(scratch.dir / "made.rgpak", error);
  REQUIRE_MESSAGE(archive.has_value(), error);
  const std::filesystem::path game = scratch.dir / "game";  // not on the disk
  std::ofstream(scratch.dir / "beside.txt") << "on the disk";
  CHECK_FALSE(game_files::in_archive(game / "scene.glb"));
  game_files::mount(game, std::move(*archive));
  CHECK(game_files::mounted_root().has_value());
  CHECK(game_files::in_archive(game / "scene.glb"));
  CHECK(game_files::is_file(game / "scripts/player.rb"));
  CHECK(game_files::is_file(game / "scripts" / ".." / "scene.glb"));
  CHECK_FALSE(game_files::is_file(game / "scripts"));
  CHECK(game_files::is_folder(game / "scripts"));
  CHECK(game_files::is_folder(game));
  CHECK(game_files::file_size(game / "scene.glb") == kNoiseBytes);
  CHECK(game_files::read(game / "scene.glb", kMaxArchiveEntryBytes) == noise());
  CHECK(game_files::entries_in(game / "locales") == std::vector<std::filesystem::path>{game / "locales" / "en.json"});
  // What the engine reads a game with goes through it.
  CHECK(read_file_up_to(game / "scene.glb", kMaxArchiveEntryBytes) == noise());
  CHECK(read_text_file(game / "scripts/player.rb", error) == repeated_script());
  CHECK(read_json_file(game / "locales/en.json")["hello"] == "Hello");
  CHECK(file_inside(game, "scripts/player.rb").has_value());
  CHECK_FALSE(file_inside(game, "../beside.txt").has_value());
  const char* extensions[] = {".png", ".glb"};
  CHECK(find_asset_file(game, "scene", extensions) == game / "scene.glb");
  // A loose file put where the game would be is never read instead of the archive.
  std::filesystem::create_directories(game);
  std::ofstream(game / "intruder.rb") << "system('rm -rf /')";
  CHECK_FALSE(game_files::is_file(game / "intruder.rb"));
  CHECK_FALSE(game_files::read(game / "intruder.rb", kMaxArchiveEntryBytes).has_value());
  // Outside the game's folder, the disk.
  CHECK(game_files::is_file(scratch.dir / "beside.txt"));
  CHECK(read_text_file(scratch.dir / "beside.txt", error) == "on the disk");
  game_files::unmount();
  CHECK_FALSE(game_files::is_file(game / "scene.glb"));
  CHECK(game_files::is_file(game / "intruder.rb"));
}

TEST_CASE("a damaged file of a mounted archive is read as no file, and the log says why") {
  const Scratch scratch;
  std::string error;
  Bytes bytes = archive_bytes(scratch, sample_inputs());
  auto archive = GameArchive::from_bytes(bytes, error);
  REQUIRE(archive.has_value());
  const ArchiveEntry* entry = archive->find("scene.glb");
  bytes[static_cast<std::size_t>(entry->offset)] ^= kFlipped;
  auto damaged = GameArchive::from_bytes(bytes, error);
  REQUIRE(damaged.has_value());
  game_files::mount(scratch.dir / "game", std::move(*damaged));
  const test::LogCapture log;
  CHECK_FALSE(read_file_up_to(scratch.dir / "game" / "scene.glb", kMaxArchiveEntryBytes).has_value());
  CHECK(log.contains("CRC32"));
}
