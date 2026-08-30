#pragma once
#include <string>

#include "CrossPointSettings.h"
#include "activities/UiListActivity.h"
#include "components/OptionPopup.h"

// Editor for the home-screen widget row: what each of the four slots shows,
// plus the weather location, unit and refresh policy the weather widget uses.
class HomeWidgetsSettingsActivity final : public UiListActivity {
 public:
  HomeWidgetsSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  static constexpr int ITEM_COUNT = 9;

  void onEnter() override;
  void render(RenderLock&&) override;

 private:
  OptionPopup optionPopup;

  int listCount() const override { return ITEM_COUNT; }
  void drawFooter() override;
  const char* headerTitle() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  bool handleCustomInput() override;

  std::string rowValueText(int index) const;
  void handleSelection(int index);
  void showEnumPopup(StrId titleId, const StrId* names, int count, uint8_t CrossPointSettings::* field);
  void editLocation();

  std::string rowValues_[ITEM_COUNT];
  freeink::ui::ListItem rowItems_[ITEM_COUNT]{};
};
