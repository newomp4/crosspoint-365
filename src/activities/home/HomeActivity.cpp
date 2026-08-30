#include "HomeActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Utf8.h>
#include <Xtc.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "OpdsServerStore.h"
#include "ReadingStats.h"
#include "RecentBooksStore.h"
#include "WeatherStore.h"
#include "WifiCredentialStore.h"
#include "activities/settings/WeatherRefreshActivity.h"
#include "components/HomeWidgets.h"
#include "components/UITheme.h"
#include "fontIds.h"

int HomeActivity::getMenuItemCount() const {
  int count = 5;  // File Browser, Recents, Focus, File transfer, Settings
  if (!recentBooks.empty()) {
    count += recentBooks.size();
  }
  if (hasOpdsServers) {
    count++;
  }
  return count;
}

void HomeActivity::loadRecentBooks(int maxBooks) {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(std::min(static_cast<int>(books.size()), maxBooks));

  for (const RecentBook& book : books) {
    // Limit to maximum number of recent books
    if (recentBooks.size() >= maxBooks) {
      break;
    }

    // Skip if file no longer exists
    if (RecentBooksStore::isMissing(book)) {
      continue;
    }

    recentBooks.push_back(book);
  }
}

void HomeActivity::loadRecentCovers(int coverHeight) {
  recentsLoading = true;
  bool showingLoading = false;
  Rect popupRect;

  int progress = 0;
  for (RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty()) {
      std::string coverPath = UITheme::getCoverThumbPath(book.coverBmpPath, coverHeight);
      if (!Storage.exists(coverPath.c_str())) {
        // If epub, try to load the metadata for title/author and cover
        if (FsHelpers::hasEpubExtension(book.path)) {
          Epub epub(book.path, "/.crosspoint");
          // Skip loading css since we only need metadata here
          epub.load(false, true);

          // Try to generate thumbnail image for Continue Reading card
          if (!showingLoading) {
            showingLoading = true;
            popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
          }
          GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
          bool success = epub.generateThumbBmp(coverHeight);
          if (!success) {
            RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
            book.coverBmpPath = "";
          }
          coverRendered = false;
          requestUpdate();
        } else if (FsHelpers::hasXtcExtension(book.path)) {
          // Handle XTC file
          Xtc xtc(book.path, "/.crosspoint");
          if (xtc.load()) {
            // Try to generate thumbnail image for Continue Reading card
            if (!showingLoading) {
              showingLoading = true;
              popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
            }
            GUI.fillPopupProgress(renderer, popupRect, 10 + progress * (90 / recentBooks.size()));
            bool success = xtc.generateThumbBmp(coverHeight);
            if (!success) {
              RECENT_BOOKS.updateBook(book.path, book.title, book.author, "");
              book.coverBmpPath = "";
            }
            coverRendered = false;
            requestUpdate();
          }
        }
      }
    }
    progress++;
  }

  recentsLoaded = true;
  recentsLoading = false;
}

void HomeActivity::onEnter() {
  Activity::onEnter();

  hasOpdsServers = OPDS_STORE.hasServers();

  const auto& metrics = UITheme::getInstance().getMetrics();
  loadRecentBooks(metrics.homeRecentBooksCount);

  const auto base = static_cast<int>(recentBooks.size());
  selectorIndex = initialMenuItem == HomeMenuItem::NONE ? 0 : base + menuItemToIndex(initialMenuItem, hasOpdsServers);

  // The second render pass exists only to generate missing cover thumbs (a
  // blocking job that must run after something is on screen). When every
  // cover is already cached — the common case — one paint is enough.
  coversNeedWork = false;
  for (const RecentBook& book : recentBooks) {
    if (!book.coverBmpPath.empty() &&
        !Storage.exists(UITheme::getCoverThumbPath(book.coverBmpPath, metrics.homeCoverHeight).c_str())) {
      coversNeedWork = true;
      break;
    }
  }

  chooseWidgetBand();
  // The widgets and the activity panel read these stores on the render task;
  // load them here first so the initial SD reads don't race the first paint.
  if (widgetBand > 0 || UITheme::getInstance().getMetrics().homeWidgetTiles) READING_STATS.ensureLoaded();
  if (widgetBand > 0 && HomeWidgets::showsWeather()) WEATHER.ensureLoaded();
  maybeAutoRefreshWeather();

  // Trigger first update
  requestUpdate();
}

