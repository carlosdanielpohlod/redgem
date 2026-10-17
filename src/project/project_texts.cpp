#include "project/project_texts.hpp"

#include "core/translations.hpp"
#include "generated/project_texts.hpp"

namespace redgem {

void add_project_texts() { translations::add_files(generated::project_texts()); }

}  // namespace redgem
