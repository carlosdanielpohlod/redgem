#pragma once

// How much memory the graphics card has, and whether it ran out. A card that runs out of memory
// does not stop the program: it drops what it cannot hold, and the frames come out wrong (black,
// noise, or missing textures) with no other sign. These say so instead.

#include <optional>
#include <string>

namespace redgem {

// What the driver tells of the card's memory, in megabytes: NVIDIA's drivers tell both, AMD's the
// free part only, the others nothing.
struct CardMemory {
  std::optional<int> total_mb;
  std::optional<int> free_mb;
};

// Asks the driver (a window must be open). Nothing known: both empty.
CardMemory query_card_memory();

// "graphics memory: 4210 of 6141 MB used", "graphics memory: 1931 MB free", or empty when nothing
// is known.
std::string card_memory_text(const CardMemory& memory);

// The same, or that the driver tells nothing: for a message that always says something of it.
std::string card_memory_said(const CardMemory& memory);

// Whether the card has said it ran out of memory since the program started (a GL_OUT_OF_MEMORY
// error). The errors waiting are read, and so forgotten.
bool card_out_of_memory();

// Logs an error, once, the first time the card has run out of memory. Called once a frame.
void watch_card_memory();

}  // namespace redgem