int HomeActivity::menuTopFor(const int tileTop) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  if (metrics.homeMenuAtBottom) {
    return renderer.getScreenHeight() - metrics.buttonHintsHeight - 8 - GUI.getMenuRowHeight(renderer);
  }
  return tileTop + metrics.homeCoverTileHeight + metrics.homeMenuTopOffset;
}

void HomeActivity::chooseWidgetBand() {
  widgetBand = 0;
  widgetBandCompact = false;
  if (HomeWidgets::slotCount() == 0) return;
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int menuRows =
      getMenuItemCount() - (metrics.homeContinueReadingInMenu ? 0 : static_cast<int>(recentBooks.size()));
  const int menuHeight = metrics.homeMenuHorizontal
                             ? GUI.getMenuRowHeight(renderer)
                             : menuRows * (GUI.getMenuRowHeight(renderer) + metrics.menuSpacing) - metrics.menuSpacing;
  // Space left for the band: everything must clear the button hints, or the
  // pinned bottom menu when the theme has one.
  const int bottomLimit =
      metrics.homeMenuAtBottom ? menuTopFor(0) - 8 : renderer.getScreenHeight() - metrics.buttonHintsHeight - 4;
  const int contentBottomWithoutBand = metrics.homeTopPadding + metrics.homeCoverTileHeight +
                                       (metrics.homeMenuAtBottom ? 0 : metrics.homeMenuTopOffset + menuHeight);
  const int room = bottomLimit - contentBottomWithoutBand;
  const int full = HomeWidgets::bandHeight(renderer, false);
  const int compact = HomeWidgets::bandHeight(renderer, true);
  if (room >= full) {
    widgetBand = full;
  } else if (room >= compact) {
    widgetBand = compact;
    widgetBandCompact = true;
  }
}

// Auto-refresh on wake: only when the user opted in, a weather widget is on
// screen, the data is old, and a saved network exists. The refresh activity
// restarts into Home when it is done, and the store's attempt throttle stops
// a failing network from costing every wake.
void HomeActivity::maybeAutoRefreshWeather() {
  if (weatherAutoTried) return;
  weatherAutoTried = true;
  if (!HomeWidgets::showsWeather() || WIFI_STORE.getCredentialCount() == 0) return;
  WEATHER.ensureLoaded();
  // shouldAutoRefresh carries the staleness + attempt throttle; on top of it,
  // a configured location with no data at all bootstraps its first fetch
  // regardless of the auto-refresh setting — otherwise the widget sits on
  // "no data yet" until the user finds the manual refresh.
  if (!WEATHER.shouldAutoRefresh(halClock.getEpochSeconds())) return;
  if (WEATHER.hasData() && SETTINGS.weatherAutoRefresh != CrossPointSettings::WEATHER_REFRESH_ON_WAKE) return;
  startActivityForResult(std::make_unique<WeatherRefreshActivity>(renderer, mappedInput, /*silent=*/true), nullptr);
}

void HomeActivity::onExit() {
  Activity::onExit();

  // Free the stored cover buffer if any
  freeCoverBuffer();
}

bool HomeActivity::storeCoverBuffer() {
  // render() must have already set the cover rect; without it we'd be back to
  // cloning the whole framebuffer.
  if (coverRectW <= 0 || coverRectH <= 0) return false;
  freeCoverBuffer();
  const size_t needed = renderer.getRegionByteSize(coverRectX, coverRectY, coverRectW, coverRectH);
  if (needed == 0) return false;
  coverBuffer = static_cast<uint8_t*>(malloc(needed));
  if (!coverBuffer) {
    LOG_ERR("HOME", "OOM: cover buffer (%u bytes)", (unsigned)needed);
    return false;
  }
  coverBufferSize = needed;
  if (!renderer.copyRegionToBuffer(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize)) {
    free(coverBuffer);
    coverBuffer = nullptr;
    coverBufferSize = 0;
    return false;
  }
  return true;
}

