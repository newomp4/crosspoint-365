#pragma once
#include <string>

#include "CrossPointSettings.h"
#include "activities/UiListActivity.h"
#include "components/DotsScreen.h"
#include "components/OptionPopup.h"

// Editor for the Year Progress and Reading Heatmap sleep screens (one instance
// per screen): the screen's own grid and text rows, the styling both share, a
// full-screen preview, and a shortcut to the clock sync both depend on.
class DotsScreenSettingsActivity final : public UiListActivity {
 public:
  DotsScreenSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, DotsScreen::Kind kind);

  static constexpr int MAX_ITEMS = 24;

  // One list row. Enum rows edit `field` through a popup of `names`; Columns
  // and Days rows edit a numeric field through a popup of presets.
  struct Row {
    enum Kind : uint8_t { Preview, Enum, Columns, Days, ClockSync };
    StrId label;
    Kind kind;
    uint8_t CrossPointSettings::* field;
    const StrId* names;
    uint8_t count;
  };

  void onEnter() override;
  void render(RenderLock&&) override;

 private:
  const DotsScreen::Kind screenKind;
  OptionPopup optionPopup;
  Row rows_[MAX_ITEMS]{};
  int rowCount_ = 0;

  int listCount() const override { return rowCount_; }
  const char* headerTitle() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  bool handleCustomInput() override;

  std::string rowValueText(int index) const;
  void handleSelection(int index);
  void showEnumPopup(const Row& row);
  void showPresetPopup(const Row& row, const uint8_t* presets, int presetCount);

  // Fixed-capacity row storage (see StatusBarSettingsActivity): labels are set
  // once in onEnter(), buildScreen() only refreshes the value text in place.
  std::string rowValues_[MAX_ITEMS];
  freeink::ui::ListItem rowItems_[MAX_ITEMS]{};
};
