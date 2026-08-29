#include "DotsScreenSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "ClockSyncActivity.h"
#include "DotsScreenPreviewActivity.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
using S = CrossPointSettings;
using Row = DotsScreenSettingsActivity::Row;

// Option labels, indexed by the enum value they stand for.
constexpr StrId shapeNames[S::DOTS_SHAPE_COUNT] = {StrId::STR_DOTS_SHAPE_CIRCLE, StrId::STR_DOTS_SHAPE_SQUARE,
                                                   StrId::STR_DOTS_SHAPE_ROUNDED};
constexpr StrId dotSizeNames[S::DOTS_SIZE_COUNT] = {StrId::STR_DOTS_SIZE_TINY,        StrId::STR_DOTS_SIZE_SMALL,
                                                    StrId::STR_DOTS_SIZE_MEDIUM,      StrId::STR_DOTS_SIZE_LARGE,
                                                    StrId::STR_DOTS_SIZE_EXTRA_LARGE, StrId::STR_DOTS_SIZE_HUGE};
constexpr StrId scaleNames[S::DOTS_SCALE_COUNT] = {StrId::STR_DOTS_SIZE_SMALL, StrId::STR_DOTS_SIZE_MEDIUM,
                                                   StrId::STR_DOTS_SIZE_LARGE};
constexpr StrId backgroundNames[S::DOTS_BACKGROUND_COUNT] = {StrId::STR_DOTS_BG_WHITE, StrId::STR_DOTS_BG_BLACK};
constexpr StrId fontNames[S::DOTS_FONT_COUNT] = {StrId::STR_DOTS_FONT_HELVETICA_NEUE_BOLD,
                                                 StrId::STR_DOTS_FONT_GEIST_BOLD, StrId::STR_DOTS_FONT_GEIST_MEDIUM};
constexpr StrId positionNames[S::DOTS_TEXT_POSITION_COUNT] = {StrId::STR_BOTTOM, StrId::STR_TOP};
constexpr StrId alignNames[S::DOTS_TEXT_ALIGN_COUNT] = {StrId::STR_ALIGN_LEFT, StrId::STR_CENTER,
                                                        StrId::STR_ALIGN_RIGHT};
constexpr StrId orientationNames[S::ORIENTATION_COUNT] = {StrId::STR_PORTRAIT, StrId::STR_LANDSCAPE_CW,
                                                          StrId::STR_ORIENTATION_INVERTED, StrId::STR_LANDSCAPE_CCW};

constexpr StrId yearLayoutNames[S::YEAR_LAYOUT_COUNT] = {StrId::STR_YP_LAYOUT_GRID, StrId::STR_YP_LAYOUT_MONTHS};
constexpr StrId dotStyleNames[S::YEAR_DOT_STYLE_COUNT] = {StrId::STR_YP_STYLE_SOLID, StrId::STR_YP_STYLE_DARK_GRAY,
                                                          StrId::STR_YP_STYLE_LIGHT_GRAY, StrId::STR_YP_STYLE_OUTLINE,
                                                          StrId::STR_YP_STYLE_HIDDEN};
constexpr StrId todayStyleNames[S::YEAR_TODAY_STYLE_COUNT] = {
    StrId::STR_YP_AS_PAST,         StrId::STR_YP_AS_FUTURE,        StrId::STR_YP_STYLE_SOLID,
    StrId::STR_YP_STYLE_DARK_GRAY, StrId::STR_YP_STYLE_LIGHT_GRAY, StrId::STR_YP_STYLE_OUTLINE,
    StrId::STR_YP_STYLE_HIDDEN};
constexpr StrId yearTextNames[S::YEAR_TEXT_COUNT] = {StrId::STR_NONE_OPT,          StrId::STR_DOTS_TEXT_YEAR,
                                                     StrId::STR_YP_TEXT_PERCENT,   StrId::STR_YP_TEXT_DAY_OF_YEAR,
                                                     StrId::STR_YP_TEXT_DAYS_LEFT, StrId::STR_YP_TEXT_FRACTION,
                                                     StrId::STR_DOTS_TEXT_DATE,    StrId::STR_YP_TEXT_FRACTION_PERCENT};

