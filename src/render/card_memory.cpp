#include "render/card_memory.hpp"

#include "core/log.hpp"
#include "core/translations.hpp"
#include "render/gl_extras.hpp"

namespace redgem {
namespace {

constexpr int kKilobytesPerMegabyte = 1024;

int megabytes(long long kilobytes) { return static_cast<int>(kilobytes / kKilobytesPerMegabyte); }

}  // namespace

CardMemory query_card_memory() {
  const render_detail::CardMemoryKilobytes told = render_detail::card_memory_kilobytes();
  CardMemory memory;
  if (told.total > 0) memory.total_mb = megabytes(told.total);
  if (told.free > 0) memory.free_mb = megabytes(told.free);
  return memory;
}

std::string card_memory_text(const CardMemory& memory) {
  if (!memory.free_mb) return {};
  if (!memory.total_mb) return tr("game.card_memory.free", {{"free", std::to_string(*memory.free_mb)}});
  const int used = *memory.total_mb - *memory.free_mb;
  return tr("game.card_memory.used", {{"used", std::to_string(used)}, {"total", std::to_string(*memory.total_mb)}});
}

std::string card_memory_said(const CardMemory& memory) {
  const std::string text = card_memory_text(memory);
  return text.empty() ? tr("game.card_memory.unknown") : text;
}

bool card_out_of_memory() { return render_detail::card_ran_out_of_memory(); }

void watch_card_memory() {
  static bool told = false;
  if (told || !card_out_of_memory()) return;
  told = true;
  log::error(tr("game.card_memory.ran_out", {{"memory", card_memory_said(query_card_memory())}}));
}

}  // namespace redgem
