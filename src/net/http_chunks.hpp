#pragma once

#include <cstddef>
#include <string>

namespace redgem::net {

// The body of an answer sent in chunks (Transfer-Encoding: chunked): each chunk is its length in
// hexadecimal on a line of its own, then that many bytes and a line break; a chunk of length 0
// ends the body, after which come trailer lines and an empty one.
class ChunkedBody {
 public:
  // Takes what it can use out of `pending` (the rest stays there until more arrives) and adds the
  // body's bytes to `body`. False when what arrived is not chunks, or a chunk is longer than any
  // answer may be.
  bool read(std::string& pending, std::string& body);
  // Whether the last chunk and the trailer lines after it were read.
  bool done() const { return part_ == Part::Done; }

 private:
  enum class Part { Size, Data, DataEnd, Trailer, Done };

  // One step of reading: false when it is wrong, and `waiting` when more has to arrive first.
  bool step(std::string& pending, std::string& body, bool& waiting);
  bool read_size(std::string& pending, bool& waiting);
  void read_data(std::string& pending, std::string& body, bool& waiting);
  bool read_data_end(std::string& pending, bool& waiting);
  bool read_trailer(std::string& pending, bool& waiting);

  Part part_ = Part::Size;
  std::size_t left_ = 0;  // bytes of the chunk being read still to come
};

// Takes the first line out of `text` (without its line break) into `line`. False, leaving both
// as they are, when no whole line has arrived yet.
bool take_line(std::string& text, std::string& line);

}  // namespace redgem::net
