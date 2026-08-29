#pragma once

#include "activities/Activity.h"
#include "components/DotsScreen.h"

// Full-screen preview of a dot-grid sleep screen, drawn exactly as the sleep
// path draws it. Any button or tap returns to the settings list.
class DotsScreenPreviewActivity final : public Activity {
 public:
  DotsScreenPreviewActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const DotsScreen::Kind kind)
      : Activity("DotsScreenPreview", renderer, mappedInput), kind(kind) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  const DotsScreen::Kind kind;
};
