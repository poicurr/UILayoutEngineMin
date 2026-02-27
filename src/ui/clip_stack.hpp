#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "ui/drawlist.hpp"
#include "ui/layout.hpp"

namespace ui {

inline UiScissorRectPx rectToScissorPx(const RectF &r, uint32_t maxWidth,
                                       uint32_t maxHeight) {
  const float x1f = r.x;
  const float y1f = r.y;
  const float x2f = r.x + r.w;
  const float y2f = r.y + r.h;

  const int x1 = static_cast<int>(std::floor(x1f));
  const int y1 = static_cast<int>(std::floor(y1f));
  const int x2 = static_cast<int>(std::ceil(x2f));
  const int y2 = static_cast<int>(std::ceil(y2f));

  const int clampedX1 = std::clamp(x1, 0, static_cast<int>(maxWidth));
  const int clampedY1 = std::clamp(y1, 0, static_cast<int>(maxHeight));
  const int clampedX2 = std::clamp(x2, 0, static_cast<int>(maxWidth));
  const int clampedY2 = std::clamp(y2, 0, static_cast<int>(maxHeight));

  const int w = std::max(0, clampedX2 - clampedX1);
  const int h = std::max(0, clampedY2 - clampedY1);

  return UiScissorRectPx{.x = static_cast<uint32_t>(clampedX1),
                         .y = static_cast<uint32_t>(clampedY1),
                         .width = static_cast<uint32_t>(w),
                         .height = static_cast<uint32_t>(h)};
}

class ClipStack {
public:
  void clear(UiScissorRectPx root) {
    m_stack.clear();
    m_stack.push_back(root);
  }

  [[nodiscard]] UiScissorRectPx current() const {
    if (m_stack.empty()) {
      return UiScissorRectPx{};
    }
    return m_stack.back();
  }

  void pushRectIntersect(const RectF &rect, uint32_t maxWidth,
                         uint32_t maxHeight) {
    const UiScissorRectPx local = rectToScissorPx(rect, maxWidth, maxHeight);
    const UiScissorRectPx parent = current();
    m_stack.push_back(intersectScissor(parent, local));
  }

  void pop() {
    if (m_stack.size() > 1) {
      m_stack.pop_back();
    }
  }

private:
  std::vector<UiScissorRectPx> m_stack;
};

} // namespace ui
