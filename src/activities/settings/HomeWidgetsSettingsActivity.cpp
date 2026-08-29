#include "HomeWidgetsSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <memory>
#include <string>

#include "MappedInputManager.h"
#include "WeatherRefreshActivity.h"
#include "WeatherStore.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
using S = CrossPointSettings;

enum MenuItem {
  ITEM_SLOT_1 = 0,
  ITEM_SLOT_2,
  ITEM_SLOT_3,
  ITEM_SLOT_4,
  ITEM_WEATHER_LOCATION,
  ITEM_WEATHER_UNIT,
  ITEM_WEATHER_AUTO,
  ITEM_WEATHER_REFRESH,
  ITEM_CLOCK_FORMAT,
  ITEM_COUNT
};
static_assert(ITEM_COUNT == HomeWidgetsSettingsActivity::ITEM_COUNT, "keep ITEM_COUNT in sync");

constexpr StrId menuNames[ITEM_COUNT] = {
    StrId::STR_HW_SLOT_1,    StrId::STR_HW_SLOT_2,           StrId::STR_HW_SLOT_3,
    StrId::STR_HW_SLOT_4,    StrId::STR_WEATHER_LOCATION,    StrId::STR_WEATHER_UNIT,
    StrId::STR_WEATHER_AUTO, StrId::STR_WEATHER_REFRESH_NOW, StrId::STR_CLOCK_FORMAT};

constexpr StrId widgetNames[S::HOME_WIDGET_COUNT] = {
    StrId::STR_NONE_OPT,   StrId::STR_HW_CLOCK,      StrId::STR_HW_DATE,    StrId::STR_HW_TODAY,
    StrId::STR_HW_WEEK,    StrId::STR_HW_TOTAL,      StrId::STR_HW_BOOK,    StrId::STR_HW_STREAK,
    StrId::STR_HW_AVERAGE, StrId::STR_YEAR_PROGRESS, StrId::STR_HW_WEATHER, StrId::STR_HW_BATTERY};
constexpr StrId unitNames[S::WEATHER_UNIT_COUNT] = {StrId::STR_WEATHER_CELSIUS, StrId::STR_WEATHER_FAHRENHEIT};
constexpr StrId autoNames[S::WEATHER_AUTO_REFRESH_COUNT] = {StrId::STR_WEATHER_AUTO_MANUAL,
                                                            StrId::STR_WEATHER_AUTO_WAKE};
constexpr StrId clockFormatNames[2] = {StrId::STR_CLOCK_FORMAT_24H, StrId::STR_CLOCK_FORMAT_12H};

uint8_t S::* slotField(const int index) {
  switch (index) {
    case ITEM_SLOT_1:
      return &S::homeWidget1;
    case ITEM_SLOT_2:
      return &S::homeWidget2;
    case ITEM_SLOT_3:
      return &S::homeWidget3;
    default:
      return &S::homeWidget4;
  }
}

const char* enumLabel(const StrId* names, const int count, const uint8_t value) {
  return I18N.get(names[value < count ? value : 0]);
}
}  // namespace

HomeWidgetsSettingsActivity::HomeWidgetsSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("HomeWidgetsSettings", renderer, mappedInput) {}

void HomeWidgetsSettingsActivity::onEnter() {
  UiListActivity::onEnter();
  WEATHER.ensureLoaded();
  for (int i = 0; i < ITEM_COUNT; i++) {
    rowItems_[i].label = I18N.get(menuNames[i]);
    rowItems_[i].actionValue = static_cast<int16_t>(i);
  }
}

const char* HomeWidgetsSettingsActivity::headerTitle() const { return tr(STR_HOME_WIDGETS); }

bool HomeWidgetsSettingsActivity::handleCustomInput() {
  return optionPopup.handleInput(mappedInput, [this] { requestUpdate(); });
}

void HomeWidgetsSettingsActivity::activateIndex(const int index) {
  if (optionPopup.isActive()) return;
  nav.selected = index;
  app.clearTapFlash();
  handleSelection(index);
  requestUpdate();
}

