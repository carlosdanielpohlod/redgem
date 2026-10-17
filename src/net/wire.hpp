#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "net/frame_buffer.hpp"

// What machines in a game say to each other, and to a relay: JSON objects, one per frame.
namespace redgem::net {

// Who a message is for, besides one peer's id.
inline constexpr int kEveryone = 0;
// The id of the machine hosting the game. Whoever joins gets 2, 3, ... in the order they arrive.
inline constexpr int kHostId = 1;
inline constexpr int kFirstGuestId = kHostId + 1;
// Machines in one game, the host included.
inline constexpr int kMaxPeers = 16;
// What a message may be: how long its name and the keys of its hashes, and how deep its data may
// nest (arrays in hashes in arrays...). Checked where a message is sent and again where it arrives,
// since what arrives may come from a program that is not this one.
inline constexpr std::size_t kMaxNameBytes = 64;
inline constexpr int kMaxDataDepth = 16;
// ... and how many things one of its arrays or hashes may hold (every one of them becomes a value
// in the game's memory on arrival).
inline constexpr std::size_t kMaxDataMembers = 4096;
// How long a machine that connected may keep quiet before saying what it wants.
inline constexpr float kJoinTimeoutSeconds = 5.0f;
// A machine that stopped listening is let go once this much is waiting to be sent to it.
inline constexpr std::size_t kMaxPendingBytes = 8 * 1024 * 1024;
// What one poll takes from one machine (at least a whole message, so there is always progress).
inline constexpr std::size_t kMaxReadBytesPerPoll = 2 * kMaxFrameBytes;
// The messages of one machine handled in one poll; the rest wait for the next, and nothing more is
// read from it while that much is waiting. However small its messages, one machine cannot keep a
// poll busy: far more than a game sends in a frame, far less than a flood.
inline constexpr int kMaxMessagesPerPoll = 1024;
// A machine with nothing to say still says "I am here" this often; one not heard from for this
// long is gone (its cable pulled, its battery flat), whatever the connection claims.
inline constexpr float kPingEverySeconds = 10.0f;
inline constexpr float kSilenceTimeoutSeconds = 45.0f;
// A room on a relay is named by a short code (long enough that guessing one is hopeless); asking
// for this one opens a new room.
inline constexpr std::size_t kRoomCodeLetters = 6;
inline constexpr const char* kNewRoom = "new";
// Connection approval (Network.authenticate): what a newcomer's hello may carry for the game it
// joins to decide on (a hash, at most this long written out), how long that game has to decide
// (one that never does turns the newcomer away), and how long the reason of a refusal may be.
inline constexpr std::size_t kMaxAuthBytes = 4096;
inline constexpr float kApprovalSeconds = 10.0f;
inline constexpr std::size_t kMaxReasonBytes = 256;
// Why a newcomer is turned away when the game did not decide on it in time, or cannot take what
// its join carried.
inline constexpr const char* kTooLate = "the game did not decide in time";
inline constexpr const char* kUnreadableAuth = "what the join carried for the game (auth:) is not a hash it can take";

namespace wire {

// The keys of a message, and the kinds of message there are.
inline constexpr const char* kKind = "k";
inline constexpr const char* kName = "n";
inline constexpr const char* kFrom = "f";
inline constexpr const char* kTo = "t";
inline constexpr const char* kData = "d";
inline constexpr const char* kPeers = "p";
inline constexpr const char* kRoom = "r";
// In a welcome: the game's host is a dedicated server, which plays nobody. (Programs that do not
// know of it never look for it.)
inline constexpr const char* kServer = "s";
// In a hello: what the game joined decides on (Network.join's auth:). In a refusal: why. (Programs
// that do not know of them never look for them: a host of version 0.2.0 lets everyone in.)
inline constexpr const char* kAuth = "a";
inline constexpr const char* kReason = "y";
// In a hello that opens a room on a relay: whether the opener's game decides who comes in, so that
// the room is guarded from the start (later changes come as kGuarded).
inline constexpr const char* kGuards = "g";
inline constexpr const char* kGameMessage = "m";
inline constexpr const char* kHello = "h";        // a newcomer: let me in (to a relay: into this room)
inline constexpr const char* kWelcome = "w";      // to a newcomer: your id, and who else is here
inline constexpr const char* kArrived = "j";      // to the others: someone joined
inline constexpr const char* kGone = "l";         // to the others: someone left
inline constexpr const char* kPing = "i";         // nothing to say: I am here
inline constexpr const char* kConsidering = "c";  // to a newcomer: the game is deciding on your hello
inline constexpr const char* kRefused = "x";      // to a newcomer: you are not let in, and why
// Connection approval in a room on a relay, which decides nothing: it passes the newcomer's hello
// to the room's opener, and the opener's decision back.
inline constexpr const char* kGuarded = "g";  // opener to relay: whether its game decides who comes in (kData)
inline constexpr const char* kAsking = "q";   // relay to opener: newcomer kFrom asks to come in, with kAuth
inline constexpr const char* kVerdict = "v";  // opener to relay: newcomer kTo is let in (kData), or not and why

// The lowest id no machine in `taken` has, for a newcomer; 0 when the game is full. Ids are
// handed out again once free, so they never outgrow what a message may carry.
int free_peer_id(const std::vector<int>& taken);

nlohmann::json game_message(const std::string& name, const nlohmann::json& data, int from, int to);
// Whether what arrived is a message of ours, every field of the kind it should be (a list of
// peers: ids there can be, each once, and fewer than a full game). Anything else comes from a
// broken or hostile program, and nothing of it is used.
bool well_formed(const nlohmann::json& message);
// Whether what a hello carries for the game to decide on can be handed to it: nothing, or a hash
// within the limits of a message's data and kMaxAuthBytes.
bool acceptable_auth(const nlohmann::json& auth);
// What turns a newcomer away, saying why (cut to kMaxReasonBytes).
nlohmann::json refusal(const std::string& reason);
// Whether a message that is well formed is one a game sent, under a name to hear it by.
bool is_game_message(const nlohmann::json& message);
// Whether the id a welcome hands this machine can be its own: only the host is 1 (as is whoever
// opened a room on a relay, who can be nothing else), and nobody is 0.
bool own_id_believable(int given, bool through_relay, bool opening_room);
// A message as the bytes that travel: framed, with text that is not valid UTF-8 replaced instead
// of refused.
std::vector<std::uint8_t> packed(const nlohmann::json& message);

}  // namespace wire

// Whether `data` is something a message may carry.
bool acceptable_data(const nlohmann::json& data);

}  // namespace redgem::net