constexpr StrId heatLayoutNames[S::HEAT_LAYOUT_COUNT] = {StrId::STR_HM_LAYOUT_WEEKS, StrId::STR_HM_LAYOUT_GRID};
constexpr StrId weekStartNames[S::WEEK_START_COUNT] = {StrId::STR_HM_MONDAY, StrId::STR_HM_SUNDAY};
constexpr StrId heatScaleNames[S::HEAT_SCALE_COUNT] = {StrId::STR_HM_SCALE_AUTO, StrId::STR_HM_SCALE_LOW,
                                                       StrId::STR_HM_SCALE_MEDIUM, StrId::STR_HM_SCALE_HIGH};
constexpr StrId emptyNames[S::HEAT_EMPTY_COUNT] = {StrId::STR_YP_STYLE_OUTLINE, StrId::STR_YP_STYLE_HIDDEN};
constexpr StrId heatTextNames[S::HEAT_TEXT_COUNT] = {
    StrId::STR_NONE_OPT,          StrId::STR_HM_TEXT_TOTAL,   StrId::STR_HM_TEXT_TODAY,  StrId::STR_HM_TEXT_STREAK,
    StrId::STR_HM_TEXT_DAYS_READ, StrId::STR_HM_TEXT_AVERAGE, StrId::STR_DOTS_TEXT_DATE, StrId::STR_DOTS_TEXT_YEAR};

constexpr Row PREVIEW_ROW = {StrId::STR_PREVIEW, Row::Preview, nullptr, nullptr, 0};
constexpr Row CLOCK_SYNC_ROW = {StrId::STR_CLOCK_SYNC_NOW, Row::ClockSync, nullptr, nullptr, 0};

constexpr Row YEAR_ROWS[] = {
    {StrId::STR_DOTS_COLUMNS, Row::Columns, &S::yearColumns, nullptr, 0},
    {StrId::STR_LAYOUT, Row::Enum, &S::yearLayout, yearLayoutNames, S::YEAR_LAYOUT_COUNT},
    {StrId::STR_DOTS_DOT_SHAPE, Row::Enum, &S::yearDotShape, shapeNames, S::DOTS_SHAPE_COUNT},
    {StrId::STR_DOTS_DOT_SIZE, Row::Enum, &S::yearDotSize, dotSizeNames, S::DOTS_SIZE_COUNT},
    {StrId::STR_YP_PAST_DAYS, Row::Enum, &S::yearPastStyle, dotStyleNames, S::YEAR_DOT_STYLE_COUNT},
    {StrId::STR_YP_TODAY, Row::Enum, &S::yearTodayStyle, todayStyleNames, S::YEAR_TODAY_STYLE_COUNT},
    {StrId::STR_YP_FUTURE_DAYS, Row::Enum, &S::yearFutureStyle, dotStyleNames, S::YEAR_DOT_STYLE_COUNT},
    {StrId::STR_TITLE, Row::Enum, &S::yearTitle, yearTextNames, S::YEAR_TEXT_COUNT},
    {StrId::STR_DOTS_SUBTITLE, Row::Enum, &S::yearSubtitle, yearTextNames, S::YEAR_TEXT_COUNT},
};

constexpr Row HEAT_ROWS[] = {
    {StrId::STR_HM_DAYS, Row::Days, &S::heatDays, nullptr, 0},
    {StrId::STR_LAYOUT, Row::Enum, &S::heatLayout, heatLayoutNames, S::HEAT_LAYOUT_COUNT},
    {StrId::STR_DOTS_COLUMNS, Row::Columns, &S::heatColumns, nullptr, 0},
    {StrId::STR_HM_WEEK_START, Row::Enum, &S::heatWeekStart, weekStartNames, S::WEEK_START_COUNT},
    {StrId::STR_HM_SCALE, Row::Enum, &S::heatScale, heatScaleNames, S::HEAT_SCALE_COUNT},
    {StrId::STR_HM_EMPTY_DAYS, Row::Enum, &S::heatEmptyStyle, emptyNames, S::HEAT_EMPTY_COUNT},
    {StrId::STR_DOTS_DOT_SHAPE, Row::Enum, &S::heatShape, shapeNames, S::DOTS_SHAPE_COUNT},
    {StrId::STR_DOTS_DOT_SIZE, Row::Enum, &S::heatDotSize, dotSizeNames, S::DOTS_SIZE_COUNT},
    {StrId::STR_TITLE, Row::Enum, &S::heatTitle, heatTextNames, S::HEAT_TEXT_COUNT},
    {StrId::STR_DOTS_SUBTITLE, Row::Enum, &S::heatSubtitle, heatTextNames, S::HEAT_TEXT_COUNT},
};

