#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <GLFW/glfw3.h>

#include "ui/drawlist.hpp"
#include "ui/layout.hpp"

namespace {

static constexpr int WINDOW_W = 1100;
static constexpr int WINDOW_H = 700;

struct Color {
  uint8_t r = 255;
  uint8_t g = 255;
  uint8_t b = 255;
  uint8_t a = 255;
};

uint32_t premulPack(Color c) {
  const float af = static_cast<float>(c.a) / 255.0F;
  const uint8_t pr =
      static_cast<uint8_t>(std::lround(static_cast<float>(c.r) * af));
  const uint8_t pg =
      static_cast<uint8_t>(std::lround(static_cast<float>(c.g) * af));
  const uint8_t pb =
      static_cast<uint8_t>(std::lround(static_cast<float>(c.b) * af));
  return ui::packRgbaPremul(pr, pg, pb, c.a);
}

ui::UiScissorRectPx rectToScissor(const ui::RectF &r) {
  // Minimal integerization: floor(min), ceil(max) using floats.
  const float x1f = r.x;
  const float y1f = r.y;
  const float x2f = r.x + r.w;
  const float y2f = r.y + r.h;

  const int x1 = static_cast<int>(std::floor(x1f));
  const int y1 = static_cast<int>(std::floor(y1f));
  const int x2 = static_cast<int>(std::ceil(x2f));
  const int y2 = static_cast<int>(std::ceil(y2f));

  const int w = std::max(0, x2 - x1);
  const int h = std::max(0, y2 - y1);

  return ui::UiScissorRectPx{.x = static_cast<uint32_t>(x1),
                             .y = static_cast<uint32_t>(y1),
                             .width = static_cast<uint32_t>(w),
                             .height = static_cast<uint32_t>(h)};
}

void addRect(ui::UiDrawListOwned &dl, ui::RectF rectPx, uint32_t rgbaPremul,
             ui::UiScissorRectPx scissorPx) {
  const uint32_t baseV = static_cast<uint32_t>(dl.vertices.size());
  const uint32_t baseI = static_cast<uint32_t>(dl.indices.size());

  // 2 triangles, CW/CCW doesn't matter with cull disabled.
  dl.vertices.push_back(
      ui::UiVertex{rectPx.x, rectPx.y, 0.0F, 0.0F, rgbaPremul});
  dl.vertices.push_back(
      ui::UiVertex{rectPx.x + rectPx.w, rectPx.y, 1.0F, 0.0F, rgbaPremul});
  dl.vertices.push_back(ui::UiVertex{rectPx.x + rectPx.w, rectPx.y + rectPx.h,
                                     1.0F, 1.0F, rgbaPremul});
  dl.vertices.push_back(
      ui::UiVertex{rectPx.x, rectPx.y + rectPx.h, 0.0F, 1.0F, rgbaPremul});

  dl.indices.push_back(baseV + 0);
  dl.indices.push_back(baseV + 1);
  dl.indices.push_back(baseV + 2);
  dl.indices.push_back(baseV + 0);
  dl.indices.push_back(baseV + 2);
  dl.indices.push_back(baseV + 3);

  ui::UiDrawCmd cmd{};
  cmd.firstIndex = baseI;
  cmd.indexCount = 6;
  cmd.baseVertex = 0; // indices are absolute here (already include baseV)
  cmd.textureId = 1;  // conceptual "white"
  cmd.textureColorSpace = ui::UiTextureColorSpace::Linear;
  cmd.samplerId = ui::UiSamplerId::LinearClamp;
  cmd.scissor = scissorPx;
  dl.commands.push_back(cmd);
}

ui::UiDrawListOwned buildDemoDrawList(uint32_t w, uint32_t h) {
  ui::UiTree tree;
  tree.nodes.reserve(16);

  auto addNode = [&](const char *name, ui::Style s) -> uint32_t {
    const uint32_t id = static_cast<uint32_t>(tree.nodes.size());
    tree.nodes.push_back(ui::Node{.name = name, .style = s, .children = {}});
    return id;
  };

  ui::Style root{};
  root.axis = ui::Axis::Row;
  root.padding = ui::EdgesF{12.0F, 12.0F, 12.0F, 12.0F};
  root.width = {ui::SizeSpec::Kind::Fixed, static_cast<float>(w)};
  root.height = {ui::SizeSpec::Kind::Fixed, static_cast<float>(h)};
  root.clip = false;

  const uint32_t rootId = addNode("root", root);
  tree.root = rootId;

  ui::Style leftPanel{};
  leftPanel.axis = ui::Axis::Column;
  leftPanel.width = {ui::SizeSpec::Kind::Fixed, 320.0F};
  leftPanel.height = {ui::SizeSpec::Kind::Fill, 1.0F};
  leftPanel.padding = ui::EdgesF{10.0F, 10.0F, 10.0F, 10.0F};
  leftPanel.clip = true;

  ui::Style rightPanel{};
  rightPanel.axis = ui::Axis::Column;
  rightPanel.width = {ui::SizeSpec::Kind::Fill, 1.0F};
  rightPanel.height = {ui::SizeSpec::Kind::Fill, 1.0F};
  rightPanel.padding = ui::EdgesF{10.0F, 10.0F, 10.0F, 10.0F};
  rightPanel.clip = false;

  const uint32_t leftId = addNode("left", leftPanel);
  const uint32_t rightId = addNode("right", rightPanel);

  tree.nodes[rootId].children = {leftId, rightId};

  // Left: fixed header + fill body (clipped).
  ui::Style header{};
  header.axis = ui::Axis::Row;
  header.width = {ui::SizeSpec::Kind::Fill, 1.0F};
  header.height = {ui::SizeSpec::Kind::Fixed, 80.0F};
  header.padding = ui::EdgesF{8.0F, 8.0F, 8.0F, 8.0F};

  ui::Style body{};
  body.axis = ui::Axis::Column;
  body.width = {ui::SizeSpec::Kind::Fill, 1.0F};
  body.height = {ui::SizeSpec::Kind::Fill, 1.0F};
  body.padding = ui::EdgesF{8.0F, 8.0F, 8.0F, 8.0F};
  body.clip = true;

  const uint32_t headerId = addNode("header", header);
  const uint32_t bodyId = addNode("body", body);
  tree.nodes[leftId].children = {headerId, bodyId};

  // Body: 3 rows, middle is fill.
  ui::Style rowFixed{};
  rowFixed.axis = ui::Axis::Row;
  rowFixed.width = {ui::SizeSpec::Kind::Fill, 1.0F};
  rowFixed.height = {ui::SizeSpec::Kind::Fixed, 60.0F};

  ui::Style rowFill = rowFixed;
  rowFill.height = {ui::SizeSpec::Kind::Fill, 1.0F};
  rowFill.clip = true;

  const uint32_t row0 = addNode("row0", rowFixed);
  const uint32_t row1 = addNode("row1", rowFill);
  const uint32_t row2 = addNode("row2", rowFixed);
  tree.nodes[bodyId].children = {row0, row1, row2};

  // Compute layout (px, top-left origin).
  ui::LayoutTree layout =
      ui::computeLayout(tree, ui::RectF{0.0F, 0.0F, static_cast<float>(w),
                                        static_cast<float>(h)});

  ui::UiDrawListOwned dl;
  dl.targetWidthPx = w;
  dl.targetHeightPx = h;

  // Traverse nodes and emit rectangles. Clip is propagated via scissor
  // intersection.
  ui::UiScissorRectPx fullScissor{0, 0, w, h};

  struct StackItem {
    uint32_t nodeId = 0;
    ui::UiScissorRectPx inherited;
    uint32_t depth = 0;
  };

  std::vector<StackItem> stack;
  stack.push_back({tree.root, fullScissor, 0});

  while (!stack.empty()) {
    const StackItem it = stack.back();
    stack.pop_back();

    const ui::RectF rect = layout.rects[it.nodeId];

    // Simple depth-based color to visualize hierarchy.
    const uint8_t base = static_cast<uint8_t>(
        std::max(0, 220 - static_cast<int>(it.depth) * 20));
    Color c{base, static_cast<uint8_t>(base - 30),
            static_cast<uint8_t>(base - 60), 220};
    const uint32_t rgba = premulPack(c);

    ui::UiScissorRectPx nodeScissor = it.inherited;
    const ui::Style &s = tree.nodes[it.nodeId].style;
    if (s.clip) {
      nodeScissor = ui::intersectScissor(nodeScissor, rectToScissor(rect));
    }

    // Emit rect.
    addRect(dl, rect, rgba, nodeScissor);

    // Push children.
    const auto &ch = tree.nodes[it.nodeId].children;
    for (auto rit = ch.rbegin(); rit != ch.rend(); ++rit) {
      stack.push_back({*rit, nodeScissor, it.depth + 1});
    }
  }

  return dl;
}

void drawDrawListImmediate(const ui::UiDrawListView &dl) {
  if (dl.empty()) {
    return;
  }

  const int w = static_cast<int>(dl.targetWidthPx);
  const int h = static_cast<int>(dl.targetHeightPx);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glEnable(GL_SCISSOR_TEST);

  // 2D projection: top-left origin.
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, static_cast<double>(w), static_cast<double>(h), 0.0, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  for (const ui::UiDrawCmd &cmd : dl.commands) {
    // GL scissor uses bottom-left origin.
    const int sx = static_cast<int>(cmd.scissor.x);
    const int sy = h - static_cast<int>(cmd.scissor.y + cmd.scissor.height);
    const int sw = static_cast<int>(cmd.scissor.width);
    const int sh = static_cast<int>(cmd.scissor.height);

    if (sw <= 0 || sh <= 0) {
      continue;
    }
    glScissor(sx, sy, sw, sh);

    // Draw indexed triangles via immediate mode:
    // Read indices, fetch vertices, emit glVertex.
    glBegin(GL_TRIANGLES);
    const uint32_t start = cmd.firstIndex;
    const uint32_t end = start + cmd.indexCount;

    for (uint32_t i = start; i < end; ++i) {
      const uint32_t vi = dl.indices[i];
      const ui::UiVertex &v = dl.vertices[vi];

      const uint32_t rgba = v.rgbaPremul;
      const float r = static_cast<float>((rgba >> 0U) & 0xFFU) / 255.0F;
      const float g = static_cast<float>((rgba >> 8U) & 0xFFU) / 255.0F;
      const float b = static_cast<float>((rgba >> 16U) & 0xFFU) / 255.0F;
      const float a = static_cast<float>((rgba >> 24U) & 0xFFU) / 255.0F;

      glColor4f(r, g, b, a);
      glVertex2f(v.x, v.y);
    }
    glEnd();
  }

  glDisable(GL_SCISSOR_TEST);
}

} // namespace

int main() {
  if (glfwInit() == GLFW_FALSE) {
    std::fprintf(stderr, "glfwInit failed\n");
    return 1;
  }

  // Compatibility context (legacy rendering).
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

  GLFWwindow *window = glfwCreateWindow(WINDOW_W, WINDOW_H,
                                        "ui_layout_glfw_min", nullptr, nullptr);
  if (window == nullptr) {
    std::fprintf(stderr, "glfwCreateWindow failed\n");
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  ui::UiDrawListOwned dl = buildDemoDrawList(WINDOW_W, WINDOW_H);

  while (glfwWindowShouldClose(window) == GLFW_FALSE) {
    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      break;
    }
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
      dl = buildDemoDrawList(WINDOW_W, WINDOW_H);
    }

    glViewport(0, 0, WINDOW_W, WINDOW_H);
    glClearColor(0.08F, 0.08F, 0.09F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);

    drawDrawListImmediate(dl.view());

    glfwSwapBuffers(window);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
