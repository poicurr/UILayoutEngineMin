#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>

#include "ui/clip_stack.hpp"
#include "ui/draw_builder.hpp"
#include "ui/drawlist.hpp"
#include "ui/font.hpp"
#include "ui/input.hpp"
#include "ui/layout.hpp"

namespace {

static constexpr int WINDOW_W = 1280;
static constexpr int WINDOW_H = 760;

static constexpr float OUTER_PADDING_PX = 10.0F;
static constexpr float SPLITTER_THICKNESS_PX = 6.0F;
static constexpr float MIN_LEFT_PX = 180.0F;
static constexpr float MIN_CENTER_PX = 260.0F;
static constexpr float MIN_RIGHT_PX = 180.0F;
static constexpr float MIN_BOTTOM_PX = 120.0F;
static constexpr float MIN_TOP_PX = 180.0F;

static constexpr ui::UiTextureId WHITE_TEXTURE_ID = 1;
static constexpr ui::UiTextureId FONT_TEXTURE_ID = 2;

struct Rgba8 {
  uint8_t r = 255;
  uint8_t g = 255;
  uint8_t b = 255;
  uint8_t a = 255;
};

struct Theme {
  static constexpr Rgba8 BG{22, 24, 30, 255};
  static constexpr Rgba8 PANEL{40, 44, 52, 255};
  static constexpr Rgba8 PANEL_ALT{48, 53, 63, 255};
  static constexpr Rgba8 SPLITTER{70, 76, 88, 255};
  static constexpr Rgba8 SPLITTER_HOVER{116, 129, 154, 255};
  static constexpr Rgba8 SPLITTER_ACTIVE{150, 176, 220, 255};
  static constexpr Rgba8 INPUT_BG{32, 35, 42, 255};
  static constexpr Rgba8 INPUT_FOCUS{68, 96, 142, 255};
  static constexpr Rgba8 TEXT{224, 230, 238, 255};
  static constexpr Rgba8 TEXT_DIM{180, 188, 198, 255};
  static constexpr Rgba8 SELECT{78, 104, 148, 220};
  static constexpr Rgba8 HOVER{66, 78, 94, 220};
};

struct InputCapture {
  float wheelDelta = 0.0F;
  std::vector<char> chars;
};

struct UiModel {
  float leftRatio = 0.22F;
  float rightRatio = 0.24F;
  float bottomRatio = 0.24F;

  float treeScroll = 0.0F;
  float logScroll = 0.0F;

  int selectedIndex = 0;
  std::string searchText = "";
  std::string commandText = "";
  std::string propPosition = "0, 0, 0";
  std::string propScale = "1, 1, 1";
  std::string propTag = "Node";
  std::vector<std::string> logs;
};

enum class NodeKind : uint8_t {
  Root,
  TopRow,
  Bottom,
  Left,
  Center,
  Right,
  SplitterVLeft,
  SplitterVRight,
  SplitterH,
};

struct LayoutBuildResult {
  ui::UiTree tree;
  std::vector<NodeKind> nodeKinds;

  uint32_t leftId = 0;
  uint32_t centerId = 0;
  uint32_t rightId = 0;
  uint32_t bottomId = 0;
};

struct UiGpuResources {
  GLuint whiteTex = 0;
  GLuint fontTex = 0;
};

[[nodiscard]] uint32_t premulPack(Rgba8 c) {
  const float alpha = static_cast<float>(c.a) / 255.0F;
  const uint8_t pr =
      static_cast<uint8_t>(std::lround(static_cast<float>(c.r) * alpha));
  const uint8_t pg =
      static_cast<uint8_t>(std::lround(static_cast<float>(c.g) * alpha));
  const uint8_t pb =
      static_cast<uint8_t>(std::lround(static_cast<float>(c.b) * alpha));
  return ui::packRgbaPremul(pr, pg, pb, c.a);
}

[[nodiscard]] bool pointInRect(float x, float y, const ui::RectF &r) {
  return x >= r.x && y >= r.y && x <= (r.x + r.w) && y <= (r.y + r.h);
}

void scrollCallback(GLFWwindow *window, double, double yoffset) {
  auto *capture = static_cast<InputCapture *>(glfwGetWindowUserPointer(window));
  if (capture == nullptr) {
    return;
  }
  capture->wheelDelta += static_cast<float>(yoffset);
}

void charCallback(GLFWwindow *window, uint32_t codepoint) {
  auto *capture = static_cast<InputCapture *>(glfwGetWindowUserPointer(window));
  if (capture == nullptr) {
    return;
  }
  if (codepoint >= 32U && codepoint <= 126U) {
    capture->chars.push_back(static_cast<char>(codepoint));
  }
}

ui::UiInput buildUiInput(GLFWwindow *window, InputCapture &capture) {
  ui::UiInput input{};

  double mx = 0.0;
  double my = 0.0;
  glfwGetCursorPos(window, &mx, &my);
  input.mousePosX = static_cast<float>(mx);
  input.mousePosY = static_cast<float>(my);

  static bool s_lastMouseDown = false;
  const bool mouseDownNow =
      glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  input.mouseDown = mouseDownNow;
  input.mousePressed = mouseDownNow && !s_lastMouseDown;
  input.mouseReleased = !mouseDownNow && s_lastMouseDown;
  s_lastMouseDown = mouseDownNow;

  input.wheelDelta = capture.wheelDelta;
  capture.wheelDelta = 0.0F;

  input.charInput = std::move(capture.chars);
  capture.chars.clear();

  for (int key = 0; key < 512; ++key) {
    input.keyDown[static_cast<size_t>(key)] =
        glfwGetKey(window, key) == GLFW_PRESS;
  }

  return input;
}

[[nodiscard]] GLuint createRgbaTexture(int width, int height,
                                       const uint8_t *pixels) {
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels);
  return tex;
}

