#include "testing/testing_texts.hpp"

#include "core/translations.hpp"
#include "generated/testing_texts.hpp"

namespace redgem::testing {

void add_testing_texts() { translations::add_files(redgem::generated::testing_texts()); }

}  // namespace redgem::testing
