#include "input/soft_keyboard.hpp"

namespace redgem {

void SoftKeyboard::follow(const TextInputRequest& request) {
  if (brings_back(request)) put_away_ = false;
  if (!wanted(request)) {
    hide();
    return;
  }
  // (A tap shows it again even when it should be up: the system may have put it away on its own,
  // its own back key closing it before the game hears of it.)
  if (asks_to_show(request)) show();
}

bool SoftKeyboard::take_back(bool down) {
  if (back_held_) {
    back_held_ = down;
    return true;
  }
  if (!closed_by(down)) return false;
  hide();
  put_away_ = true;
  back_held_ = true;
  return true;
}

void SoftKeyboard::show() {
  platform_.show();
  shown_ = true;
}

void SoftKeyboard::hide() {
  if (!shown_) return;
  platform_.hide();
  shown_ = false;
}

}  // namespace redgem
