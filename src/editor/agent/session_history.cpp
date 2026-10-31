#include "editor/agent/session_history.hpp"

#include <algorithm>
#include <chrono>
#include <system_error>

#include <nlohmann/json.hpp>

#include "app/json_file.hpp"
#include "core/log.hpp"
#include "core/platform.hpp"
#include "core/translations.hpp"
#include "storage/durable_file.hpp"

namespace redgem::agent {
namespace {

using Json = nlohmann::json;

constexpr const char* kSessionsFileName = "agent-sessions.json";
constexpr const char* kGamesKey = "games";
constexpr int kFileIndent = 2;
constexpr const char* kUnreadableSuffix = ".unreadable";
constexpr const char* kLineEnds = "\r\n";
constexpr const char* kCutMark = "...";
constexpr std::int64_t kMinute = 60;  // seconds
constexpr std::int64_t kHour = 60 * kMinute;
constexpr std::int64_t kDay = 24 * kHour;

// A game is known by its folder, written the same way whoever asks.
std::string game_key(const std::filesystem::path& game) {
  std::error_code error;
  const std::filesystem::path real = std::filesystem::weakly_canonical(std::filesystem::absolute(game, error), error);
  return (error ? game.lexically_normal() : real).u8string();
}

bool is_session(const Json& value) { return holds_text(value, "id") && holds_text(value, "agent"); }

PastSession session_of(const Json& value) {
  PastSession session;
  session.id = value["id"].get<std::string>();
  session.agent = value["agent"].get<std::string>();
  if (holds_text(value, "title")) session.title = value["title"].get<std::string>();
  if (holds_whole_number(value, "last_used")) session.last_used = value["last_used"].get<std::int64_t>();
  return session;
}

Json session_json(const PastSession& session) {
  return {{"id", session.id}, {"agent", session.agent}, {"title", session.title}, {"last_used", session.last_used}};
}

std::vector<PastSession> sessions_in(const Json& listed) {
  std::vector<PastSession> sessions;
  if (!listed.is_array()) return sessions;
  for (const Json& value : listed) {
    if (is_session(value)) sessions.push_back(session_of(value));
  }
  return sessions;
}

bool newer(const PastSession& one, const PastSession& other) { return one.last_used > other.last_used; }

// The file's games, as an object (a missing or broken file: none).
bool lists_games(const Json& read) {
  return read.is_object() && read.contains(kGamesKey) && read[kGamesKey].is_object();
}

Json games_in(const Json& read) { return lists_games(read) ? read[kGamesKey] : Json::object(); }

constexpr unsigned char kContinuationMask = 0xC0;  // UTF-8: the top bits of a byte inside a character
constexpr unsigned char kContinuation = 0x80;

bool inside_a_character(const std::string& text, std::size_t at) {
  return at < text.size() && (static_cast<unsigned char>(text[at]) & kContinuationMask) == kContinuation;
}

// The first `bytes` of a UTF-8 text, not cutting a character in two.
std::string cut_text(const std::string& text, std::size_t bytes) {
  while (bytes > 0 && inside_a_character(text, bytes)) --bytes;
  return text.substr(0, bytes);
}

// `sessions` with `session` in place of the one with its id, newest first, kKeptSessions at most.
std::vector<PastSession> with(std::vector<PastSession> sessions, const PastSession& session) {
  sessions.erase(
      std::remove_if(sessions.begin(), sessions.end(), [&](const PastSession& kept) { return kept.id == session.id; }),
      sessions.end());
  sessions.insert(sessions.begin(), session);  // (first among those of the same second)
  std::stable_sort(sessions.begin(), sessions.end(), newer);
  if (sessions.size() > kKeptSessions) sessions.resize(kKeptSessions);
  return sessions;
}

// A game's conversations in what the file holds, newest first.
std::vector<PastSession> sessions_of(const Json& games, const std::filesystem::path& game) {
  const auto found = games.find(game_key(game));
  std::vector<PastSession> sessions = found == games.end() ? std::vector<PastSession>{} : sessions_in(*found);
  std::stable_sort(sessions.begin(), sessions.end(), newer);
  return sessions;
}

// A file that is there but cannot be read as the list: what it holds is not thrown away with it.
bool unreadable(const std::filesystem::path& file, const Json& read) {
  std::error_code error;
  return std::filesystem::exists(file, error) && !read.is_object();
}

void keep_aside(const std::filesystem::path& file) {
  std::filesystem::path aside = file;
  aside += kUnreadableSuffix;
  std::error_code error;
  std::filesystem::rename(file, aside, error);
  log::warning(file.u8string() + " could not be read: kept as " + aside.u8string() + ", and a new one started");
}

// The whole file replaced at once, flushed to the disk: a crash leaves the old one or the new one.
bool write(const std::filesystem::path& file, const Json& content, std::string& error) {
  std::error_code fs_error;
  std::filesystem::create_directories(file.parent_path(), fs_error);
  if (durable::replace_file(file, content.dump(kFileIndent) + "\n")) return true;
  error = "cannot write " + file.u8string();
  return false;
}

}  // namespace

std::filesystem::path sessions_file() { return platform::user_data_dir() / kSessionsFileName; }

std::vector<PastSession> read_sessions(const std::filesystem::path& file, const std::filesystem::path& game) {
  return sessions_of(games_in(read_json_file(file)), game);
}

std::optional<std::vector<PastSession>> remember_session(const std::filesystem::path& file,
                                                         const std::filesystem::path& game, const PastSession& session,
                                                         std::string& error) {
  // (Read and written back at once, so that what another editor kept meanwhile is kept too.)
  const Json read = read_json_file(file);
  if (unreadable(file, read)) keep_aside(file);
  Json games = games_in(read);
  std::vector<PastSession> sessions = with(sessions_of(games, game), session);
  Json listed = Json::array();
  for (const PastSession& kept : sessions) listed.push_back(session_json(kept));
  games[game_key(game)] = listed;
  if (!write(file, Json{{kGamesKey, games}}, error)) return std::nullopt;
  return sessions;
}

std::string session_title(const std::string& first_message) {
  const std::size_t start = first_message.find_first_not_of(kLineEnds);
  if (start == std::string::npos) return "";
  std::string title = first_message.substr(start, first_message.find_first_of(kLineEnds, start) - start);
  if (title.size() <= kTitleLength) return title;
  return cut_text(title, kTitleLength) + kCutMark;
}

std::int64_t seconds_now() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

std::string time_ago(std::int64_t then, std::int64_t now) {
  const std::int64_t seconds = std::max<std::int64_t>(0, now - then);
  if (seconds < kMinute) return tr("editor.agent.ago_now");
  if (seconds < kHour) return tr("editor.agent.ago_minutes", {{"count", std::to_string(seconds / kMinute)}});
  if (seconds < kDay) return tr("editor.agent.ago_hours", {{"count", std::to_string(seconds / kHour)}});
  return tr("editor.agent.ago_days", {{"count", std::to_string(seconds / kDay)}});
}

}  // namespace redgem::agent
