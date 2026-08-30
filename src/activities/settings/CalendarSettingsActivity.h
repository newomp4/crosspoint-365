#pragma once
#include <string>

#include "CrossPointSettings.h"
#include "activities/UiListActivity.h"
#include "components/OptionPopup.h"

// Editor for the Calendar sleep screen: the iCal feed URL, how many days the
// screen lists, and the while-asleep refresh cadence.
class CalendarSettingsActivity final : public UiListActivity {
 public:
  CalendarSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  static constexpr int ITEM_COUNT = 5;

  void onEnter() override;
  void render(RenderLock&&) override;

 private:
  OptionPopup optionPopup;

  int listCount() const override { return ITEM_COUNT; }
  const char* headerTitle() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  bool handleCustomInput() override;

  std::string rowValueText(int index) const;
  void handleSelection(int index);
  void editUrl();

  std::string rowValues_[ITEM_COUNT];
  freeink::ui::ListItem rowItems_[ITEM_COUNT]{};
};