UiGpuResources createUiGpuResources(const ui::FontAtlas &fontAtlas) {
  UiGpuResources out{};

  const uint8_t white[4] = {255, 255, 255, 255};
  out.whiteTex = createRgbaTexture(1, 1, white);

  if (fontAtlas.isReady()) {
    out.fontTex =
        createRgbaTexture(fontAtlas.atlasWidth(), fontAtlas.atlasHeight(),
                          fontAtlas.atlasRgba().data());
  }

  return out;
}

void destroyUiGpuResources(UiGpuResources &gpu) {
  if (gpu.whiteTex != 0) {
    glDeleteTextures(1, &gpu.whiteTex);
    gpu.whiteTex = 0;
  }
  if (gpu.fontTex != 0) {
    glDeleteTextures(1, &gpu.fontTex);
    gpu.fontTex = 0;
  }
}

LayoutBuildResult buildDockTree(float width, float height,
                                const UiModel &model) {
  LayoutBuildResult result{};

  auto &tree = result.tree;
  tree.nodes.reserve(9);
  result.nodeKinds.reserve(9);

  auto addNode = [&](const char *name, ui::Style style,
                     NodeKind kind) -> uint32_t {
    const uint32_t id = static_cast<uint32_t>(tree.nodes.size());
    tree.nodes.push_back(
        ui::Node{.name = name, .style = style, .children = {}});
    result.nodeKinds.push_back(kind);
    return id;
  };

  const float innerW = std::max(1.0F, width - OUTER_PADDING_PX * 2.0F);
  const float innerH = std::max(1.0F, height - OUTER_PADDING_PX * 2.0F);

  float bottomH = std::clamp(
      innerH * model.bottomRatio, MIN_BOTTOM_PX,
      std::max(MIN_BOTTOM_PX, innerH - MIN_TOP_PX - SPLITTER_THICKNESS_PX));

  float leftW =
      std::clamp(innerW * model.leftRatio, MIN_LEFT_PX,
                 std::max(MIN_LEFT_PX, innerW - MIN_CENTER_PX - MIN_RIGHT_PX -
                                           2.0F * SPLITTER_THICKNESS_PX));

  float rightW =
      std::clamp(innerW * model.rightRatio, MIN_RIGHT_PX,
                 std::max(MIN_RIGHT_PX, innerW - MIN_CENTER_PX - leftW -
                                            2.0F * SPLITTER_THICKNESS_PX));

  float centerW = innerW - leftW - rightW - 2.0F * SPLITTER_THICKNESS_PX;
  if (centerW < MIN_CENTER_PX) {
    const float deficit = MIN_CENTER_PX - centerW;
    const float reduceLeft =
        std::min(deficit, std::max(0.0F, leftW - MIN_LEFT_PX));
    leftW -= reduceLeft;
    centerW += reduceLeft;

    const float remain = MIN_CENTER_PX - centerW;
    if (remain > 0.0F) {
      const float reduceRight =
          std::min(remain, std::max(0.0F, rightW - MIN_RIGHT_PX));
      rightW -= reduceRight;
      centerW += reduceRight;
    }
  }

  ui::Style root{};
  root.axis = ui::Axis::Column;
  root.width = {ui::SizeSpec::Kind::Fixed, width};
  root.height = {ui::SizeSpec::Kind::Fixed, height};
  root.padding = ui::EdgesF{OUTER_PADDING_PX, OUTER_PADDING_PX,
                            OUTER_PADDING_PX, OUTER_PADDING_PX};

  ui::Style topRow{};
  topRow.axis = ui::Axis::Row;
  topRow.width = {ui::SizeSpec::Kind::Fill, 1.0F};
  topRow.height = {ui::SizeSpec::Kind::Fill, 1.0F};
  topRow.clip = true;

  ui::Style splitH{};
  splitH.width = {ui::SizeSpec::Kind::Fill, 1.0F};
  splitH.height = {ui::SizeSpec::Kind::Fixed, SPLITTER_THICKNESS_PX};

  ui::Style bottom{};
  bottom.width = {ui::SizeSpec::Kind::Fill, 1.0F};
  bottom.height = {ui::SizeSpec::Kind::Fixed, bottomH};
  bottom.clip = true;

  ui::Style left{};
  left.width = {ui::SizeSpec::Kind::Fixed, leftW};
  left.height = {ui::SizeSpec::Kind::Fill, 1.0F};
  left.clip = true;

  ui::Style center{};
  center.width = {ui::SizeSpec::Kind::Fill, 1.0F};
  center.height = {ui::SizeSpec::Kind::Fill, 1.0F};
  center.clip = true;

  ui::Style right{};
  right.width = {ui::SizeSpec::Kind::Fixed, rightW};
  right.height = {ui::SizeSpec::Kind::Fill, 1.0F};
  right.clip = true;

  ui::Style splitV{};
  splitV.width = {ui::SizeSpec::Kind::Fixed, SPLITTER_THICKNESS_PX};
  splitV.height = {ui::SizeSpec::Kind::Fill, 1.0F};

  const uint32_t rootId = addNode("root", root, NodeKind::Root);
  const uint32_t topId = addNode("top", topRow, NodeKind::TopRow);
  const uint32_t splitHId = addNode("split_h", splitH, NodeKind::SplitterH);
  const uint32_t bottomId = addNode("bottom", bottom, NodeKind::Bottom);

  const uint32_t leftId = addNode("left", left, NodeKind::Left);
  const uint32_t splitLeftId =
      addNode("split_left", splitV, NodeKind::SplitterVLeft);
  const uint32_t centerId = addNode("center", center, NodeKind::Center);
  const uint32_t splitRightId =
      addNode("split_right", splitV, NodeKind::SplitterVRight);
  const uint32_t rightId = addNode("right", right, NodeKind::Right);

  tree.root = rootId;
  tree.nodes[rootId].children = {topId, splitHId, bottomId};
  tree.nodes[topId].children = {leftId, splitLeftId, centerId, splitRightId,
                                rightId};

  result.leftId = leftId;
  result.centerId = centerId;
  result.rightId = rightId;
  result.bottomId = bottomId;

  return result;
}