bool HomeActivity::restoreCoverBuffer() {
  if (!coverBuffer || coverRectW <= 0 || coverRectH <= 0) return false;
  return renderer.copyBufferToRegion(coverRectX, coverRectY, coverRectW, coverRectH, coverBuffer, coverBufferSize);
}

void HomeActivity::freeCoverBuffer() {
  if (coverBuffer) {
    free(coverBuffer);
    coverBuffer = nullptr;
  }
  coverBufferSize = 0;
  coverBufferStored = false;
}

void HomeActivity::loop() {
  const int menuCount = getMenuItemCount();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int tileTop = metrics.homeTopPadding + widgetBand;

  // A clock widget keeps time: repaint when the minute turns over.
  if (widgetBand > 0 && lastClockMinute != 255) {
    uint8_t hour, minute;
    if (halClock.getTime(hour, minute) && minute != lastClockMinute) {
      lastClockMinute = minute;
      requestUpdate();
    }
  }

  auto activateSelection = [this] {
    if (selectorIndex < recentBooks.size()) {
      onSelectBook(recentBooks[selectorIndex].path);
      return;
    }
    const int menuIndex = selectorIndex - static_cast<int>(recentBooks.size());
    flashMenuSelection(menuIndex);
    switch (indexToMenuItem(menuIndex, hasOpdsServers)) {
      case HomeMenuItem::FILE_BROWSER:
        onFileBrowserOpen();
        break;
      case HomeMenuItem::RECENTS:
        onRecentsOpen();
        break;
      case HomeMenuItem::OPDS_BROWSER:
        onOpdsBrowserOpen();
        break;
      case HomeMenuItem::FOCUS_TIMER:
        onPomodoroOpen();
        break;
      case HomeMenuItem::FILE_TRANSFER:
        onFileTransferOpen();
        break;
      case HomeMenuItem::SETTINGS_MENU:
        onSettingsOpen();
        break;
      default:
        break;
    }
  };

  buttonNavigator.onNext([this, menuCount] {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  buttonNavigator.onPrevious([this, menuCount] {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
  });

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up) {
    selectorIndex = ButtonNavigator::nextIndex(selectorIndex, menuCount);
    requestUpdate();
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Down) {
    selectorIndex = ButtonNavigator::previousIndex(selectorIndex, menuCount);
    requestUpdate();
    return;
  }

  // Back is otherwise unused on the home menu: open the most recently read
  // book directly (recentBooks is most-recent-first and already pruned of
  // files missing from the SD card).
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) && !recentBooks.empty()) {
    onSelectBook(recentBooks[0].path);
    return;
  }

  const int coverColumnCount = std::max(1, metrics.homeRecentBooksCount);
  const int recentCount = std::min(static_cast<int>(recentBooks.size()), coverColumnCount);
  const int coverColumnWidth = (renderer.getScreenWidth() - 2 * metrics.contentSidePadding) / coverColumnCount;
  int touchedBook = -1;
  const auto coverTouch = mappedInput.colTouch(touchedBook, metrics.contentSidePadding, coverColumnWidth, recentCount,
                                               tileTop, tileTop + metrics.homeCoverTileHeight, coverColumnWidth);
  if (coverTouch != MappedInputManager::RowTouch::None) {
    if (coverTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != touchedBook) {
        selectorIndex = touchedBook;
        requestUpdate();
      }
    } else {
      selectorIndex = touchedBook;
      activateSelection();
    }
    return;
  }

  const int menuTop = menuTopFor(tileTop);
  const int renderedMenuSelection =
      metrics.homeContinueReadingInMenu ? selectorIndex : selectorIndex - recentBooks.size();
  const int renderedMenuCount =
      menuCount - (metrics.homeContinueReadingInMenu ? 0 : static_cast<int>(recentBooks.size()));
  int menuRow = -1;
  // Row height from the theme, not the metrics table: RoundedRaff draws
  // font-derived rows and the touch grid must match the visuals exactly.
  const int menuRowHeight = GUI.getMenuRowHeight(renderer);
  const auto menuTouch = metrics.homeMenuHorizontal
                             ? mappedInput.colTouch(menuRow, metrics.contentSidePadding,
                                                    (renderer.getScreenWidth() - 2 * metrics.contentSidePadding) /
                                                        std::max(1, renderedMenuCount),
                                                    renderedMenuCount, menuTop, menuTop + menuRowHeight)
                             : mappedInput.rowTouch(menuRow, menuTop, menuRowHeight + metrics.menuSpacing,
                                                    renderedMenuCount, 0, INT32_MAX, menuRowHeight);
  if (menuTouch != MappedInputManager::RowTouch::None) {
    const int touchedIndex =
        metrics.homeContinueReadingInMenu ? menuRow : menuRow + static_cast<int>(recentBooks.size());
    if (menuTouch == MappedInputManager::RowTouch::Down) {
      if (selectorIndex != touchedIndex) {
        selectorIndex = touchedIndex;
        requestUpdate();
      }
    } else {
      selectorIndex = touchedIndex;
      activateSelection();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    activateSelection();
  }
}

void HomeActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  bool bufferRestored = coverBufferStored && restoreCoverBuffer();

  // Band spans topPadding..homeTopPadding: the cover tile starts at the fixed
  // homeTopPadding, so the height must shrink by topPadding or the band (and a
  // centered title, e.g. RoundedRaff's book title) sinks into the tile.
  char headerDate[40] = {0};
  const char* headerTitle = nullptr;
  if (metrics.homeHeaderShowsDate) {
    headerTitle = HomeWidgets::formatHeaderDate(headerDate, sizeof(headerDate)) ? headerDate : tr(STR_HOME_TITLE);
  } else if (metrics.homeContinueReadingInMenu && !recentBooks.empty()) {
    headerTitle = recentBooks[0].title.c_str();
  }
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.homeTopPadding - metrics.topPadding},
                 headerTitle);

  chooseWidgetBand();
  const int tileTop = metrics.homeTopPadding + widgetBand;
  if (widgetBand > 0) {
    HomeWidgets::draw(renderer, Rect{0, metrics.homeTopPadding, pageWidth, widgetBand}, widgetBandCompact);
    uint8_t hour, minute;
    lastClockMinute = (HomeWidgets::showsClock() && halClock.getTime(hour, minute)) ? minute : 255;
  }

  // Record the tile rect so storeCoverBuffer (called from the theme) knows
  // which sub-region of the framebuffer to snapshot. ~16 KB in Portrait
  // instead of the 48 KB full framebuffer the previous bind captured.
  coverRectX = 0;
  coverRectY = tileTop;
  coverRectW = pageWidth;
  coverRectH = metrics.homeCoverTileHeight;

  GUI.drawRecentBookCover(renderer, Rect{0, tileTop, pageWidth, metrics.homeCoverTileHeight}, recentBooks,
                          selectorIndex, coverRendered, coverBufferStored, bufferRestored,
                          std::bind(&HomeActivity::storeCoverBuffer, this));

  // Build menu items dynamically
  // A tab bar gets one-word labels; stacked rows keep the full names.
  const bool tabs = metrics.homeMenuHorizontal;
  std::vector<const char*> menuItems = {
      tabs ? tr(STR_TAB_FILES) : tr(STR_BROWSE_FILES), tabs ? tr(STR_TAB_RECENT) : tr(STR_MENU_RECENT_BOOKS),
      tabs ? tr(STR_TAB_FOCUS) : tr(STR_POMODORO), tabs ? tr(STR_TAB_TRANSFER) : tr(STR_FILE_TRANSFER),
      tabs ? tr(STR_TAB_SETTINGS) : tr(STR_SETTINGS_TITLE)};
  std::vector<UIIcon> menuIcons = {Folder, Recent, Timer, Transfer, Settings};

  if (hasOpdsServers) {
    menuItems.insert(menuItems.begin() + 2, tabs ? tr(STR_TAB_LIBRARY) : tr(STR_OPDS_BROWSER));
    menuIcons.insert(menuIcons.begin() + 2, Library);
  }

  if (metrics.homeContinueReadingInMenu && !recentBooks.empty()) {
    // Insert Continue Reading at the top if enabled in theme
    menuItems.insert(menuItems.begin(), tr(STR_CONTINUE_READING));
    menuIcons.insert(menuIcons.begin(), Book);
  }

  const int menuTop = menuTopFor(tileTop);

  // Tile themes pin the menu to the bottom; whatever is left between the book
  // card and the menu becomes the reading-activity panel (skipped when the
  // band squeezed it below a usable height).
  if (metrics.homeWidgetTiles && metrics.homeMenuAtBottom) {
    const int panelTop = tileTop + metrics.homeCoverTileHeight + 12;
    const int panelHeight = menuTop - 12 - panelTop;
    if (panelHeight >= 64) {
      HomeWidgets::drawActivityPanel(renderer, Rect{0, panelTop, pageWidth, panelHeight});
    }
  }

  GUI.drawButtonMenu(
      renderer, Rect{0, menuTop, pageWidth, pageHeight - menuTop - metrics.buttonHintsHeight},
      static_cast<int>(menuItems.size()),
      metrics.homeContinueReadingInMenu ? selectorIndex : selectorIndex - recentBooks.size(),
      [&menuItems](int index) { return std::string(menuItems[index]); },
      [&menuIcons](int index) { return menuIcons[index]; });

  const auto labels = mappedInput.mapLabels(recentBooks.empty() ? "" : tr(STR_RESUME), tr(STR_SELECT), tr(STR_DIR_UP),
                                            tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(cleanInitialRefresh && !firstRenderDone ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);

  if (!firstRenderDone) {
    firstRenderDone = true;
    if (coversNeedWork) {
      requestUpdate();
    } else {
      recentsLoaded = true;
    }
  } else if (!recentsLoaded && !recentsLoading) {
    recentsLoading = true;
    loadRecentCovers(metrics.homeCoverHeight);
  }
}

