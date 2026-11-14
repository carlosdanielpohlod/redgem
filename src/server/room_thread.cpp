#include "server/room_thread.hpp"

#include <algorithm>
#include <exception>

#include "core/config.hpp"
#include "core/log.hpp"
#include "core/engine_thread.hpp"
#include "server/tick_pacer.hpp"

namespace redgem::server {

namespace {

constexpr const char* kServerStopped = "the server stopped";

}  // namespace

RoomThread::RoomThread(RoomStart start) : start_(std::move(start)) {
  thread_ = std::thread([this] { run_engine_thread([this] { run(); }); });
}

RoomThread::~RoomThread() { stop(); }

void RoomThread::add(std::shared_ptr<RoomSlot> slot) {
  const std::lock_guard<std::mutex> hold(lock_);
  new_rooms_.push_back(std::move(slot));
}

void RoomThread::ask_to_stop() {
  const std::lock_guard<std::mutex> hold(lock_);
  stopping_ = true;
}

void RoomThread::stop() {
  ask_to_stop();
  if (thread_.joinable()) thread_.join();
}

void RoomThread::run() {
  Clock& clock = *start_.clock;
  TickPacer pacer(config::kFixedDeltaSeconds, clock.now());
  while (wait_for_turn(pacer.until_next(clock.now()))) {
    take_new_rooms();
    play_turn(pacer.take_turn(clock.now()));
    drop_closed_rooms();
  }
  take_new_rooms();  // (handed over as it stopped: their slots close too)
  for (Room& room : rooms_) end(room, kServerStopped);
  rooms_.clear();
}

bool RoomThread::wait_for_turn(double seconds) {
  start_.clock->sleep(seconds);
  const std::lock_guard<std::mutex> hold(lock_);
  return !stopping_;
}

void RoomThread::take_new_rooms() {
  const std::lock_guard<std::mutex> hold(lock_);
  for (auto& slot : new_rooms_) rooms_.push_back({std::move(slot), nullptr});
  new_rooms_.clear();
}

void RoomThread::play_turn(float seconds) {
  loading_seconds_ = 0.0;
  const double started = start_.clock->now();
  for (Room& room : rooms_) play_or_close(room, seconds);
  // What made the rooms of this thread late, or not: the turn of all of them, one after the other
  // (loading a new room's game aside: that happens once, as its first player arrives).
  const double turn = start_.clock->now() - started - loading_seconds_;
  for (Room& room : rooms_) room.slot->note_frame(turn);
}

void RoomThread::play_or_close(Room& room, float seconds) {
  try {
    play(room, seconds);
  } catch (const std::exception& error) {
    end(room, std::string("its game failed: ") + error.what());  // the other rooms play on
  }
}

void RoomThread::play(Room& room, float seconds) {
  if (!room.game) open(room);
  // Before anyone is let in: what a behaviour's start sets up (Network.authenticate) decides about
  // the room's very first player too, not only about those who come after its first frame.
  room.game->scripts().start_pending();
  room.slot->admit_arrivals(room.game->network());
  room.slot->run_visits(*room.game);
  room.game->frame(seconds);
  room.slot->take_census(room.game->network());
  const bool quit = room.game->finished();
  if (room.slot->close_when_over(quit)) end(room, quit ? "the match ended" : "everyone left");
}

void RoomThread::open(Room& room) {
  const double started = start_.clock->now();
  room.game = open_game(room.slot->code());
  loading_seconds_ += start_.clock->now() - started;
}

std::unique_ptr<Engine> RoomThread::open_game(const std::string& code) const {
  EngineOptions options;
  options.game_dir = start_.game_dir;
  options.save = start_.save;
  options.server_room = code;
  return Engine::create(options, *start_.scene);
}

void RoomThread::end(Room& room, const std::string& why) {
  room.slot->close();
  if (room.game) {
    // A last try to deliver what the game said; then its players hear that the game is gone.
    room.game->network().leave();
    room.game.reset();
  }
  log::info("room " + room.slot->code() + " closed: " + why);
}

void RoomThread::drop_closed_rooms() {
  rooms_.erase(std::remove_if(rooms_.begin(), rooms_.end(), [](const Room& room) { return room.slot->closed(); }),
               rooms_.end());
}

}  // namespace redgem::server