[[nodiscard]] ui::UiId splitterIdFromKind(NodeKind kind) {
  switch (kind) {
    case NodeKind::SplitterVLeft:
      return ui::hashUiId("splitter/left");
    case NodeKind::SplitterVRight:
      return ui::hashUiId("splitter/right");
    case NodeKind::SplitterH:
      return ui::hashUiId("splitter/h");
    default:
      return ui::INVALID_UI_ID;
  }
}

void updateSplitterInteraction(const ui::LayoutTree &layout,
                               const std::vector<NodeKind> &nodeKinds,
                               const ui::UiInput &input, ui::UiState &state,
                               UiModel &model, float viewWidth,
                               float viewHeight) {
  ui::UiId hovered = ui::INVALID_UI_ID;

  for (uint32_t i = 0; i < nodeKinds.size(); ++i) {
    const NodeKind kind = nodeKinds[i];
    if (kind != NodeKind::SplitterVLeft && kind != NodeKind::SplitterVRight &&
        kind != NodeKind::SplitterH) {
      continue;
    }

    const ui::RectF r = layout.rects[i];
    if (pointInRect(input.mousePosX, input.mousePosY, r)) {
      hovered = splitterIdFromKind(kind);
      break;
    }
  }

  if (hovered != ui::INVALID_UI_ID) {
    state.hotId = hovered;
  }

  if (input.mousePressed && state.hotId != ui::INVALID_UI_ID) {
    state.activeId = state.hotId;
    state.focusId = state.hotId;
  }

  if (input.mouseReleased && state.activeId != ui::INVALID_UI_ID) {
    state.activeId = ui::INVALID_UI_ID;
  }

  if (!input.mouseDown) {
    return;
  }

  const float innerW = std::max(1.0F, viewWidth - OUTER_PADDING_PX * 2.0F);
  const float innerH = std::max(1.0F, viewHeight - OUTER_PADDING_PX * 2.0F);
  // Resize uses reversed cursor axes to match engine-side cursor move
  // semantics.
  const float xLocal = (viewWidth - OUTER_PADDING_PX) - input.mousePosX;
  const float yLocal = (viewHeight - OUTER_PADDING_PX) - input.mousePosY;

  const ui::UiId splitLeftId = splitterIdFromKind(NodeKind::SplitterVLeft);
  const ui::UiId splitRightId = splitterIdFromKind(NodeKind::SplitterVRight);
  const ui::UiId splitHId = splitterIdFromKind(NodeKind::SplitterH);

  if (state.activeId == splitLeftId) {
    const float maxLeft =
        innerW - MIN_CENTER_PX - MIN_RIGHT_PX - 2.0F * SPLITTER_THICKNESS_PX;
    const float left =
        std::clamp(xLocal, MIN_LEFT_PX, std::max(MIN_LEFT_PX, maxLeft));
    model.leftRatio = left / innerW;
  } else if (state.activeId == splitRightId) {
    const float right =
        std::clamp(innerW - xLocal, MIN_RIGHT_PX,
                   std::max(MIN_RIGHT_PX, innerW - MIN_CENTER_PX - MIN_LEFT_PX -
                                              2.0F * SPLITTER_THICKNESS_PX));
    model.rightRatio = right / innerW;
  } else if (state.activeId == splitHId) {
    const float bottom = std::clamp(
        innerH - yLocal, MIN_BOTTOM_PX,
        std::max(MIN_BOTTOM_PX, innerH - MIN_TOP_PX - SPLITTER_THICKNESS_PX));
    model.bottomRatio = bottom / innerH;
  }
}

