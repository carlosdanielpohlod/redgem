#include "project/scene_revision.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <vector>

#include "app/encoder_process.hpp"
#include "core/config.hpp"
#include "core/files.hpp"
#include "core/translations.hpp"
#include "project/closest_words.hpp"
#include "project/scene_diff.hpp"
#include "project/tool_play.hpp"

namespace redgem::tools {
namespace {

namespace fs = std::filesystem;

constexpr const char* kGit = "git";
constexpr const char* kFolderFlag = "-C";  // git runs as if started in that folder
constexpr const char* kRevParse = "rev-parse";
constexpr const char* kInsideWorkTree = "--is-inside-work-tree";
constexpr const char* kShow = "show";
// "<revision>:./scene.glb": the file of that revision, by its path from the folder git runs in.
constexpr const char* kRevisionFileSeparator = ":";
constexpr const char* kFromHere = "./";
// The letters a revision is written with beside ASCII letters and digits: main, v1.2, HEAD~2,
// HEAD^, main@{1}, feature/door, 3fa2c1b.
constexpr std::string_view kRevisionPunctuation = "._/~^@{}-";
constexpr char kOptionMark = '-';
constexpr std::string_view kRange = "..";
// What a run of git says, and the scene a revision holds, in the scratch folder.
constexpr const char* kAnswerFile = "git.txt";
constexpr const char* kSceneAtRevision = "revision.glb";
constexpr const char* kTrailingSpace = " \t\r\n";
constexpr const char* kCurrentFolder = ".";

bool revision_letter(char letter) {
  const auto code = static_cast<unsigned char>(letter);
  return std::isalnum(code) != 0 || kRevisionPunctuation.find(letter) != std::string_view::npos;
}

bool sized_revision(const std::string& revision) { return !revision.empty() && revision.size() <= kMostRevisionBytes; }

bool only_revision_letters(const std::string& revision) {
  return std::all_of(revision.begin(), revision.end(), revision_letter);
}

bool reads_as_option(const std::string& revision) { return revision.front() == kOptionMark; }

bool names_range(const std::string& revision) { return revision.find(kRange) != std::string::npos; }

bool ended_well(const ProcessEnd& end) { return end.started && end.exited && end.code == kExitSuccess; }

// Runs git in `folder` with `words`, what it prints (output and errors) written to `output`.
ProcessEnd git(const fs::path& folder, std::vector<std::string> words, const fs::path& output) {
  words.insert(words.begin(), {kGit, kFolderFlag, folder.string()});
  return run_to_end(words, output);
}

// What git said in `output`, without the line end it closes with.
std::string git_said(const fs::path& output) {
  std::string error;
  std::string said = read_text_file(output, error).value_or("");
  said.erase(said.find_last_not_of(kTrailingSpace) + 1);
  return said;
}

bool inside_repository(const fs::path& folder, const ScratchFolder& scratch) {
  return ended_well(git(folder, {kRevParse, kInsideWorkTree}, scratch.path() / kAnswerFile));
}

// The folder git is run in: the scene file's.
fs::path folder_of(const fs::path& scene_file) {
  return scene_file.has_parent_path() ? scene_file.parent_path() : fs::path(kCurrentFolder);
}

ToolResult no_revision(const std::string& revision, const fs::path& scene_file, const fs::path& said) {
  return failed(tr("project.scene.no_revision",
                   {{"revision", revision}, {"file", scene_file.filename().string()}, {"problem", git_said(said)}}));
}

std::string revision_file(const std::string& revision, const fs::path& scene_file) {
  return revision + kRevisionFileSeparator + kFromHere + scene_file.filename().generic_string();
}

}  // namespace

bool names_scene_file(const std::string& against) {
  return lowered(fs::path(against).extension().string()) == config::kSceneExtension;
}

bool valid_revision(const std::string& revision) {
  if (!sized_revision(revision)) return false;
  if (!only_revision_letters(revision)) return false;
  if (reads_as_option(revision)) return false;
  if (names_range(revision)) return false;
  return !names_scene_file(revision);
}

ToolResult compare_scene_revision(const SceneDesc& current, const std::string& name, const fs::path& scene_file,
                                  const std::string& revision, std::size_t limit) {
  if (!valid_revision(revision)) return failed(tr("project.scene.bad_revision", {{"revision", revision}}), kExitUsage);
  const ScratchFolder scratch;  // (gone, with what git wrote in it, when this ends)
  const fs::path folder = folder_of(scene_file);
  if (!inside_repository(folder, scratch)) return failed(tr("project.scene.not_in_git", {{"folder", folder.string()}}));
  const fs::path before = scratch.path() / kSceneAtRevision;
  if (!ended_well(git(folder, {kShow, revision_file(revision, scene_file)}, before)))
    return no_revision(revision, scene_file, before);
  return compare_scene_file(current, name, before, limit);
}

}  // namespace redgem::tools
