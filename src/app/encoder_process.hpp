#pragma once

// The video encoder: a program of its own, started with its arguments as they are (through no
// shell, except on Windows, where each is quoted) and fed through a pipe on its standard input.

#include <cstdio>
#include <string>
#include <vector>

namespace redgem {

struct EncoderProcess {
  std::FILE* input = nullptr;  // null: it could not be started
  long long id = 0;            // the process, where the system names it
};

// Starts `arguments` (the program first) reading from a pipe.
EncoderProcess start_process(const std::vector<std::string>& arguments);
// Closes its input and waits for it to end. True when it ended well.
bool finish_process(EncoderProcess& process);
// Whether `arguments` start and end well (what they print is thrown away).
bool runs(const std::vector<std::string>& arguments);

}  // namespace redgem