Rgba8 panelColor(NodeKind kind) {
  switch (kind) {
    case NodeKind::Root:
      return Theme::BG;
    case NodeKind::TopRow:
      return Theme::PANEL;
    case NodeKind::Bottom:
      return Theme::PANEL_ALT;
    case NodeKind::Left:
      return Theme::PANEL;
    case NodeKind::Center:
      return Theme::PANEL_ALT;
    case NodeKind::Right:
      return Theme::PANEL;
    case NodeKind::SplitterVLeft:
    case NodeKind::SplitterVRight:
    case NodeKind::SplitterH:
      return Theme::SPLITTER;
  }
  return Theme::PANEL;
}

void drawLabel(ui::DrawListBuilder &builder, ui::ClipStack &clipStack,
               const ui::FontAtlas &font, std::string_view label, float x,
               float y, uint32_t rgbaPremul) {
  ui::DrawCmdKey key{};
  key.textureId = FONT_TEXTURE_ID;
  key.scissor = clipStack.current();
  font.appendText(builder, key, x, y, label, rgbaPremul);
}

struct TextInputResult {
  bool clickedInside = false;
  bool submitted = false;
};

TextInputResult
drawTextInput(ui::DrawListBuilder &builder, ui::ClipStack &clipStack,
              const ui::FontAtlas &font, const ui::UiInput &input,
              ui::UiState &state, ui::UiId id, const ui::RectF &rect,
              std::string &buffer, uint32_t viewWidth, uint32_t viewHeight) {
  TextInputResult out{};

  const bool hovered = pointInRect(input.mousePosX, input.mousePosY, rect);
  if (hovered) {
    state.hotId = id;
  }

  if (input.mousePressed && hovered) {
    state.focusId = id;
    state.textInputId = id;
    out.clickedInside = true;
  }

  uint32_t &caret = state.uintState[id];
  caret = std::min(caret, static_cast<uint32_t>(buffer.size()));

  if (state.focusId == id) {
    for (const char ch : input.charInput) {
      buffer.insert(buffer.begin() + static_cast<std::ptrdiff_t>(caret), ch);
      ++caret;
    }

    if (state.isKeyPressed(input, GLFW_KEY_BACKSPACE) && caret > 0) {
      buffer.erase(buffer.begin() + static_cast<std::ptrdiff_t>(caret - 1));
      --caret;
    }
    if (state.isKeyPressed(input, GLFW_KEY_LEFT) && caret > 0) {
      --caret;
    }
    if (state.isKeyPressed(input, GLFW_KEY_RIGHT) && caret < buffer.size()) {
      ++caret;
    }
    if (state.isKeyPressed(input, GLFW_KEY_ENTER) ||
        state.isKeyPressed(input, GLFW_KEY_KP_ENTER)) {
      out.submitted = true;
    }
  }

  Rgba8 boxColor = Theme::INPUT_BG;
  if (state.focusId == id) {
    boxColor = Theme::INPUT_FOCUS;
  } else if (hovered) {
    boxColor = Theme::HOVER;
  }

  ui::DrawCmdKey boxKey{};
  boxKey.textureId = WHITE_TEXTURE_ID;
  boxKey.scissor = clipStack.current();
  builder.addRect(rect, premulPack(boxColor), boxKey);

  const float textX = rect.x + 6.0F;
  const float textY = rect.y + 4.0F;

  const ui::UiScissorRectPx textScissor = ui::intersectScissor(
      clipStack.current(), ui::rectToScissorPx(rect, viewWidth, viewHeight));

  ui::DrawCmdKey textKey{};
  textKey.textureId = FONT_TEXTURE_ID;
  textKey.scissor = textScissor;
  font.appendText(builder, textKey, textX, textY, buffer,
                  premulPack(Theme::TEXT));

  if (state.focusId == id) {
    const float caretX =
        textX + font.measureText(std::string_view(buffer.data(), caret));
    ui::RectF caretRect{caretX, rect.y + 3.0F, 1.5F, rect.h - 6.0F};
    builder.addRect(caretRect, premulPack(Theme::TEXT), boxKey);
  }

  return out;
}

