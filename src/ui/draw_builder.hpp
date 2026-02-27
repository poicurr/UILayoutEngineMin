#pragma once

#include "ui/drawlist.hpp"
#include "ui/layout.hpp"

namespace ui {

struct DrawCmdKey {
  UiTextureId textureId = 1;
  UiSamplerId samplerId = UiSamplerId::LinearClamp;
  UiScissorRectPx scissor{};

  [[nodiscard]] bool operator==(const DrawCmdKey &rhs) const {
    return textureId == rhs.textureId && samplerId == rhs.samplerId &&
           scissor.x == rhs.scissor.x && scissor.y == rhs.scissor.y &&
           scissor.width == rhs.scissor.width &&
           scissor.height == rhs.scissor.height;
  }
};

struct UvRect {
  float u0 = 0.0F;
  float v0 = 0.0F;
  float u1 = 1.0F;
  float v1 = 1.0F;
};

class DrawListBuilder {
public:
  explicit DrawListBuilder(UiDrawListOwned &drawList) : m_drawList(drawList) {}

  void addRect(RectF rectPx, uint32_t rgbaPremul, const DrawCmdKey &key) {
    addQuad(rectPx, UvRect{}, rgbaPremul, key);
  }

  void addQuad(RectF rectPx, UvRect uv, uint32_t rgbaPremul,
               const DrawCmdKey &key) {
    if (rectPx.w <= 0.0F || rectPx.h <= 0.0F || key.scissor.width == 0 ||
        key.scissor.height == 0) {
      return;
    }

    const uint32_t baseVertex =
        static_cast<uint32_t>(m_drawList.vertices.size());
    m_drawList.vertices.push_back(
        UiVertex{rectPx.x, rectPx.y, uv.u0, uv.v0, rgbaPremul});
    m_drawList.vertices.push_back(
        UiVertex{rectPx.x + rectPx.w, rectPx.y, uv.u1, uv.v0, rgbaPremul});
    m_drawList.vertices.push_back(UiVertex{
        rectPx.x + rectPx.w, rectPx.y + rectPx.h, uv.u1, uv.v1, rgbaPremul});
    m_drawList.vertices.push_back(
        UiVertex{rectPx.x, rectPx.y + rectPx.h, uv.u0, uv.v1, rgbaPremul});

    m_drawList.indices.push_back(baseVertex + 0);
    m_drawList.indices.push_back(baseVertex + 1);
    m_drawList.indices.push_back(baseVertex + 2);
    m_drawList.indices.push_back(baseVertex + 0);
    m_drawList.indices.push_back(baseVertex + 2);
    m_drawList.indices.push_back(baseVertex + 3);

    appendIndices(6, key);
  }

private:
  void appendIndices(uint32_t indexCount, const DrawCmdKey &key) {
    if (m_hasLastKey && !m_drawList.commands.empty() && m_lastKey == key) {
      m_drawList.commands.back().indexCount += indexCount;
      return;
    }

    UiDrawCmd cmd{};
    cmd.firstIndex =
        static_cast<uint32_t>(m_drawList.indices.size()) - indexCount;
    cmd.indexCount = indexCount;
    cmd.baseVertex = 0;
    cmd.textureId = key.textureId;
    cmd.samplerId = key.samplerId;
    cmd.textureColorSpace = UiTextureColorSpace::Linear;
    cmd.scissor = key.scissor;

    m_drawList.commands.push_back(cmd);
    m_lastKey = key;
    m_hasLastKey = true;
  }

  UiDrawListOwned &m_drawList;
  DrawCmdKey m_lastKey{};
  bool m_hasLastKey = false;
};

} // namespace ui
