#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace ui {

enum class Axis : uint8_t { Row, Column };

struct EdgesF {
  float l = 0.0F;
  float t = 0.0F;
  float r = 0.0F;
  float b = 0.0F;
};

struct SizeSpec {
  enum class Kind : uint8_t { Fixed, Fill };
  Kind kind = Kind::Fixed;
  float value = 0.0F; // Fixed: px, Fill: weight
};

struct Style {
  Axis axis = Axis::Column;
  SizeSpec width{SizeSpec::Kind::Fill, 1.0F};
  SizeSpec height{SizeSpec::Kind::Fill, 1.0F};

  EdgesF padding{};
  float gap = 0.0F;
  bool clip = false;
};

struct RectF {
  float x = 0.0F;
  float y = 0.0F;
  float w = 0.0F;
  float h = 0.0F;
};

struct Node {
  std::string name;
  Style style;
  std::vector<uint32_t> children;
};

struct UiTree {
  std::vector<Node> nodes;
  uint32_t root = 0;
};

struct LayoutTree {
  std::vector<RectF> rects; // same size as UiTree::nodes
};

inline RectF inset(RectF r, EdgesF p) {
  r.x += p.l;
  r.y += p.t;
  r.w = std::max(0.0F, r.w - (p.l + p.r));
  r.h = std::max(0.0F, r.h - (p.t + p.b));
  return r;
}

inline LayoutTree computeLayout(const UiTree &tree, RectF rootRect) {
  LayoutTree out;
  out.rects.resize(tree.nodes.size());
  if (tree.nodes.empty()) {
    return out;
  }

  struct StackItem {
    uint32_t nodeId = 0;
    RectF rect{};
  };

  std::vector<StackItem> stack;
  stack.push_back({tree.root, rootRect});

  while (!stack.empty()) {
    const StackItem item = stack.back();
    stack.pop_back();

    out.rects[item.nodeId] = item.rect;
    const Node &node = tree.nodes[item.nodeId];

    if (node.children.empty()) {
      continue;
    }

    const RectF content = inset(item.rect, node.style.padding);
    const Axis axis = node.style.axis;

    // 1) measure fixed space and total fill weight along main axis.
    float fixedMain = 0.0F;
    float totalWeight = 0.0F;
    const float gap = std::max(0.0F, node.style.gap);
    const size_t childCount = node.children.size();
    const float totalGap =
        (childCount > 1) ? (gap * static_cast<float>(childCount - 1)) : 0.0F;

    for (uint32_t childId : node.children) {
      const Style &cs = tree.nodes[childId].style;
      const SizeSpec main = (axis == Axis::Row) ? cs.width : cs.height;
      if (main.kind == SizeSpec::Kind::Fixed) {
        fixedMain += std::max(0.0F, main.value);
      } else {
        totalWeight += std::max(0.0F, main.value);
      }
    }

    const float mainAvail = (axis == Axis::Row) ? content.w : content.h;
    const float remaining = std::max(0.0F, mainAvail - fixedMain - totalGap);

    float cursor = (axis == Axis::Row) ? content.x : content.y;

    // 2) arrange children.
    // IMPORTANT:
    // - Geometry placement must follow forward order (node.children order).
    // - Stack push can be reversed to keep traversal stable (LIFO).

    struct ChildPlacement {
      uint32_t nodeId = 0;
      RectF rect{};
    };

    std::vector<ChildPlacement> placements;
    placements.reserve(node.children.size());

    for (uint32_t childId : node.children) {
      const Style &cs = tree.nodes[childId].style;

      const SizeSpec main = (axis == Axis::Row) ? cs.width : cs.height;
      const SizeSpec cross = (axis == Axis::Row) ? cs.height : cs.width;

      float childMain = 0.0F;
      if (main.kind == SizeSpec::Kind::Fixed) {
        childMain = std::max(0.0F, main.value);
      } else {
        const float w = std::max(0.0F, main.value);
        childMain =
            (totalWeight > 0.0F) ? (remaining * (w / totalWeight)) : 0.0F;
      }

      float childCross = 0.0F;
      if (cross.kind == SizeSpec::Kind::Fixed) {
        childCross = std::max(0.0F, cross.value);
      } else {
        childCross = (axis == Axis::Row) ? content.h : content.w;
      }

      RectF childRect{};
      if (axis == Axis::Row) {
        childRect = RectF{cursor, content.y, childMain, childCross};
        cursor += childMain + gap;
      } else {
        childRect = RectF{content.x, cursor, childCross, childMain};
        cursor += childMain + gap;
      }

      placements.push_back(ChildPlacement{childId, childRect});
    }

    // Push in reverse for stable traversal (stack LIFO) without reversing
    // placement.
    for (auto it = placements.rbegin(); it != placements.rend(); ++it) {
      stack.push_back({it->nodeId, it->rect});
    }
  }

  return out;
}

} // namespace ui