void updateScrollValue(float &scrollOffset, float panelHeight,
                       float contentHeight, const ui::RectF &panelRect,
                       const ui::UiInput &input) {
  const bool hovered = pointInRect(input.mousePosX, input.mousePosY, panelRect);
  if (hovered && input.wheelDelta != 0.0F) {
    scrollOffset -= input.wheelDelta * 28.0F;
  }

  const float maxScroll = std::max(0.0F, contentHeight - panelHeight);
  scrollOffset = std::clamp(scrollOffset, 0.0F, maxScroll);
}

ui::UiDrawListOwned buildUiDrawList(uint32_t width, uint32_t height,
                                    const ui::FontAtlas &font,
                                    const ui::UiInput &input,
                                    ui::UiState &state, UiModel &model) {
  LayoutBuildResult layoutBuild = buildDockTree(
      static_cast<float>(width), static_cast<float>(height), model);

  const ui::LayoutTree layout = ui::computeLayout(
      layoutBuild.tree, ui::RectF{0.0F, 0.0F, static_cast<float>(width),
                                  static_cast<float>(height)});

  updateSplitterInteraction(layout, layoutBuild.nodeKinds, input, state, model,
                            static_cast<float>(width),
                            static_cast<float>(height));

  ui::UiDrawListOwned drawList;
  drawList.targetWidthPx = width;
  drawList.targetHeightPx = height;

  ui::ClipStack clipStack;
  clipStack.clear(ui::UiScissorRectPx{0, 0, width, height});

  ui::DrawListBuilder builder(drawList);

  std::function<void(uint32_t)> emitNode = [&](uint32_t nodeId) {
    const NodeKind kind = layoutBuild.nodeKinds[nodeId];
    const ui::RectF rect = layout.rects[nodeId];
    const ui::Style &style = layoutBuild.tree.nodes[nodeId].style;

    const bool isSplitter = kind == NodeKind::SplitterVLeft ||
                            kind == NodeKind::SplitterVRight ||
                            kind == NodeKind::SplitterH;

    Rgba8 c = panelColor(kind);
    if (isSplitter) {
      const ui::UiId id = splitterIdFromKind(kind);
      if (state.activeId == id) {
        c = Theme::SPLITTER_ACTIVE;
      } else if (state.hotId == id) {
        c = Theme::SPLITTER_HOVER;
      }
    }

    if (style.clip) {
      clipStack.pushRectIntersect(rect, width, height);
    }

    ui::DrawCmdKey key{};
    key.textureId = WHITE_TEXTURE_ID;
    key.scissor = clipStack.current();
    builder.addRect(rect, premulPack(c), key);

    for (uint32_t childId : layoutBuild.tree.nodes[nodeId].children) {
      emitNode(childId);
    }

    if (style.clip) {
      clipStack.pop();
    }
  };

  emitNode(layoutBuild.tree.root);

  bool clickedAnyTextInput = false;

  const ui::RectF leftRect = layout.rects[layoutBuild.leftId];
  const ui::RectF centerRect = layout.rects[layoutBuild.centerId];
  const ui::RectF rightRect = layout.rects[layoutBuild.rightId];
  const ui::RectF bottomRect = layout.rects[layoutBuild.bottomId];

  // Left panel: search + scrollable list.
  clipStack.pushRectIntersect(leftRect, width, height);

  drawLabel(builder, clipStack, font, "Tree", leftRect.x + 8.0F,
            leftRect.y + 8.0F, premulPack(Theme::TEXT));

  const ui::RectF searchRect{leftRect.x + 8.0F, leftRect.y + 30.0F,
                             leftRect.w - 16.0F, 24.0F};
  const TextInputResult searchInput = drawTextInput(
      builder, clipStack, font, input, state, ui::hashUiId("text/search"),
      searchRect, model.searchText, width, height);
  clickedAnyTextInput |= searchInput.clickedInside;

  const ui::RectF listRect{leftRect.x + 8.0F, leftRect.y + 60.0F,
                           leftRect.w - 16.0F, leftRect.h - 68.0F};
  const float rowH = 20.0F;

  struct TreeRow {
    ui::UiId id = ui::INVALID_UI_ID;
    std::string text;
    int depth = 0;
    bool isGroup = false;
    ui::UiId groupId = ui::INVALID_UI_ID;
    int leafIndex = -1;
  };

  std::vector<TreeRow> rows;
  rows.reserve(96);

  auto appendGroup = [&](std::string_view groupLabel, int baseLeaf) {
    const ui::UiId groupId =
        ui::hashUiId(std::string("tree/group/") + std::string(groupLabel));
    auto groupIt = state.uintState.find(groupId);
    if (groupIt == state.uintState.end()) {
      state.uintState[groupId] = 1;
      groupIt = state.uintState.find(groupId);
    }
    uint32_t &expanded = groupIt->second;

    rows.push_back(TreeRow{
        .id = groupId,
        .text =
            std::string(expanded != 0 ? "v " : "> ") + std::string(groupLabel),
        .depth = 0,
        .isGroup = true,
        .groupId = groupId,
        .leafIndex = -1,
    });

    if (expanded == 0) {
      return;
    }

    for (int i = 0; i < 10; ++i) {
      const int leaf = baseLeaf + i;
      char label[64];
      std::snprintf(label, sizeof(label), "Node %02d", leaf);
      if (!model.searchText.empty() &&
          std::string(label).find(model.searchText) == std::string::npos) {
        continue;
      }
      rows.push_back(TreeRow{
          .id = ui::hashUiId("tree/row", static_cast<uint32_t>(leaf)),
          .text = label,
          .depth = 1,
          .isGroup = false,
          .groupId = groupId,
          .leafIndex = leaf,
      });
    }
  };

  appendGroup("Scene", 0);
  appendGroup("Meshes", 10);
  appendGroup("Lights", 20);
  appendGroup("Cameras", 30);

  const float contentH = static_cast<float>(rows.size()) * rowH;
  updateScrollValue(model.treeScroll, listRect.h, contentH, listRect, input);

  clipStack.pushRectIntersect(listRect, width, height);

  for (size_t i = 0; i < rows.size(); ++i) {
    const float y =
        listRect.y + static_cast<float>(i) * rowH - model.treeScroll;
    if (y + rowH < listRect.y || y > listRect.y + listRect.h) {
      continue;
    }

    ui::RectF rowRect{listRect.x, y, listRect.w, rowH - 1.0F};
    const TreeRow &row = rows[i];
    const ui::UiId rowId = row.id;
    const bool hovered = pointInRect(input.mousePosX, input.mousePosY, rowRect);
    if (hovered) {
      state.hotId = rowId;
      if (input.mousePressed) {
        if (row.isGroup) {
          uint32_t &expanded = state.uintState[row.groupId];
          expanded = (expanded == 0) ? 1U : 0U;
        } else if (row.leafIndex >= 0) {
          model.selectedIndex = row.leafIndex;
        }
      }
    }

    if (!row.isGroup && model.selectedIndex == row.leafIndex) {
      ui::DrawCmdKey bgKey{};
      bgKey.textureId = WHITE_TEXTURE_ID;
      bgKey.scissor = clipStack.current();
      builder.addRect(rowRect, premulPack(Theme::SELECT), bgKey);
    } else if (hovered) {
      ui::DrawCmdKey bgKey{};
      bgKey.textureId = WHITE_TEXTURE_ID;
      bgKey.scissor = clipStack.current();
      builder.addRect(rowRect, premulPack(Theme::HOVER), bgKey);
    }

    const float textX =
        rowRect.x + 6.0F + static_cast<float>(row.depth) * 14.0F;
    drawLabel(builder, clipStack, font, row.text, textX, rowRect.y + 3.0F,
              premulPack(Theme::TEXT));
  }

  clipStack.pop();
  clipStack.pop();

  // Center panel: title text.
  clipStack.pushRectIntersect(centerRect, width, height);
  drawLabel(builder, clipStack, font, "Viewport", centerRect.x + 12.0F,
            centerRect.y + 10.0F, premulPack(Theme::TEXT));
  drawLabel(builder, clipStack, font, "LDR overlay UI", centerRect.x + 12.0F,
            centerRect.y + 30.0F, premulPack(Theme::TEXT_DIM));
  clipStack.pop();

  // Right panel: property grid.
  clipStack.pushRectIntersect(rightRect, width, height);
  drawLabel(builder, clipStack, font, "Properties", rightRect.x + 8.0F,
            rightRect.y + 8.0F, premulPack(Theme::TEXT));

  char selectedText[64];
  std::snprintf(selectedText, sizeof(selectedText), "Selected: Item %02d",
                model.selectedIndex);
  drawLabel(builder, clipStack, font, selectedText, rightRect.x + 8.0F,
            rightRect.y + 34.0F, premulPack(Theme::TEXT_DIM));

  const float labelColW = 84.0F;
  const float rowY0 = rightRect.y + 58.0F;
  const float rowGap = 30.0F;

  drawLabel(builder, clipStack, font, "Tag", rightRect.x + 8.0F, rowY0,
            premulPack(Theme::TEXT_DIM));
  const ui::RectF tagRect{rightRect.x + 8.0F + labelColW, rowY0 - 4.0F,
                          rightRect.w - (24.0F + labelColW), 24.0F};
  const TextInputResult tagInput = drawTextInput(
      builder, clipStack, font, input, state, ui::hashUiId("text/prop/tag"),
      tagRect, model.propTag, width, height);
  clickedAnyTextInput |= tagInput.clickedInside;

  drawLabel(builder, clipStack, font, "Position", rightRect.x + 8.0F,
            rowY0 + rowGap, premulPack(Theme::TEXT_DIM));
  const ui::RectF posRect{rightRect.x + 8.0F + labelColW, rowY0 + rowGap - 4.0F,
                          rightRect.w - (24.0F + labelColW), 24.0F};
  const TextInputResult posInput = drawTextInput(
      builder, clipStack, font, input, state, ui::hashUiId("text/prop/pos"),
      posRect, model.propPosition, width, height);
  clickedAnyTextInput |= posInput.clickedInside;

  drawLabel(builder, clipStack, font, "Scale", rightRect.x + 8.0F,
            rowY0 + rowGap * 2.0F, premulPack(Theme::TEXT_DIM));
  const ui::RectF scaleRect{rightRect.x + 8.0F + labelColW,
                            rowY0 + rowGap * 2.0F - 4.0F,
                            rightRect.w - (24.0F + labelColW), 24.0F};
  const TextInputResult scaleInput = drawTextInput(
      builder, clipStack, font, input, state, ui::hashUiId("text/prop/scale"),
      scaleRect, model.propScale, width, height);
  clickedAnyTextInput |= scaleInput.clickedInside;
  clipStack.pop();

  // Bottom panel: command input + scrollable log.
  clipStack.pushRectIntersect(bottomRect, width, height);
  drawLabel(builder, clipStack, font, "Log", bottomRect.x + 8.0F,
            bottomRect.y + 8.0F, premulPack(Theme::TEXT));

  const ui::RectF cmdRect{bottomRect.x + 8.0F, bottomRect.y + 30.0F,
                          bottomRect.w - 16.0F, 24.0F};

  const TextInputResult cmdInput = drawTextInput(
      builder, clipStack, font, input, state, ui::hashUiId("text/command"),
      cmdRect, model.commandText, width, height);
  clickedAnyTextInput |= cmdInput.clickedInside;

  if (cmdInput.submitted && !model.commandText.empty()) {
    model.logs.push_back(model.commandText);
    model.commandText.clear();
    state.uintState[ui::hashUiId("text/command")] = 0;
  }

  const ui::RectF logRect{bottomRect.x + 8.0F, bottomRect.y + 60.0F,
                          bottomRect.w - 16.0F, bottomRect.h - 68.0F};
  const float logRowH = std::max(16.0F, font.lineHeight() + 2.0F);
  const float logContentH = static_cast<float>(model.logs.size()) * logRowH;
  updateScrollValue(model.logScroll, logRect.h, logContentH, logRect, input);

  clipStack.pushRectIntersect(logRect, width, height);
  for (size_t i = 0; i < model.logs.size(); ++i) {
    const float y =
        logRect.y + static_cast<float>(i) * logRowH - model.logScroll;
    if (y + logRowH < logRect.y || y > logRect.y + logRect.h) {
      continue;
    }
    drawLabel(builder, clipStack, font, model.logs[i], logRect.x + 4.0F,
              y + 1.0F, premulPack(Theme::TEXT_DIM));
  }
  clipStack.pop();
  clipStack.pop();

  if (input.mousePressed && !clickedAnyTextInput &&
      state.focusId == state.textInputId) {
    state.focusId = ui::INVALID_UI_ID;
    state.textInputId = ui::INVALID_UI_ID;
  }

  return drawList;
}

