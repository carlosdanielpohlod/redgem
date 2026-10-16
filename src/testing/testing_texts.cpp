#include "testing/testing_texts.hpp"

#include "core/translations.hpp"
#include "generated/testing_texts.hpp"

namespace redgem::testing {

void add_testing_texts() {
  for (const redgem::generated::FolderFile& file : redgem::generated::testing_texts()) {
    translations::add_file(file.path, file.file);
  }
}

}  // namespace redgem::testing
