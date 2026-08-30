#include "MonoTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <string>

#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/menuIcons.h"
#include "fontIds.h"

namespace {
constexpr int cardPadding = 16;
constexpr int selectionRing = 3;
constexpr int menuIconSize = 32;
constexpr int coverRadius = 8;

const uint8_t* menuIconFor(const UIIcon icon) {
  switch (icon) {
    case UIIcon::Folder:
      return FolderIcon;
    case UIIcon::Book:
      return BookIcon;
    case UIIcon::Recent:
      return RecentIcon;
    case UIIcon::Settings:
      return Settings2Icon;
    case UIIcon::Transfer:
      return TransferIcon;
    case UIIcon::Library:
      return LibraryIcon;
    case UIIcon::Wifi:
      return WifiIcon;
    case UIIcon::Hotspot:
      return HotspotIcon;
    case UIIcon::Bookmark:
      return BookmarkIcon;
    case UIIcon::Timer:
      return TimerIcon;
    default:
      return nullptr;
  }
}
}  // namespace

// Battery strip from the shared header, then the title as a large bold line
// in the band's lower half — the dashboard's "Workouts"-style headline.
void MonoTheme::drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle) const {
  BaseTheme::drawHeader(renderer, rect, nullptr, nullptr);
  const int side = MonoMetrics::values.headerSidePadding;
  if (title != nullptr && title[0] != '\0') {
    const int lineHeight = renderer.getLineHeight(UI_TITLE_FONT_ID);
    const int y = rect.y + rect.height - lineHeight - 6;
    const int maxWidth = rect.width - 2 * side;
    const std::string shown = renderer.truncatedText(UI_TITLE_FONT_ID, title, maxWidth);
    renderer.drawText(UI_TITLE_FONT_ID, rect.x + side, y, shown.c_str(), true);
  }
  if (subtitle != nullptr && subtitle[0] != '\0') {
    const int lineHeight = renderer.getLineHeight(SMALL_FONT_ID);
    const std::string shown = renderer.truncatedText(SMALL_FONT_ID, subtitle, rect.width / 2 - side);
    const int width = renderer.getTextWidth(SMALL_FONT_ID, shown.c_str());
    renderer.drawText(SMALL_FONT_ID, rect.x + rect.width - side - width, rect.y + rect.height - lineHeight - 10,
                      shown.c_str(), true);
  }
}

int MonoTheme::getMenuRowHeight(const GfxRenderer&) const { return MonoMetrics::values.menuRowHeight; }

// One row of icon tabs, evenly spaced; the selected tab sits on a light pill.
void MonoTheme::drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                               const std::function<std::string(int index)>& buttonLabel,
                               const std::function<UIIcon(int index)>& rowIcon) const {
  if (buttonCount <= 0) return;
  const int side = MonoMetrics::values.contentSidePadding;
  const int barHeight = MonoMetrics::values.menuRowHeight;
  const int slotWidth = (rect.width - 2 * side) / buttonCount;
  const int labelHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int contentHeight = menuIconSize + 2 + labelHeight;
  const int contentTop = rect.y + (barHeight - contentHeight) / 2;

  for (int i = 0; i < buttonCount; ++i) {
    const int x = rect.x + side + i * slotWidth;
    if (i == selectedIndex) {
      renderer.fillRoundedRect(x + 2, rect.y, slotWidth - 4, barHeight, cardRadius, Color::LightGray);
    }
    const uint8_t* icon = rowIcon ? menuIconFor(rowIcon(i)) : nullptr;
    if (icon != nullptr) {
      renderer.drawIcon(icon, x + (slotWidth - menuIconSize) / 2, contentTop, menuIconSize);
    }
    const std::string label = renderer.truncatedText(SMALL_FONT_ID, buttonLabel(i).c_str(), slotWidth - 8);
    const int labelWidth = renderer.getTextWidth(SMALL_FONT_ID, label.c_str());
    renderer.drawText(SMALL_FONT_ID, x + (slotWidth - labelWidth) / 2, contentTop + menuIconSize + 2, label.c_str(),
                      true);
  }
}

