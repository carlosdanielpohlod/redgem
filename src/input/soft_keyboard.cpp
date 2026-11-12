#include "input/soft_keyboard.hpp"

namespace redgem {

void SoftKeyboard::follow(const TextInputRequest& request) {
  if (brings_back(request)) put_away_ = false;
  if (!wanted(request)) {
    hide();
    return;
  }
  // (A tap shows it again even when it should be up: the system may have put it away on its own,
  // its back key closing it before the game hears of it.)
  if (asks_to_show(request)) show();
}

void SoftKeyboard::back_pressed() {
  if (!shown_) return;
  hide();  // (to be sure: it is down already when the system's keyboard took the key first)
  put_away_ = true;
}

void SoftKeyboard::leave_screen() {
  hide();
  put_away_ = false;
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