void HomeWidgetsSettingsActivity::showEnumPopup(const StrId titleId, const StrId* names, const int count,
                                                uint8_t CrossPointSettings::* field) {
  const uint8_t current = SETTINGS.*field < count ? SETTINGS.*field : 0;
  optionPopup.show(titleId, names, count, current, [field](const int idx) {
    SETTINGS.*field = static_cast<uint8_t>(idx);
    SETTINGS.saveToFile();
  });
}

void HomeWidgetsSettingsActivity::editLocation() {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_WEATHER_ENTER_CITY),
                                              WEATHER.locationQuery(), WeatherStore::LOCATION_LEN - 1, InputType::Text),
      [](const ActivityResult& result) {
        if (result.isCancelled) return;
        const auto& kb = std::get<KeyboardResult>(result.data);
        WEATHER.setLocationQuery(kb.text.c_str());
      });
}

void HomeWidgetsSettingsActivity::handleSelection(const int index) {
  switch (index) {
    case ITEM_SLOT_1:
    case ITEM_SLOT_2:
    case ITEM_SLOT_3:
    case ITEM_SLOT_4:
      showEnumPopup(menuNames[index], widgetNames, S::HOME_WIDGET_COUNT, slotField(index));
      break;
    case ITEM_WEATHER_LOCATION:
      editLocation();
      break;
    case ITEM_WEATHER_UNIT:
      showEnumPopup(StrId::STR_WEATHER_UNIT, unitNames, S::WEATHER_UNIT_COUNT, &S::weatherUnit);
      break;
    case ITEM_WEATHER_AUTO:
      showEnumPopup(StrId::STR_WEATHER_AUTO, autoNames, S::WEATHER_AUTO_REFRESH_COUNT, &S::weatherAutoRefresh);
      break;
    case ITEM_WEATHER_REFRESH:
      startActivityForResult(std::make_unique<WeatherRefreshActivity>(renderer, mappedInput), nullptr);
      break;
    case ITEM_CLOCK_FORMAT:
      showEnumPopup(StrId::STR_CLOCK_FORMAT, clockFormatNames, 2, &S::clockFormat);
      break;
    default:
      break;
  }
}

std::string HomeWidgetsSettingsActivity::rowValueText(const int index) const {
  const auto& s = SETTINGS;
  switch (index) {
    case ITEM_SLOT_1:
    case ITEM_SLOT_2:
    case ITEM_SLOT_3:
    case ITEM_SLOT_4:
      return enumLabel(widgetNames, S::HOME_WIDGET_COUNT, s.*slotField(index));
    case ITEM_WEATHER_LOCATION:
      return WEATHER.hasLocation() ? WEATHER.locationName() : tr(STR_NOT_SET);
    case ITEM_WEATHER_UNIT:
      return enumLabel(unitNames, S::WEATHER_UNIT_COUNT, s.weatherUnit);
    case ITEM_WEATHER_AUTO:
      return enumLabel(autoNames, S::WEATHER_AUTO_REFRESH_COUNT, s.weatherAutoRefresh);
    case ITEM_WEATHER_REFRESH:
      return WEATHER.hasData() ? tr(STR_WEATHER_HAS_DATA) : tr(STR_NOT_SET);
    case ITEM_CLOCK_FORMAT:
      return enumLabel(clockFormatNames, 2, s.clockFormat);
    default:
      return "";
  }
}

void HomeWidgetsSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  for (int i = 0; i < ITEM_COUNT; i++) {
    rowValues_[i] = rowValueText(i);
    rowItems_[i].value = rowValues_[i].empty() ? nullptr : rowValues_[i].c_str();
  }

  fui::ListProps props;
  props.items = rowItems_;
  props.count = static_cast<uint16_t>(ITEM_COUNT);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}

void HomeWidgetsSettingsActivity::render(RenderLock&& lock) {
  if (optionPopup.processRender(renderer, mappedInput)) return;
  UiListActivity::render(std::move(lock));
}
