#include "editor/editor_texts.hpp"

#include "core/translations.hpp"
#include "generated/editor_texts.hpp"

namespace redgem {

void add_editor_texts() { translations::add_files(generated::editor_texts()); }

}  // namespace redgem