void HomeActivity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }

void HomeActivity::onFileBrowserOpen() { activityManager.goToFileBrowser(); }

void HomeActivity::onRecentsOpen() { activityManager.goToRecentBooks(); }

void HomeActivity::onSettingsOpen() { activityManager.goToSettings(); }

void HomeActivity::onFileTransferOpen() { activityManager.goToFileTransfer(); }

void HomeActivity::onPomodoroOpen() { activityManager.goToPomodoro(); }

// Press feedback on the tab bar: fill the activated slot solid for the moment
// the next activity takes to load. A ~120ms fast partial refresh makes the
// press feel acknowledged before the slower full paint lands.
void HomeActivity::flashMenuSelection(const int menuIndex) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  if (!metrics.homeMenuHorizontal || !metrics.homeMenuAtBottom) return;
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int menuCountRendered = getMenuItemCount() - static_cast<int>(recentBooks.size());
  if (menuCountRendered <= 0 || menuIndex < 0 || menuIndex >= menuCountRendered) return;
  const int side = metrics.contentSidePadding;
  const int slotWidth = (pageWidth - 2 * side) / menuCountRendered;
  const int barHeight = GUI.getMenuRowHeight(renderer);
  const int menuTop = pageHeight - metrics.buttonHintsHeight - 8 - barHeight;
  RenderLock lock;
  renderer.fillRoundedRect(side + menuIndex * slotWidth + 2, menuTop, slotWidth - 4, barHeight, 14, Color::Black);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

void HomeActivity::onOpdsBrowserOpen() { activityManager.goToBrowser(); }
