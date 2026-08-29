#include "DotsScreenPreviewActivity.h"

#include <HalDisplay.h>

void DotsScreenPreviewActivity::onEnter() {
  Activity::onEnter();
  requestUpdate();
}

void DotsScreenPreviewActivity::onExit() {
  // The panel shows a grayscale composite and the framebuffer holds plane
  // scratch. Re-seed the controller's baseline planes from a white frame and
  // hand the list underneath a HALF refresh, which drives every pixel to its
  // target regardless of that baseline: one clean pass. Left to itself the
  // driver would instead revert-scrub, full-sync and settle — three flashes.
  renderer.clearScreen();
  renderer.cleanupGrayscaleWithFrameBuffer();
  renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
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
