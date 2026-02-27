#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace ui {

// 0 is reserved as INVALID.
using UiTextureId = uint32_t;
static constexpr UiTextureId INVALID_UI_TEXTURE_ID = 0;

enum class UiTextureColorSpace : uint8_t {
  Linear = 0,
  Srgb,
};

enum class UiSamplerId : uint8_t {
  LinearClamp = 0,
  LinearRepeat,
  NearestClamp,
};

struct UiScissorRectPx {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
};

struct UiVertex {
  float x = 0.0F; // px
  float y = 0.0F; // px (top-left origin in this sample)
  float u = 0.0F;
  float v = 0.0F;
  uint32_t rgbaPremul = 0; // premultiplied RGBA8 (R in lowest byte)
};

// Indexed-only command.
struct UiDrawCmd {
  uint32_t firstIndex = 0;
  uint32_t indexCount = 0;
  int32_t baseVertex = 0;

  UiTextureId textureId = INVALID_UI_TEXTURE_ID;
  UiTextureColorSpace textureColorSpace = UiTextureColorSpace::Srgb;
  UiSamplerId samplerId = UiSamplerId::LinearClamp;

  UiScissorRectPx scissor{};
};

struct UiDrawListView {
  uint32_t targetWidthPx = 0;
  uint32_t targetHeightPx = 0;
  std::span<const UiVertex> vertices{};
  std::span<const uint32_t> indices{};
  std::span<const UiDrawCmd> commands{};

  [[nodiscard]] bool empty() const {
    return commands.empty() || vertices.empty() || indices.empty();
  }
};

struct UiDrawListOwned {
  uint32_t targetWidthPx = 0;
  uint32_t targetHeightPx = 0;
  std::vector<UiVertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<UiDrawCmd> commands;

  [[nodiscard]] UiDrawListView view() const {
    return UiDrawListView{
        .targetWidthPx = targetWidthPx,
        .targetHeightPx = targetHeightPx,
        .vertices = vertices,
        .indices = indices,
        .commands = commands,
    };
  }

  [[nodiscard]] bool empty() const {
    return commands.empty() || vertices.empty() || indices.empty();
  }
};

inline UiScissorRectPx intersectScissor(UiScissorRectPx a, UiScissorRectPx b) {
  const uint32_t ax2 = a.x + a.width;
  const uint32_t ay2 = a.y + a.height;
  const uint32_t bx2 = b.x + b.width;
  const uint32_t by2 = b.y + b.height;

  const uint32_t x1 = std::max(a.x, b.x);
  const uint32_t y1 = std::max(a.y, b.y);
  const uint32_t x2 = std::min(ax2, bx2);
  const uint32_t y2 = std::min(ay2, by2);

  if (x2 <= x1 || y2 <= y1) {
    return UiScissorRectPx{};
  }
  return UiScissorRectPx{.x = x1, .y = y1, .width = x2 - x1, .height = y2 - y1};
}

inline uint32_t packRgbaPremul(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  // Caller is responsible for premultiplying (r,g,b) by a.
  return (static_cast<uint32_t>(r) << 0U) | (static_cast<uint32_t>(g) << 8U) |
         (static_cast<uint32_t>(b) << 16U) | (static_cast<uint32_t>(a) << 24U);
}

} // namespace ui
