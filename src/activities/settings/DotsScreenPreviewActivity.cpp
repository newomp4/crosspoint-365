#include "DotsScreenPreviewActivity.h"

#include <HalDisplay.h>

void DotsScreenPreviewActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void DotsScreenPreviewActivity::onExit() {
  // The panel holds a grayscale composite and the framebuffer holds plane
  // scratch, so the list underneath repaints with a ghost-clearing waveform
  // instead of a differential update against a baseline that no longer exists.
  renderer.promoteNextRefresh(HalDisplay::FULL_REFRESH);
  Activity::onExit();
}

void DotsScreenPreviewActivity::loop() {
  using Button = MappedInputManager::Button;
  int x = 0;
  int y = 0;
  if (mappedInput.wasReleased(Button::Back) || mappedInput.wasReleased(Button::Confirm) ||
      mappedInput.wasReleased(Button::Left) || mappedInput.wasReleased(Button::Right) ||
      mappedInput.wasReleased(Button::Up) || mappedInput.wasReleased(Button::Down) ||
      mappedInput.wasScreenTapped(x, y)) {
    finish();
  }
}

void DotsScreenPreviewActivity::render(RenderLock&&) {
  // Sleep screens always use normal polarity, whatever night mode says.
  display.setInverted(false);
  DotsScreen::render(renderer, kind, CalendarDate::today());
}
