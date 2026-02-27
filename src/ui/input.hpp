#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ui {

using UiId = uint32_t;
static constexpr UiId INVALID_UI_ID = 0;

struct UiInput {
  float mousePosX = 0.0F;
  float mousePosY = 0.0F;
  bool mouseDown = false;
  bool mousePressed = false;
  bool mouseReleased = false;
  float wheelDelta = 0.0F;

  std::array<bool, 512> keyDown{};
  std::vector<char> charInput;
};

struct UiState {
  UiId hotId = INVALID_UI_ID;
  UiId activeId = INVALID_UI_ID;
  UiId focusId = INVALID_UI_ID;
  UiId lastFrameHotId = INVALID_UI_ID;
  UiId textInputId = INVALID_UI_ID;

  std::unordered_map<UiId, float> floatState;
  std::unordered_map<UiId, uint32_t> uintState;
  std::array<bool, 512> prevKeyDown{};

  void beginFrame() {
    lastFrameHotId = hotId;
    hotId = INVALID_UI_ID;
  }

  [[nodiscard]] bool isKeyPressed(const UiInput &input, int key) const {
    const size_t idx = static_cast<size_t>(key);
    if (idx >= input.keyDown.size()) {
      return false;
    }
    return input.keyDown[idx] && !prevKeyDown[idx];
  }

  void endFrame(const UiInput &input) { prevKeyDown = input.keyDown; }
};

inline UiId hashUiId(std::string_view path, uint32_t index = 0) {
  uint32_t hash = 2166136261U;
  for (const char c : path) {
    hash ^= static_cast<uint8_t>(c);
    hash *= 16777619U;
  }

  hash ^= index;
  hash *= 16777619U;

  if (hash == INVALID_UI_ID) {
    return 1;
  }
  return hash;
}

} // namespace ui
