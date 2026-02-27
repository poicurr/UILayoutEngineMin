#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ui/draw_builder.hpp"

namespace ui {

class FontAtlas {
public:
  bool buildFromSystemFonts(float pixelHeight = 16.0F, int atlasWidth = 512,
                            int atlasHeight = 512);

  [[nodiscard]] bool isReady() const { return m_ready; }
  [[nodiscard]] float lineHeight() const { return m_lineHeight; }
  [[nodiscard]] float measureText(std::string_view text) const;

  [[nodiscard]] const std::vector<uint8_t> &atlasRgba() const {
    return m_atlasRgba;
  }
  [[nodiscard]] int atlasWidth() const { return m_atlasWidth; }
  [[nodiscard]] int atlasHeight() const { return m_atlasHeight; }

  void appendText(DrawListBuilder &builder, const DrawCmdKey &key, float x,
                  float y, std::string_view text, uint32_t rgbaPremul) const;

private:
  bool buildFromFile(const std::string &ttfPath, float pixelHeight,
                     int atlasWidth, int atlasHeight);

  bool m_ready = false;
  float m_lineHeight = 0.0F;
  int m_atlasWidth = 0;
  int m_atlasHeight = 0;
  std::vector<uint8_t> m_atlasRgba;

  struct GlyphInfo {
    float x0 = 0.0F;
    float y0 = 0.0F;
    float x1 = 0.0F;
    float y1 = 0.0F;
    float xAdvance = 0.0F;
  };

  GlyphInfo m_glyphs[96]{};
};

} // namespace ui
