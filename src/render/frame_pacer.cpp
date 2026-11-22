#include "render/frame_pacer.hpp"

#include <algorithm>
#include <utility>

namespace redgem {

FenceCalls card_fences() {
  return {render_detail::insert_card_fence, render_detail::wait_card_fence, render_detail::release_card_fence};
}

FramePacer::FramePacer(std::size_t frames_ahead, FenceCalls calls)
    : frames_ahead_(std::max<std::size_t>(frames_ahead, 1)), calls_(std::move(calls)) {}

FramePacer::~FramePacer() {
  for (const render_detail::CardFence fence : fences_) calls_.release(fence);
}

void FramePacer::frame_drawn() {
  fences_.push_back(calls_.insert());
  while (fences_.size() > frames_ahead_) wait_oldest();
}

void FramePacer::wait_oldest() {
  const render_detail::CardFence oldest = fences_.front();
  fences_.pop_front();
  calls_.wait(oldest);
  calls_.release(oldest);
}

}  // namespace redgem