// Styling both screens share (edits the same fields from either editor).
constexpr Row SHARED_ROWS[] = {
    {StrId::STR_DOTS_TITLE_FONT, Row::Enum, &S::dotsTitleFont, fontNames, S::DOTS_FONT_COUNT},
    {StrId::STR_DOTS_TITLE_SIZE, Row::Enum, &S::dotsTitleSize, scaleNames, S::DOTS_SCALE_COUNT},
    {StrId::STR_DOTS_SUBTITLE_FONT, Row::Enum, &S::dotsSubtitleFont, fontNames, S::DOTS_FONT_COUNT},
    {StrId::STR_DOTS_SUBTITLE_SIZE, Row::Enum, &S::dotsSubtitleSize, scaleNames, S::DOTS_SCALE_COUNT},
    {StrId::STR_DOTS_TEXT_POSITION, Row::Enum, &S::dotsTextPosition, positionNames, S::DOTS_TEXT_POSITION_COUNT},
    {StrId::STR_DOTS_TEXT_ALIGN, Row::Enum, &S::dotsTextAlign, alignNames, S::DOTS_TEXT_ALIGN_COUNT},
    {StrId::STR_DOTS_BACKGROUND, Row::Enum, &S::dotsBackground, backgroundNames, S::DOTS_BACKGROUND_COUNT},
    {StrId::STR_DOTS_MARGINS, Row::Enum, &S::dotsMargin, scaleNames, S::DOTS_SCALE_COUNT},
    {StrId::STR_DOTS_ORIENTATION, Row::Enum, &S::dotsOrientation, orientationNames, S::ORIENTATION_COUNT},
};

// Column / day counts offered on device (the settings file accepts any value in range).
constexpr uint8_t COLUMN_PRESETS[] = {4, 5, 6, 7, 8, 10, 12, 14, 15, 16, 18, 20, 24, 26, 28, 31};
constexpr uint8_t DAY_PRESETS[] = {7, 14, 21, 30, 45, 60, 90, 120, 180, 240};

template <size_t N>
constexpr int lengthOf(const uint8_t (&)[N]) {
  return static_cast<int>(N);
}
template <size_t N>
constexpr int lengthOf(const Row (&)[N]) {
  return static_cast<int>(N);
}

static_assert(1 + lengthOf(YEAR_ROWS) + lengthOf(SHARED_ROWS) + 1 <= DotsScreenSettingsActivity::MAX_ITEMS, "");
static_assert(1 + lengthOf(HEAT_ROWS) + lengthOf(SHARED_ROWS) + 1 <= DotsScreenSettingsActivity::MAX_ITEMS, "");

const char* enumLabel(const Row& row) {
  const uint8_t value = SETTINGS.*(row.field);
  return I18N.get(row.names[value < row.count ? value : 0]);
}
}  // namespace

DotsScreenSettingsActivity::DotsScreenSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                       const DotsScreen::Kind kind)
    : UiListActivity("DotsScreenSettings", renderer, mappedInput), screenKind(kind) {}

void DotsScreenSettingsActivity::onEnter() {
  UiListActivity::onEnter();

  const bool year = screenKind == DotsScreen::Kind::YearProgress;
  const Row* own = year ? YEAR_ROWS : HEAT_ROWS;
  const int ownCount = year ? lengthOf(YEAR_ROWS) : lengthOf(HEAT_ROWS);
  rowCount_ = 0;
  rows_[rowCount_++] = PREVIEW_ROW;
  for (int i = 0; i < ownCount; i++) rows_[rowCount_++] = own[i];
  for (const Row& row : SHARED_ROWS) rows_[rowCount_++] = row;
  rows_[rowCount_++] = CLOCK_SYNC_ROW;

  for (int i = 0; i < rowCount_; i++) {
    rowItems_[i].label = I18N.get(rows_[i].label);
    rowItems_[i].actionValue = static_cast<int16_t>(i);
  }
}

