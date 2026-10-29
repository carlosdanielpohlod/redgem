#include "app/engine.hpp"
namespace redgem {
void Engine::connect_saves() {
  context_.save = options_.save != nullptr ? options_.save : save_.get();
  save_slots_.use_default(*context_.save);
  context_.save_slot = kDefaultSaveSlot;
  context_.select_save_slot = [this](const std::string& name) { select_save_slot(name); };
  context_.list_save_slots = [this] { return save_slots_.names(); };
}
void Engine::select_save_slot(const std::string& name) {
  std::string chosen = name;
  auto& store = save_slots_.select(chosen);
  context_.save_slot.swap(chosen);
  context_.save = &store;
}
}  // namespace redgem
