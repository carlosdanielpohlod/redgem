#include "benchmark/field_textures.hpp"

#include <raylib.h>

namespace redgem::benchmark {
namespace {

constexpr int kSide = 64;  // pixels
constexpr int kBlades = 9;
constexpr int kBladeWidth = 3;
constexpr int kBladeGap = 7;
constexpr int kBladeShortest = 30;
constexpr int kBladeGrowth = 5;  // pixels taller each blade, wrapping
constexpr int kBladeHeights = 6;
constexpr int kHoles = 40;
constexpr int kHoleRadius = 4;
constexpr int kHoleStepX = 17;
constexpr int kHoleStepY = 29;
constexpr int kLeafRadius = 30;
constexpr ::Color kTransparent{0, 0, 0, 0};
constexpr ::Color kOpaqueWhite{255, 255, 255, 255};

ImageData encoded(Image image) {
  int size = 0;
  unsigned char* bytes = ExportImageToMemory(image, ".png", &size);
  ImageData data{{bytes, bytes + size}, ".png"};
  MemFree(bytes);
  UnloadImage(image);
  return data;
}

}  // namespace

ImageData grass_picture() {
  Image image = GenImageColor(kSide, kSide, kTransparent);
  for (int blade = 0; blade < kBlades; ++blade) {
    const int height = kBladeShortest + (blade % kBladeHeights) * kBladeGrowth;
    ImageDrawRectangle(&image, kBladeGap / 2 + blade * kBladeGap, kSide - height, kBladeWidth, height, kOpaqueWhite);
  }
  return encoded(image);
}

ImageData leaves_picture() {
  Image image = GenImageColor(kSide, kSide, kTransparent);
  ImageDrawCircle(&image, kSide / 2, kSide / 2, kLeafRadius, kOpaqueWhite);
  for (int hole = 0; hole < kHoles; ++hole) {
    ImageDrawCircle(&image, (hole * kHoleStepX) % kSide, (hole * kHoleStepY) % kSide, kHoleRadius, kTransparent);
  }
  return encoded(image);
}

}  // namespace redgem::benchmark
