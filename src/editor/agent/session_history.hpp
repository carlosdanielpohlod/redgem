#pragma once

// The conversations with coding agents the Agent panel had for each game, so that one is never
// lost: kept per user (agent-sessions.json in the user's RedGem data folder, never in the game's
// folder, which may be shared), by the game's folder, newest first. The agent keeps the
// conversation itself; this is what the editor needs to ask for it back (session/load).

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace redgem::agent {

// How many past conversations each game keeps; older ones are let go.
inline constexpr std::size_t kKeptSessions = 20;
// The most characters of a first message a conversation's title keeps.
inline constexpr std::size_t kTitleLength = 80;

struct PastSession {
  std::string id;              // the agent's session id
  std::string agent;           // the name of the agent it was had with (AgentProfile::name)
  std::string title;           // from its first message
  std::int64_t last_used = 0;  // seconds since 1970, UTC
};

// <user data>/agent-sessions.json.
std::filesystem::path sessions_file();

// The game's past conversations, newest first; none when the file is missing or unreadable.
std::vector<PastSession> read_sessions(const std::filesystem::path& file, const std::filesystem::path& game);
// Remembers a conversation (in place of one with its id), keeping the kKeptSessions newest.
bool remember_session(const std::filesystem::path& file, const std::filesystem::path& game, const PastSession& session,
                      std::string& error);

// A conversation's title: the first line of its first message, cut to kTitleLength bytes (never
// inside a character).
std::string session_title(const std::string& first_message);
// The time now, as PastSession keeps it.
std::int64_t seconds_now();
// How long ago `then` was, as the history list says it ("5 min ago").
std::string time_ago(std::int64_t then, std::int64_t now);

}  // namespace redgem::agent