// "Continue reading" card: cover at the left, title/author beside it, a
// selection ring when focused. The card fill + cover are painted once and
// kept in the cover snapshot; text and ring are redrawn every render.
void MonoTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                    const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                    bool& bufferRestored, std::function<bool()> storeCoverBuffer) const {
  const int side = MonoMetrics::values.contentSidePadding;
  const int cardX = rect.x + side;
  const int cardY = rect.y;
  const int cardWidth = rect.width - 2 * side;
  const int cardHeight = rect.height;
  const int coverHeight = MonoMetrics::values.homeCoverHeight;
  const int coverY = cardY + (cardHeight - coverHeight) / 2;

  if (recentBooks.empty()) {
    renderer.fillRoundedRect(cardX, cardY, cardWidth, cardHeight, cardRadius, Color::LightGray);
    const int lineHeight = renderer.getLineHeight(UI_12_FONT_ID);
    renderer.drawCenteredText(UI_12_FONT_ID, cardY + (cardHeight - lineHeight) / 2, tr(STR_NO_OPEN_BOOK), true,
                              EpdFontFamily::BOLD);
    return;
  }

  const RecentBook& book = recentBooks[0];
  static int coverWidth = 0;
  if (!coverRendered) {
    renderer.fillRoundedRect(cardX, cardY, cardWidth, cardHeight, cardRadius, Color::LightGray);
    bool hasCover = false;
    if (!book.coverBmpPath.empty()) {
      HalFile file;
      if (Storage.openFileForRead("HOME", UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight), file)) {
        Bitmap bitmap(file);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          coverWidth = bitmap.getWidth();
          renderer.drawBitmap(bitmap, cardX + cardPadding, coverY, coverWidth, coverHeight);
          renderer.maskRoundedRectOutsideCorners(cardX + cardPadding, coverY, coverWidth, coverHeight, coverRadius,
                                                 Color::LightGray);
          hasCover = true;
        }
        file.close();
      }
    }
    if (!hasCover) {
      coverWidth = coverHeight * 2 / 3;
      renderer.fillRoundedRect(cardX + cardPadding, coverY, coverWidth, coverHeight, coverRadius, Color::White);
      renderer.drawRoundedRect(cardX + cardPadding, coverY, coverWidth, coverHeight, 2, coverRadius, true);
      renderer.drawIcon(CoverIcon, cardX + cardPadding + (coverWidth - 32) / 2, coverY + (coverHeight - 32) / 2, 32);
    }
    coverBufferStored = storeCoverBuffer();
    coverRendered = coverBufferStored;
  }
  if (coverWidth == 0) coverWidth = coverHeight * 2 / 3;

  const int textX = cardX + cardPadding + coverWidth + cardPadding;
  const int textWidth = cardX + cardWidth - cardPadding - textX;
  const auto titleLines = renderer.wrappedText(UI_12_FONT_ID, book.title.c_str(), textWidth, 3, EpdFontFamily::BOLD);
  const int titleLineHeight = renderer.getLineHeight(UI_12_FONT_ID);
  const int authorHeight = book.author.empty() ? 0 : renderer.getLineHeight(UI_10_FONT_ID) + 4;
  const int hintHeight = renderer.getLineHeight(SMALL_FONT_ID) + 10;
  const int blockHeight = titleLineHeight * static_cast<int>(titleLines.size()) + authorHeight + hintHeight;
  int y = cardY + (cardHeight - blockHeight) / 2;
  for (const auto& line : titleLines) {
    renderer.drawText(UI_12_FONT_ID, textX, y, line.c_str(), true, EpdFontFamily::BOLD);
    y += titleLineHeight;
  }
  if (!book.author.empty()) {
    y += 4;
    const std::string author = renderer.truncatedText(UI_10_FONT_ID, book.author.c_str(), textWidth);
    renderer.drawText(UI_10_FONT_ID, textX, y, author.c_str(), true);
    y += renderer.getLineHeight(UI_10_FONT_ID);
  }
  y += 10;
  renderer.drawText(SMALL_FONT_ID, textX, y, tr(STR_CONTINUE_READING), true);

  if (selectorIndex == 0) {
    renderer.drawRoundedRect(cardX, cardY, cardWidth, cardHeight, selectionRing, cardRadius, true);
  }
}

Rect MonoTheme::drawPopup(const GfxRenderer& renderer, const char* message) const {
  return LyraTheme::drawPopup(renderer, message);
}

// Button hints as key-cap chips: each physical button gets a small dithered
// capsule with its label, sitting over the button it names.
void MonoTheme::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                const char* btn4) const {
  if (gpio.hasTouch()) return;
  const GfxRenderer::Orientation savedOrientation = renderer.getOrientation();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);

  const int pageHeight = renderer.getScreenHeight();
  const int pageWidth = renderer.getScreenWidth();
  constexpr int buttonWidth = 80;
  constexpr int chipHeight = 26;
  constexpr int chipPadX = 10;
  constexpr int narrowButtonPositions[] = {58, 146, 254, 342};
  constexpr int wideButtonPositions[] = {65, 157, 291, 383};
  const int* positions = pageWidth >= 528 ? wideButtonPositions : narrowButtonPositions;
  const char* labels[] = {btn1, btn2, btn3, btn4};
  const int lineHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int top = pageHeight - MonoMetrics::values.buttonHintsHeight;
  const int chipY = top + (MonoMetrics::values.buttonHintsHeight - chipHeight) / 2;
  const int textY = chipY + (chipHeight - lineHeight) / 2;

  for (int i = 0; i < 4; i++) {
    if (labels[i] == nullptr || labels[i][0] == '\0') continue;
    const std::string label = renderer.truncatedText(SMALL_FONT_ID, labels[i], buttonWidth - 2 * chipPadX + 8);
    const int width = renderer.getTextWidth(SMALL_FONT_ID, label.c_str());
    const int chipWidth = std::min(buttonWidth + 8, width + 2 * chipPadX);
    const int chipX = positions[i] + (buttonWidth - chipWidth) / 2;
    renderer.fillRoundedRect(chipX, chipY, chipWidth, chipHeight, chipHeight / 2, Color::LightGray);
    renderer.drawText(SMALL_FONT_ID, positions[i] + (buttonWidth - width) / 2, textY, label.c_str(), true);
  }
  renderer.setOrientation(savedOrientation);
}
