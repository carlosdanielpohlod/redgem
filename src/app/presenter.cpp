#include "app/presenter.hpp"

#include "core/config.hpp"
#include "script/script_context.hpp"

namespace redgem {

int NullPresenter::text_width(const std::string& text, int size) const { return estimated_text_width(text, size); }

float NullPresenter::frame_seconds() const { return config::kFixedDeltaSeconds; }

}  // namespace redgem