void drawDrawListImmediate(const ui::UiDrawListView &drawList,
                           const UiGpuResources &gpu) {
  if (drawList.empty()) {
    return;
  }

  const int width = static_cast<int>(drawList.targetWidthPx);
  const int height = static_cast<int>(drawList.targetHeightPx);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glEnable(GL_SCISSOR_TEST);
  glEnable(GL_TEXTURE_2D);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, static_cast<double>(width), static_cast<double>(height), 0.0,
          -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  for (const ui::UiDrawCmd &cmd : drawList.commands) {
    const int sx = static_cast<int>(cmd.scissor.x);
    const int sy =
        height - static_cast<int>(cmd.scissor.y + cmd.scissor.height);
    const int sw = static_cast<int>(cmd.scissor.width);
    const int sh = static_cast<int>(cmd.scissor.height);

    if (sw <= 0 || sh <= 0) {
      continue;
    }

    GLuint tex = gpu.whiteTex;
    if (cmd.textureId == FONT_TEXTURE_ID && gpu.fontTex != 0) {
      tex = gpu.fontTex;
    }
    glBindTexture(GL_TEXTURE_2D, tex);
    glScissor(sx, sy, sw, sh);

    glBegin(GL_TRIANGLES);
    const uint32_t start = cmd.firstIndex;
    const uint32_t end = start + cmd.indexCount;

    for (uint32_t i = start; i < end; ++i) {
      const uint32_t vertexIndex = drawList.indices[i];
      const ui::UiVertex &v = drawList.vertices[vertexIndex];

      const uint32_t rgba = v.rgbaPremul;
      const float r = static_cast<float>((rgba >> 0U) & 0xFFU) / 255.0F;
      const float g = static_cast<float>((rgba >> 8U) & 0xFFU) / 255.0F;
      const float b = static_cast<float>((rgba >> 16U) & 0xFFU) / 255.0F;
      const float a = static_cast<float>((rgba >> 24U) & 0xFFU) / 255.0F;

      glColor4f(r, g, b, a);
      glTexCoord2f(v.u, v.v);
      glVertex2f(v.x, v.y);
    }
    glEnd();
  }

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_SCISSOR_TEST);
}

} // namespace

