#pragma once

#include <functional>
#include <vector>

#include "MappedInputManager.h"

class ButtonNavigator final {
  using Callback = std::function<void()>;
  using Buttons = std::vector<MappedInputManager::Button>;

  const uint16_t continuousStartMs;
  const uint16_t continuousIntervalMs;
  uint32_t lastContinuousNavTime = 0;
  static const MappedInputManager* mappedInput;

  [[nodiscard]] bool shouldNavigateContinuously() const;

 public:
  // Start well past a deliberate press: e-ink repaints lag the button, so
  // people naturally hold until they see the change — with a 500 ms start the
  // hold-repeat fired once on top of the press edge and every tap moved two
  // rows. Once repeating, a shorter interval keeps held-scrolling quick.
  explicit ButtonNavigator(const uint16_t continuousIntervalMs = 400, const uint16_t continuousStartMs = 900)
      : continuousStartMs(continuousStartMs), continuousIntervalMs(continuousIntervalMs) {}

  static void setMappedInputManager(const MappedInputManager& mappedInputManager) { mappedInput = &mappedInputManager; }

  void onNext(const Callback& callback);
  void onPrevious(const Callback& callback);
  void onPressAndContinuous(const Buttons& buttons, const Callback& callback);

  void onNextPress(const Callback& callback);
  void onPreviousPress(const Callback& callback);
  void onPress(const Buttons& buttons, const Callback& callback);

  void onNextRelease(const Callback& callback);
  void onPreviousRelease(const Callback& callback);
  void onRelease(const Buttons& buttons, const Callback& callback);

  void onNextContinuous(const Callback& callback);
  void onPreviousContinuous(const Callback& callback);
  void onContinuous(const Buttons& buttons, const Callback& callback);

  [[nodiscard]] static int nextIndex(int currentIndex, int totalItems);
  [[nodiscard]] static int previousIndex(int currentIndex, int totalItems);

  [[nodiscard]] static int nextPageIndex(int currentIndex, int totalItems, int itemsPerPage);
  [[nodiscard]] static int previousPageIndex(int currentIndex, int totalItems, int itemsPerPage);

  // Navigation uses the logical NavNext / NavPrevious buttons; MappedInputManager::mapButton resolves
  // them to physical buttons and applies any orientation-based direction swap, so this stays settings-free.
  [[nodiscard]] static Buttons getNextButtons() { return {MappedInputManager::Button::NavNext}; }
  [[nodiscard]] static Buttons getPreviousButtons() { return {MappedInputManager::Button::NavPrevious}; }
};