const char* DotsScreenSettingsActivity::headerTitle() const {
  return screenKind == DotsScreen::Kind::YearProgress ? tr(STR_YEAR_PROGRESS) : tr(STR_READING_HEATMAP);
}

bool DotsScreenSettingsActivity::handleCustomInput() {
  return optionPopup.handleInput(mappedInput, [this] { requestUpdate(); });
}

void DotsScreenSettingsActivity::activateIndex(const int index) {
  if (optionPopup.isActive()) return;
  nav.selected = index;
  // Activation opens a popup or sub-activity; a lingering flash would gray an
  // unrelated row on the next render.
  app.clearTapFlash();
  handleSelection(index);
  requestUpdate();
}

void DotsScreenSettingsActivity::showEnumPopup(const Row& row) {
  const uint8_t current = SETTINGS.*(row.field) < row.count ? SETTINGS.*(row.field) : 0;
  auto field = row.field;
  optionPopup.show(row.label, row.names, row.count, current, [field](const int idx) {
    SETTINGS.*field = static_cast<uint8_t>(idx);
    SETTINGS.saveToFile();
  });
}

void DotsScreenSettingsActivity::showPresetPopup(const Row& row, const uint8_t* presets, const int presetCount) {
  std::vector<std::string> labels;
  labels.reserve(presetCount);
  int current = 0;
  int bestDistance = 255;
  const int value = SETTINGS.*(row.field);
  for (int i = 0; i < presetCount; i++) {
    labels.push_back(std::to_string(presets[i]));
    const int distance = std::abs(static_cast<int>(presets[i]) - value);
    if (distance < bestDistance) {
      bestDistance = distance;
      current = i;
    }
  }
  auto field = row.field;
  optionPopup.show(row.label, labels, current, [field, presets, presetCount](const int idx) {
    if (idx < 0 || idx >= presetCount) return;
    SETTINGS.*field = presets[idx];
    SETTINGS.saveToFile();
  });
}

void DotsScreenSettingsActivity::handleSelection(const int index) {
  if (index < 0 || index >= rowCount_) return;
  const Row& row = rows_[index];
  switch (row.kind) {
    case Row::Preview:
      startActivityForResult(std::make_unique<DotsScreenPreviewActivity>(renderer, mappedInput, screenKind), nullptr);
      break;
    case Row::Enum:
      showEnumPopup(row);
      break;
    case Row::Columns:
      showPresetPopup(row, COLUMN_PRESETS, lengthOf(COLUMN_PRESETS));
      break;
    case Row::Days:
      showPresetPopup(row, DAY_PRESETS, lengthOf(DAY_PRESETS));
      break;
    case Row::ClockSync:
      startActivityForResult(std::make_unique<ClockSyncActivity>(renderer, mappedInput), nullptr);
      break;
  }
}

std::string DotsScreenSettingsActivity::rowValueText(const int index) const {
  const Row& row = rows_[index];
  switch (row.kind) {
    case Row::Enum:
      return enumLabel(row);
    case Row::Columns:
    case Row::Days:
      return std::to_string(SETTINGS.*(row.field));
    case Row::ClockSync:
      return SETTINGS.clockHasBeenSynced ? tr(STR_CLOCK_SYNCED) : tr(STR_NOT_SET);
    default:
      return "";
  }
}

void DotsScreenSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                      static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  for (int i = 0; i < rowCount_; i++) {
    rowValues_[i] = rowValueText(i);
    rowItems_[i].value = rowValues_[i].empty() ? nullptr : rowValues_[i].c_str();
  }

  fui::ListProps props;
  props.items = rowItems_;
  props.count = static_cast<uint16_t>(rowCount_);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;  // also the explicitly-set marker, see SettingsActivity
  syncListViewport(screen, props);
  screen.list(props);
}

void DotsScreenSettingsActivity::render(RenderLock&& lock) {
  if (optionPopup.processRender(renderer, mappedInput)) return;
  UiListActivity::render(std::move(lock));
}
