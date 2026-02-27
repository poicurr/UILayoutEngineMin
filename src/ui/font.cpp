#include "ui/font.hpp"

#include <array>
#include <filesystem>
#include <fstream>

#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

namespace ui {

namespace {

bool readBinaryFile(const std::string &path, std::vector<uint8_t> &out) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    return false;
  }

  ifs.seekg(0, std::ios::end);
  const std::streamoff size = ifs.tellg();
  if (size <= 0) {
    return false;
  }
  ifs.seekg(0, std::ios::beg);

  out.resize(static_cast<size_t>(size));
  ifs.read(reinterpret_cast<char *>(out.data()), size);
  return ifs.good();
}

} // namespace

bool FontAtlas::buildFromSystemFonts(float pixelHeight, int atlasWidth,
                                     int atlasHeight) {
  const std::array<const char *, 4> candidates = {
      "C:/Windows/Fonts/consola.ttf",
      "C:/Windows/Fonts/segoeui.ttf",
      "C:/Windows/Fonts/arial.ttf",
      "C:/Windows/Fonts/tahoma.ttf",
  };

  for (const char *path : candidates) {
    if (!std::filesystem::exists(path)) {
      continue;
    }
    if (buildFromFile(path, pixelHeight, atlasWidth, atlasHeight)) {
      return true;
    }
  }

  return false;
}

bool FontAtlas::buildFromFile(const std::string &ttfPath, float pixelHeight,
                              int atlasWidth, int atlasHeight) {
  std::vector<uint8_t> ttf;
  if (!readBinaryFile(ttfPath, ttf)) {
    return false;
  }

  std::vector<uint8_t> alpha(static_cast<size_t>(atlasWidth * atlasHeight), 0);
  stbtt_bakedchar baked[96]{};

  const int bakeResult =
      stbtt_BakeFontBitmap(ttf.data(), 0, pixelHeight, alpha.data(), atlasWidth,
                           atlasHeight, 32, 96, baked);

  if (bakeResult <= 0) {
    return false;
  }

  m_atlasWidth = atlasWidth;
  m_atlasHeight = atlasHeight;
  m_lineHeight = pixelHeight;
  m_atlasRgba.resize(static_cast<size_t>(atlasWidth * atlasHeight * 4));

  for (int i = 0; i < atlasWidth * atlasHeight; ++i) {
    const uint8_t a = alpha[static_cast<size_t>(i)];
    const size_t off = static_cast<size_t>(i) * 4;
    m_atlasRgba[off + 0] = a;
    m_atlasRgba[off + 1] = a;
    m_atlasRgba[off + 2] = a;
    m_atlasRgba[off + 3] = a;
  }

  for (int i = 0; i < 96; ++i) {
    const stbtt_bakedchar &g = baked[i];
    GlyphInfo gi{};
    gi.x0 = static_cast<float>(g.x0) / static_cast<float>(atlasWidth);
    gi.y0 = static_cast<float>(g.y0) / static_cast<float>(atlasHeight);
    gi.x1 = static_cast<float>(g.x1) / static_cast<float>(atlasWidth);
    gi.y1 = static_cast<float>(g.y1) / static_cast<float>(atlasHeight);
    gi.xAdvance = g.xadvance;
    m_glyphs[i] = gi;
  }

  m_ready = true;
  return true;
}

float FontAtlas::measureText(std::string_view text) const {
  if (!m_ready) {
    return 0.0F;
  }

  float x = 0.0F;
  for (const char ch : text) {
    if (ch < 32 || ch > 126) {
      x += m_lineHeight * 0.5F;
      continue;
    }
    const GlyphInfo &g = m_glyphs[ch - 32];
    x += g.xAdvance;
  }
  return x;
}

void FontAtlas::appendText(DrawListBuilder &builder, const DrawCmdKey &key,
                           float x, float y, std::string_view text,
                           uint32_t rgbaPremul) const {
  if (!m_ready) {
    return;
  }

  float cursorX = x;
  for (const char ch : text) {
    if (ch < 32 || ch > 126) {
      cursorX += m_lineHeight * 0.5F;
      continue;
    }

    const GlyphInfo &g = m_glyphs[ch - 32];
    const float glyphW = (g.x1 - g.x0) * static_cast<float>(m_atlasWidth);
    const float glyphH = (g.y1 - g.y0) * static_cast<float>(m_atlasHeight);

    RectF rect{};
    rect.x = cursorX;
    rect.y = y;
    rect.w = glyphW;
    rect.h = glyphH;

    UvRect uv{};
    uv.u0 = g.x0;
    uv.v0 = g.y0;
    uv.u1 = g.x1;
    uv.v1 = g.y1;

    builder.addQuad(rect, uv, rgbaPremul, key);
    cursorX += g.xAdvance;
  }
}

} // namespace ui
