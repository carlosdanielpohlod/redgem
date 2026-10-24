#include "net/wire_codec.hpp"

#include <algorithm>

#include "core/log.hpp"
#include "core/platform.hpp"
#include "net/frame_buffer.hpp"
#include "net/wire.hpp"

namespace redgem::net::wire {

namespace {

using nlohmann::json;

constexpr int kCompact = -1;  // nlohmann's "no indentation"
// How deep a message nests: the message itself, holding data that nests kMaxDataDepth deep.
constexpr int kMaxMessageDepth = kMaxDataDepth + 1;
constexpr const char* kLogVariable = "REDGEM_LOG_MESSAGES";
constexpr const char* kLogOn = "1";
constexpr const char* kHidden = "(hidden)";  // what the log shows of a join's credentials

// The reader of MessagePack hands what it finds to this, which builds the message, and stops it
// at once when the message goes past what one may be: an array or a map nested deeper, or holding
// more, than a message's may (announced before any of it is read), or text that is not UTF-8.
class BoundedBuilder {
 public:
  explicit BoundedBuilder(json& message) : builder_(message, false) {}

  bool null() { return builder_.null(); }
  bool boolean(bool value) { return builder_.boolean(value); }
  bool number_integer(std::int64_t value) { return builder_.number_integer(value); }
  bool number_unsigned(std::uint64_t value) { return builder_.number_unsigned(value); }
  bool number_float(double value, const std::string& text) { return builder_.number_float(value, text); }
  bool string(std::string& value) { return valid_utf8(value) && builder_.string(value); }
  bool binary(json::binary_t& value) { return builder_.binary(value); }
  bool start_object(std::size_t members) { return opens(members) && builder_.start_object(members); }
  bool key(std::string& value) { return valid_utf8(value) && builder_.key(value); }
  bool end_object() { return closes() && builder_.end_object(); }
  bool start_array(std::size_t members) { return opens(members) && builder_.start_array(members); }
  bool end_array() { return closes() && builder_.end_array(); }
  bool parse_error(std::size_t, const std::string&, const json::exception&) { return false; }

 private:
  using Input = nlohmann::detail::iterator_input_adapter<std::string::const_iterator>;

  bool opens(std::size_t members) {
    ++depth_;
    return depth_ <= kMaxMessageDepth && members <= kMaxDataMembers;
  }
  bool closes() {
    --depth_;
    return true;
  }

  nlohmann::detail::json_sax_dom_parser<json, Input> builder_;
  int depth_ = 0;
};

bool texts_valid(const json& value);

bool members_valid(const json& object) {
  for (const auto& [key, member] : object.items()) {
    if (!valid_utf8(key) || !texts_valid(member)) return false;
  }
  return true;
}

// Whether every text in `value`, the keys of its maps included, is valid UTF-8.
bool texts_valid(const json& value) {
  if (value.is_string()) return valid_utf8(value.get_ref<const std::string&>());
  if (value.is_array()) return std::all_of(value.begin(), value.end(), texts_valid);
  return !value.is_object() || members_valid(value);
}

std::string as_text(const json& message) { return message.dump(kCompact, ' ', false, json::error_handler_t::replace); }

bool carries_credentials(const json& message) { return message.is_object() && message.contains(kAuth); }

bool logging_messages() {
  static const bool logging = platform::environment(kLogVariable) == kLogOn;
  return logging;
}

}  // namespace

std::vector<std::uint8_t> msgpack(const json& message) {
  if (logging_messages()) log::info("sent " + log_text(message));
  // (The rare message with broken text goes through JSON text, which replaces what is broken.)
  if (texts_valid(message)) return json::to_msgpack(message);
  return json::to_msgpack(json::parse(as_text(message)));
}

std::string log_text(const json& message) {
  if (!carries_credentials(message)) return as_text(message);
  json shown = message;
  shown[kAuth] = kHidden;
  return as_text(shown);
}

std::vector<std::uint8_t> packed(const json& message) { return frame(msgpack(message)); }

std::optional<json> unpacked(const std::string& bytes) {
  json message;
  BoundedBuilder builder(message);
  if (!json::sax_parse(bytes.begin(), bytes.end(), &builder, json::input_format_t::msgpack)) return std::nullopt;
  if (logging_messages()) log::info("received " + log_text(message));
  return message;
}

}  // namespace redgem::net::wire
