#pragma once

// Copied objects as text on the system's clipboard, so that they paste into another editor window
// too: a line that says what the text is, then the .glb the objects were copied as (see
// SceneDocument::copy_nodes) in base64.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace redgem::object_clipboard {

// The most bytes of copied objects put on the system's clipboard as text; bigger copies stay in
// the editor's own clipboard (they paste into the window they were copied in).
inline constexpr std::size_t kMaxClipboardBytes = std::size_t{64} * 1024 * 1024;  // 64 MiB

// The longest clipboard text that can hold objects: base64 takes four characters for every three
// bytes, and the header and the line ends a system may add get some room.
inline constexpr std::size_t kTextRoom = 4096;
inline constexpr std::size_t kMaxClipboardTextBytes = (kMaxClipboardBytes + 2) / 3 * 4 + kTextRoom;

// Whether copied objects this big go on the system's clipboard.
bool fits_system_clipboard(std::size_t bytes, std::size_t max_bytes = kMaxClipboardBytes);

// The clipboard's text for objects copied as `glb`.
std::string to_text(const std::vector<std::uint8_t>& glb);
// What goes on the system's clipboard when objects are copied: their text, or `too_big_note`
// (which holds no objects) for a copy bigger than `max_bytes`, so that no older copy there is
// pasted in place of this one, which stays in the editor's own clipboard.
std::string system_text(const std::vector<std::uint8_t>& glb, const std::string& too_big_note,
                        std::size_t max_bytes = kMaxClipboardBytes);
// The objects a clipboard's text holds; nullopt for any other text, and without reading it for a
// text longer than `max_text` (no objects copied here are that big).
std::optional<std::vector<std::uint8_t>> from_text(std::string_view text,
                                                   std::size_t max_text = kMaxClipboardTextBytes);

// Base64 (RFC 4648, with padding), and back; nullopt for text that is not base64.
std::string base64_encode(const std::vector<std::uint8_t>& bytes);
std::optional<std::vector<std::uint8_t>> base64_decode(const std::string& text);

}  // namespace redgem::object_clipboard
