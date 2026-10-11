#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include "core/files.hpp"

using namespace redgem;

TEST_CASE("a file's text is written whole and read back as it was") {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "redgem_files_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  const std::string text = std::string("first line\nsecond\r\n") + '\0' + "after a zero byte";
  std::string error;
  REQUIRE(write_text_file(dir / "notes.txt", text, error));
  CHECK(read_text_file(dir / "notes.txt", error) == text);
  REQUIRE(write_text_file(dir / "notes.txt", "shorter", error));  // nothing of the old text stays behind
  CHECK(read_text_file(dir / "notes.txt", error) == std::string("shorter"));
  CHECK(error.empty());
  std::filesystem::remove_all(dir);
}

TEST_CASE("a file that cannot be read or written says which one") {
  const std::filesystem::path missing = std::filesystem::temp_directory_path() / "redgem_files_test_missing" / "a.txt";
  std::filesystem::remove_all(missing.parent_path());
  std::string error;
  CHECK_FALSE(read_text_file(missing, error).has_value());
  CHECK(error.find(missing.string()) != std::string::npos);
  error.clear();
  CHECK_FALSE(write_text_file(missing, "text", error));  // its folder is not there
  CHECK(error.find(missing.string()) != std::string::npos);
}

TEST_CASE("a folder's path is cleaned of dots and of the separator at its end") {
  CHECK(normalized_folder("games/foo/") == "games/foo");
  CHECK(normalized_folder("games/foo").filename() == "foo");
  CHECK(normalized_folder("games/./bar/../foo//") == "games/foo");
  CHECK(normalized_folder("/") == "/");
  CHECK(normalized_folder("foo") == "foo");
}