int main() {
  if (glfwInit() == GLFW_FALSE) {
    std::fprintf(stderr, "glfwInit failed\n");
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

  GLFWwindow *window = glfwCreateWindow(
      WINDOW_W, WINDOW_H, "UILayoutEngineMin", nullptr, nullptr);
  if (window == nullptr) {
    std::fprintf(stderr, "glfwCreateWindow failed\n");
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  InputCapture capture{};
  glfwSetWindowUserPointer(window, &capture);
  glfwSetScrollCallback(window, scrollCallback);
  glfwSetCharCallback(window, charCallback);

  ui::FontAtlas fontAtlas;
  if (!fontAtlas.buildFromSystemFonts(15.0F, 512, 512)) {
    std::fprintf(stderr, "FontAtlas build failed\n");
  }

  UiGpuResources gpu = createUiGpuResources(fontAtlas);

  ui::UiState uiState{};
  UiModel uiModel{};
  for (int i = 0; i < 60; ++i) {
    char line[128];
    std::snprintf(line, sizeof(line), "[%02d] renderer ready", i);
    uiModel.logs.emplace_back(line);
  }

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      break;
    }

    int fbWidth = 0;
    int fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

    ui::UiInput input = buildUiInput(window, capture);
    uiState.beginFrame();

    ui::UiDrawListOwned drawList = buildUiDrawList(
        static_cast<uint32_t>(fbWidth), static_cast<uint32_t>(fbHeight),
        fontAtlas, input, uiState, uiModel);

    glViewport(0, 0, fbWidth, fbHeight);
    glClearColor(0.06F, 0.07F, 0.09F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    drawDrawListImmediate(drawList.view(), gpu);
    uiState.endFrame(input);

    glfwSwapBuffers(window);
  }

  destroyUiGpuResources(gpu);

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
