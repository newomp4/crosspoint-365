#pragma once
#include <ArduinoJson.h>
#include <Epub/ReaderRenderSpec.h>
#include <PersistableStore.h>

#include <cstdint>

class CrossPointSettings : public PersistableStore<CrossPointSettings> {
 private:
  // Private constructor for singleton
  CrossPointSettings() = default;

  friend class PersistableStore<CrossPointSettings>;

 public:
  enum SLEEP_SCREEN_MODE {
    DARK = 0,
    LIGHT = 1,
    CUSTOM = 2,
    COVER = 3,
    COVER_CUSTOM = 4,
    BLANK = 5,
    QUICK_RESUME = 6,
    TRANSPARENT_CUSTOM = 7,
    YEAR_PROGRESS = 8,
    READING_HEATMAP = 9,
    CALENDAR_VIEW = 10,
    SLEEP_SCREEN_MODE_COUNT
  };
  enum SLEEP_SCREEN_COVER_MODE { FIT = 0, CROP = 1, SLEEP_SCREEN_COVER_MODE_COUNT };
  enum SLEEP_SCREEN_COVER_FILTER {
    NO_FILTER = 0,
    BLACK_AND_WHITE = 1,
    INVERTED_BLACK_AND_WHITE = 2,
    SLEEP_SCREEN_COVER_FILTER_COUNT
  };
  enum STATUS_BAR_PROGRESS_BAR {
    BOOK_PROGRESS = 0,
    CHAPTER_PROGRESS = 1,
    HIDE_PROGRESS = 2,
    STATUS_BAR_PROGRESS_BAR_COUNT
  };
  enum STATUS_BAR_PROGRESS_BAR_THICKNESS {
    PROGRESS_BAR_THIN = 0,
    PROGRESS_BAR_NORMAL = 1,
    PROGRESS_BAR_THICK = 2,
    STATUS_BAR_PROGRESS_BAR_THICKNESS_COUNT
  };
  enum STATUS_BAR_TITLE { BOOK_TITLE = 0, CHAPTER_TITLE = 1, HIDE_TITLE = 2, STATUS_BAR_TITLE_COUNT };
  enum XTC_STATUS_BAR_MODE {
    XTC_STATUS_BAR_HIDE = 0,
    XTC_STATUS_BAR_BOTTOM = 1,
    XTC_STATUS_BAR_TOP = 2,
    XTC_STATUS_BAR_MODE_COUNT
  };

  enum STATUS_BAR_CLOCK_MODE {
    STATUS_BAR_CLOCK_HIDE = 0,
    STATUS_BAR_CLOCK_RIGHT = 1,
    STATUS_BAR_CLOCK_LEFT = 2,
    STATUS_BAR_CLOCK_MODE_COUNT
  };

  enum ORIENTATION {
    PORTRAIT = 0,       // 480x800 logical coordinates (current default)
    LANDSCAPE_CW = 1,   // 800x480 logical coordinates, rotated 180° (swap top/bottom)
    INVERTED = 2,       // 480x800 logical coordinates, inverted
    LANDSCAPE_CCW = 3,  // 800x480 logical coordinates, native panel orientation
    ORIENTATION_COUNT
  };

  // Front button layout options (legacy)
  // Default: Back, Confirm, Left, Right
  // Swapped: Left, Right, Back, Confirm
  enum FRONT_BUTTON_LAYOUT {
    BACK_CONFIRM_LEFT_RIGHT = 0,
    LEFT_RIGHT_BACK_CONFIRM = 1,
    LEFT_BACK_CONFIRM_RIGHT = 2,
    BACK_CONFIRM_RIGHT_LEFT = 3,
    FRONT_BUTTON_LAYOUT_COUNT
  };

  // Front button hardware identifiers (for remapping)
  enum FRONT_BUTTON_HARDWARE {
    FRONT_HW_BACK = 0,
    FRONT_HW_CONFIRM = 1,
    FRONT_HW_LEFT = 2,
    FRONT_HW_RIGHT = 3,
    FRONT_BUTTON_HARDWARE_COUNT
  };

  // Side button layout options
  // Default: Up = Previous, Down = Next
  enum SIDE_BUTTON_LAYOUT { PREV_NEXT = 0, NEXT_PREV = 1, SIDE_BUTTONS_DISABLED = 2, SIDE_BUTTON_LAYOUT_COUNT };

  // Font family options (built-in fonts only; SD card fonts use sdFontFamilyName)
  enum FONT_FAMILY { NOTOSERIF = 0, NOTOSANS = 1, FONT_FAMILY_COUNT };
  static constexpr uint8_t LEGACY_OPENDYSLEXIC = 2;
  static constexpr uint8_t BUILTIN_FONT_COUNT = FONT_FAMILY_COUNT;
  // Reader font size is a point size, not an enum slot — see fontPointSize.
  // Legacy 1.4-and-earlier files stored a 0..3 SMALL/MEDIUM/LARGE/EXTRA_LARGE
  // slot; fromJson() folds that range up (see LEGACY_FONT_SIZE_MAX).
  static constexpr uint8_t LEGACY_FONT_SIZE_MAX = 3;
  static constexpr uint8_t DEFAULT_FONT_POINT_SIZE = 14;
  enum LINE_COMPRESSION { TIGHT = 0, NORMAL = 1, WIDE = 2, EXTRA_WIDE = 3, LINE_COMPRESSION_COUNT };
  enum PARAGRAPH_ALIGNMENT {
    JUSTIFIED = 0,
    LEFT_ALIGN = 1,
    CENTER_ALIGN = 2,
    RIGHT_ALIGN = 3,
    BOOK_STYLE = 4,
    PARAGRAPH_ALIGNMENT_COUNT
  };

  // Auto-sleep timeout options (in minutes)
  enum SLEEP_TIMEOUT {
    SLEEP_1_MIN = 0,
    SLEEP_5_MIN = 1,
    SLEEP_10_MIN = 2,
    SLEEP_15_MIN = 3,
    SLEEP_30_MIN = 4,
    SLEEP_TIMEOUT_COUNT
  };

  // E-ink refresh frequency (pages between full refreshes).
  enum REFRESH_FREQUENCY {
    REFRESH_1 = 0,
    REFRESH_5 = 1,
    REFRESH_10 = 2,
    REFRESH_15 = 3,
    REFRESH_30 = 4,
    REFRESH_NEVER = 5,
    REFRESH_FREQUENCY_COUNT
  };

  // Short power button press actions
  enum SHORT_PWRBTN {
    IGNORE = 0,
    SLEEP = 1,
    PAGE_TURN = 2,
    FORCE_REFRESH = 3,
    FOOTNOTES = 4,
    PWR_CONFIRM = 5,
    SHORT_PWRBTN_COUNT
  };

  // Long-press Confirm action while reading an EPUB. The setting cycles through these values.
  // Persisted in settings.json by index: any new function (e.g. dictionary, bookmark) MUST use a
  // value >= 2 and be appended at the END of the enumValues array in SettingsList.h, otherwise the
  // stored indices shift and existing saves are silently misinterpreted.
  enum LONG_PRESS_MENU_FUNCTION {
    LP_MENU_KOSYNC = 0,
    LP_MENU_DISABLED = 1,
    LP_MENU_BOOKMARK = 2,
    LP_MENU_DICTIONARY = 3,
    LP_MENU_READER_MENU = 4,
    LONG_PRESS_MENU_FUNCTION_COUNT
  };

  // Hide battery percentage
  enum HIDE_BATTERY_PERCENTAGE { HIDE_NEVER = 0, HIDE_READER = 1, HIDE_ALWAYS = 2, HIDE_BATTERY_PERCENTAGE_COUNT };

  // Page turn button long press behavior
  enum LONG_PRESS_BUTTON_BEHAVIOR {
    OFF = 0,
    CHAPTER_SKIP = 1,
    ORIENTATION_CHANGE = 2,
    LONG_PRESS_BUTTON_BEHAVIOR_COUNT
  };

  // UI Theme
  enum UI_THEME { CLASSIC = 0, LYRA = 1, LYRA_3_COVERS = 2, ROUNDEDRAFF = 3, MONO = 4 };
  // Typeface behind every UI font slot (menus, lists, headers, hints).
  enum UI_FONT { UI_FONT_GEIST = 0, UI_FONT_UBUNTU = 1, UI_FONT_COUNT };

  // Image rendering in EPUB reader
  enum IMAGE_RENDERING { IMAGES_DISPLAY = 0, IMAGES_PLACEHOLDER = 1, IMAGES_SUPPRESS = 2, IMAGE_RENDERING_COUNT };

  // How Select opens the reader menu: the classic full-screen list, or a toolbar
  // overlay (top/bottom bars with Contents / Text / More bottom-sheet panels)
  // painted over the page.
  enum READER_MENU_STYLE { READER_MENU_LIST = 0, READER_MENU_TOOLBAR = 1, READER_MENU_STYLE_COUNT };

  enum TILT_PAGE_TURN { TILT_OFF = 0, TILT_NORMAL = 1, TILT_NVERTED = 2, TILT_PAGE_TURN_COUNT };

  enum TOUCH_READER_CONTROLS {
    TOUCH_READER_OFF = 0,
    TOUCH_READER_ON = 1,
    TOUCH_READER_SWIPE = 2,
    TOUCH_READER_INVERTED_TAP = 3,
    TOUCH_READER_CONTROLS_COUNT
  };

  // How the reader menu opens on touch boards. Persisted under the legacy
  // "tapForReaderMenu" key: 0/1 keep their old Off/Tap meaning.
  enum SHOW_READER_MENU { READER_MENU_OFF = 0, READER_MENU_TAP = 1, READER_MENU_SWIPE_UP = 2, SHOW_READER_MENU_COUNT };

  enum QUICK_RESUME_SLEEP_SCREEN {
    QUICK_RESUME_NEVER = 0,
    QUICK_RESUME_AFTER_TIMEOUT = 1,
    QUICK_RESUME_SLEEP_SCREEN_COUNT
  };

  // Year Progress / Reading Heatmap sleep screens (DotsScreen). Persisted by
  // value, so new options go at the END of each enum.
  enum DOTS_SHAPE { DOTS_SHAPE_CIRCLE = 0, DOTS_SHAPE_SQUARE = 1, DOTS_SHAPE_ROUNDED = 2, DOTS_SHAPE_COUNT };
  enum DOTS_SIZE {
    DOTS_SIZE_TINY = 0,
    DOTS_SIZE_SMALL = 1,
    DOTS_SIZE_MEDIUM = 2,
    DOTS_SIZE_LARGE = 3,
    DOTS_SIZE_EXTRA_LARGE = 4,
    DOTS_SIZE_HUGE = 5,
    DOTS_SIZE_COUNT
  };
  // Shared Small / Medium / Large scale for margins and text sizes.
  enum DOTS_SCALE { DOTS_SCALE_SMALL = 0, DOTS_SCALE_MEDIUM = 1, DOTS_SCALE_LARGE = 2, DOTS_SCALE_COUNT };
  enum DOTS_BACKGROUND { DOTS_BG_WHITE = 0, DOTS_BG_BLACK = 1, DOTS_BACKGROUND_COUNT };
  enum DOTS_FONT {
    DOTS_FONT_HELVETICA_NEUE_BOLD = 0,
    DOTS_FONT_GEIST_BOLD = 1,
    DOTS_FONT_GEIST_MEDIUM = 2,
    DOTS_FONT_COUNT
  };
  enum DOTS_TEXT_POSITION { DOTS_TEXT_BOTTOM = 0, DOTS_TEXT_TOP = 1, DOTS_TEXT_POSITION_COUNT };
  enum DOTS_TEXT_ALIGN { DOTS_ALIGN_LEFT = 0, DOTS_ALIGN_CENTER = 1, DOTS_ALIGN_RIGHT = 2, DOTS_TEXT_ALIGN_COUNT };

  enum YEAR_LAYOUT { YEAR_LAYOUT_GRID = 0, YEAR_LAYOUT_MONTHS = 1, YEAR_LAYOUT_COUNT };
  enum YEAR_DOT_STYLE {
    YEAR_DOT_SOLID = 0,
    YEAR_DOT_DARK_GRAY = 1,
    YEAR_DOT_LIGHT_GRAY = 2,
    YEAR_DOT_OUTLINE = 3,
    YEAR_DOT_HIDDEN = 4,
    YEAR_DOT_STYLE_COUNT
  };
  // Today's dot: follow one of the neighbouring classes, or any YEAR_DOT_STYLE
  // (the tail of this enum mirrors YEAR_DOT_STYLE in order).
  enum YEAR_TODAY_STYLE {
    YEAR_TODAY_AS_PAST = 0,
    YEAR_TODAY_AS_FUTURE = 1,
    YEAR_TODAY_SOLID = 2,
    YEAR_TODAY_DARK_GRAY = 3,
    YEAR_TODAY_LIGHT_GRAY = 4,
    YEAR_TODAY_OUTLINE = 5,
    YEAR_TODAY_HIDDEN = 6,
    YEAR_TODAY_STYLE_COUNT
  };
  enum YEAR_TEXT {
    YEAR_TEXT_NONE = 0,
    YEAR_TEXT_YEAR = 1,
    YEAR_TEXT_PERCENT = 2,
    YEAR_TEXT_DAY_OF_YEAR = 3,
    YEAR_TEXT_DAYS_LEFT = 4,
    YEAR_TEXT_FRACTION = 5,
    YEAR_TEXT_DATE = 6,
    YEAR_TEXT_FRACTION_PERCENT = 7,
    YEAR_TEXT_COUNT
  };

  enum HEAT_LAYOUT { HEAT_LAYOUT_WEEKS = 0, HEAT_LAYOUT_GRID = 1, HEAT_LAYOUT_COUNT };
  enum WEEK_START { WEEK_START_MONDAY = 0, WEEK_START_SUNDAY = 1, WEEK_START_COUNT };
  // Shade thresholds: relative to the busiest day, or fixed minute steps.
  enum HEAT_SCALE {
    HEAT_SCALE_AUTO = 0,
    HEAT_SCALE_LOW = 1,
    HEAT_SCALE_MEDIUM = 2,
    HEAT_SCALE_HIGH = 3,
    HEAT_SCALE_COUNT
  };
  enum HEAT_EMPTY { HEAT_EMPTY_OUTLINE = 0, HEAT_EMPTY_HIDDEN = 1, HEAT_EMPTY_COUNT };
  // Home-screen widgets (HomeWidgets), one per slot. Persisted by value: new
  // widgets go at the END.
  enum HOME_WIDGET {
    HW_NONE = 0,
    HW_CLOCK = 1,
    HW_DATE = 2,
    HW_TODAY = 3,
    HW_WEEK = 4,
    HW_TOTAL = 5,
    HW_BOOK = 6,
    HW_STREAK = 7,
    HW_AVERAGE = 8,
    HW_YEAR = 9,
    HW_WEATHER = 10,
    HW_BATTERY = 11,
    HOME_WIDGET_COUNT
  };
  enum WEATHER_UNIT { WEATHER_CELSIUS = 0, WEATHER_FAHRENHEIT = 1, WEATHER_UNIT_COUNT };
  // Manual: only "Refresh weather now" and opportunistic updates while Wi-Fi is
  // up anyway. On wake: also bring Wi-Fi up at boot when the data is old.
  enum WEATHER_AUTO_REFRESH { WEATHER_REFRESH_MANUAL = 0, WEATHER_REFRESH_ON_WAKE = 1, WEATHER_AUTO_REFRESH_COUNT };

  // Calendar sleep screen: how many upcoming days to list.
  enum CAL_DAYS { CAL_DAYS_TODAY = 0, CAL_DAYS_3 = 1, CAL_DAYS_WEEK = 2, CAL_DAYS_COUNT };
  // Refresh cadence while the Calendar sleep screen is up. Anything but Off
  // holds the device in timed light sleep (the X3 cuts battery power in deep
  // sleep, so a deep-sleep timer could never fire) and re-fetches on schedule.
  enum CAL_SLEEP_REFRESH {
    CAL_REFRESH_OFF = 0,
    CAL_REFRESH_10M = 1,
    CAL_REFRESH_30M = 2,
    CAL_REFRESH_1H = 3,
    CAL_SLEEP_REFRESH_COUNT
  };

  enum HEAT_TEXT {
    HEAT_TEXT_NONE = 0,
    HEAT_TEXT_TOTAL_TIME = 1,
    HEAT_TEXT_TODAY_TIME = 2,
    HEAT_TEXT_STREAK = 3,
    HEAT_TEXT_DAYS_READ = 4,
    HEAT_TEXT_AVERAGE = 5,
    HEAT_TEXT_DATE = 6,
    HEAT_TEXT_YEAR = 7,
    HEAT_TEXT_COUNT
  };

  // Sleep screen settings
  uint8_t sleepScreen = DARK;
  // Night mode: inverted output polarity, applied to every activity per
  // render by ActivityManager. The sleep screen opts out itself.
  uint8_t screenInverted = 0;
  // Sleep screen cover mode settings
  uint8_t sleepScreenCoverMode = FIT;
  // Sleep screen cover filter
  uint8_t sleepScreenCoverFilter = NO_FILTER;
  // Status bar settings
  uint8_t statusBarChapterPageCount = 1;
  uint8_t statusBarBookProgressPercentage = 1;
  uint8_t statusBarProgressBar = HIDE_PROGRESS;
  uint8_t statusBarProgressBarThickness = PROGRESS_BAR_NORMAL;
  uint8_t statusBarTitle = CHAPTER_TITLE;
  uint8_t statusBarBattery = 1;
  uint8_t xtcStatusBarMode = XTC_STATUS_BAR_HIDE;
  // Clock display in status bar (X3 only, requires DS3231 RTC)
  uint8_t statusBarClock = STATUS_BAR_CLOCK_HIDE;
  // Clock UTC offset in quarter-hour steps, biased by 48 so it fits in uint8_t.
  // Value 48 = UTC+0, 0 = UTC-12:00, 104 = UTC+14:00.
  // Quarter-hour granularity supports oddball zones like Nepal (+5:45) and Chatham (+12:45).
  uint8_t clockUtcOffsetQ = 48;
  // Clock display format: 0 = 24-hour, 1 = 12-hour
  uint8_t clockFormat = 0;
  // Set once an NTP sync succeeds. Used to skip re-syncing on every WiFi connect.
  // Resetting to 0 (e.g. via the web UI) forces a re-sync on next WiFi connect.
  uint8_t clockHasBeenSynced = 0;
  // Text rendering settings
  uint8_t extraParagraphSpacing = 1;
  uint8_t textAntiAliasing = 1;
  // Short power button click behaviour
  uint8_t shortPwrBtn = IGNORE;
  // EPUB reading orientation settings
  // 0 = portrait (default), 1 = landscape clockwise, 2 = inverted, 3 = landscape counter-clockwise
  uint8_t orientation = PORTRAIT;
  // Button layouts (front layout retained for migration only)
  uint8_t frontButtonLayout = BACK_CONFIRM_LEFT_RIGHT;
  uint8_t sideButtonLayout = PREV_NEXT;
  uint8_t frontButtonFollowOrientation = 0;
  // Front button remap (logical -> hardware)
  // Used by MappedInputManager to translate logical buttons into physical front buttons.
  uint8_t frontButtonBack = FRONT_HW_BACK;
  uint8_t frontButtonConfirm = FRONT_HW_CONFIRM;
  uint8_t frontButtonLeft = FRONT_HW_LEFT;
  uint8_t frontButtonRight = FRONT_HW_RIGHT;
  // Reader font settings
  uint8_t fontFamily = NOTOSERIF;
  // Point size of the reader font. Only sizes the active family actually ships
  // are selectable; SdCardFontSystem::ensureLoaded() snaps this to the nearest
  // available size (and persists the snap) whenever the family changes.
  uint8_t fontPointSize = DEFAULT_FONT_POINT_SIZE;
  uint8_t lineSpacing = NORMAL;
  uint8_t paragraphAlignment = JUSTIFIED;
  // Auto-sleep timeout setting (default 10 minutes). Legacy sleepTimeout enum values are migration-only.
  uint8_t sleepTimeoutMinutes = 10;
  // E-ink refresh frequency (default 15 pages)
  uint8_t refreshFrequency = REFRESH_15;
  uint8_t hyphenationEnabled = 0;

  // Reader screen margin settings
  static constexpr uint8_t SCREEN_MARGIN_MIN = 5;
  static constexpr uint8_t SCREEN_MARGIN_MAX = 40;
  static constexpr uint8_t SCREEN_MARGIN_STEP = 5;
  uint8_t screenMargin = SCREEN_MARGIN_MIN;
  // OPDS download destination folder ("" = SD root). Global; edited from the
  // OPDS server list. Persisted via a category-less SettingInfo::String in
  // SettingsList.h, so it stays out of the on-device Settings screen.
  char opdsDownloadFolder[64] = "";
  // On-disk filename format for OPDS downloads (0=Author-Title default, 1=Title-Author,
  // 2=Title). See OpdsFilenameFormat. Persisted via a category-less SettingInfo::Enum,
  // edited from the OPDS server list; hidden from the on-device Settings screen.
  uint8_t opdsFilenameFormat = 0;
  // Hide battery percentage
  uint8_t hideBatteryPercentage = HIDE_NEVER;
  // Long-press page turn button behavior
  uint8_t longPressButtonBehavior = OFF;
  // Long-press Confirm function in EPUB reader (cycles through LONG_PRESS_MENU_FUNCTION values).
  // Defaults to Disabled so shortcut-based bookmark toggling remains opt-in.
  uint8_t longPressMenuFunction = LP_MENU_DISABLED;
  // UI Theme
  uint8_t uiTheme = MONO;
  uint8_t uiFont = UI_FONT_GEIST;
  // Settings files written before this fork carry uiTheme=Lyra by default;
  // the first load switches them to Mono once (see fromJson).
  uint8_t forkThemeMigrated = 0;
  // Sunlight fading compensation
  uint8_t fadingFix = 0;
  // Power button return from footnotes (1 = enabled, 0 = disabled)
  uint8_t pwrBtnFootnoteBack = 1;
  // Use book's embedded CSS styles for EPUB rendering (1 = enabled, 0 = disabled)
  uint8_t embeddedStyle = 1;
  // Focus Reading - emphasizes the first part of words with bold
  uint8_t focusReadingEnabled = 0;
  uint8_t readerMenuStyle = READER_MENU_LIST;
  // SD card font family name (empty = use built-in fontFamily)
  char sdFontFamilyName[32] = "";
  // Dictionary folder name under /dictionaries (empty = no dictionary)
  char dictionaryName[32] = "";
  // Show hidden files/directories (starting with '.') in the file browser (0 = hidden, 1 = show)
  uint8_t showHiddenFiles = 0;
  // Remove a book from the Recent Books list when its End-of-Book screen is reached (0 = off, 1 = on)
  uint8_t removeReadBooksFromRecents = 0;
  // Move epub to /Read/ folder on SD card when finished (0 = disabled, 1 = enabled)
  uint8_t moveFinishedToReadFolder = 0;
  // Short press Back goes to file browser instead of home (0 = disabled, 1 = enabled)
  uint8_t backShortToFileBrowser = 0;
  // Image rendering mode in EPUB reader
  uint8_t imageRendering = IMAGES_DISPLAY;
  // Tilt-based page turning (X3 only — requires QMI8658 IMU)
  uint8_t tiltPageTurn = TILT_OFF;
  // Touch screen reader zones/gestures on boards with a touch controller.
  uint8_t touchReaderControls = TOUCH_READER_SWIPE;
  // Reader menu open gesture (SHOW_READER_MENU: off / center tap / bottom-edge
  // up-swipe). Only surfaced on home-key boards, where Home is the capacitive
  // key and the bottom edge is free; elsewhere it stays at the Tap default.
  uint8_t showReaderMenu = READER_MENU_TAP;
  // Frontlight quick-panel state. Category-less SettingsList entries persist
  // these without adding them to the regular Settings screen.
  uint8_t frontlightBrightness = 60;
  uint8_t frontlightWarmth = 50;  // 0 = cool .. 100 = warm
  uint8_t frontlightOn = 0;
  // Restore the saved on/off state after a normal boot or wake. Brightness and
  // warmth are always remembered even when this is disabled.
  uint8_t frontlightRestoreOnWake = 1;
  // Language setting (Language enum index, default 0 = EN)
  uint8_t language = 0;
  // Quick Resume: keep current content visible with moon icon instead of showing a static sleep screen.
  uint8_t quickResumeSleepScreen = QUICK_RESUME_NEVER;

  // Year Progress / Reading Heatmap sleep screens. Edited in
  // DotsScreenSettingsActivity; persisted by toJson/fromJson through the
  // DOTS_FIELDS table rather than SettingsList (they have no place in the four
  // Settings tabs).
  static constexpr uint8_t DOTS_COLUMNS_MIN = 4;
  static constexpr uint8_t DOTS_COLUMNS_MAX = 40;
  static constexpr uint8_t HEAT_DAYS_MIN = 7;
  static constexpr uint8_t HEAT_DAYS_MAX = 250;
  // Look shared by both screens
  uint8_t dotsMargin = DOTS_SCALE_MEDIUM;
  uint8_t dotsBackground = DOTS_BG_WHITE;
  uint8_t dotsTitleFont = DOTS_FONT_HELVETICA_NEUE_BOLD;
  uint8_t dotsTitleSize = DOTS_SCALE_LARGE;
  uint8_t dotsSubtitleFont = DOTS_FONT_GEIST_MEDIUM;
  uint8_t dotsSubtitleSize = DOTS_SCALE_SMALL;
  uint8_t dotsTextPosition = DOTS_TEXT_BOTTOM;
  uint8_t dotsTextAlign = DOTS_ALIGN_CENTER;
  uint8_t dotsOrientation = PORTRAIT;  // ORIENTATION value
  // Year Progress
  uint8_t yearColumns = 15;  // 15 x 25 fills a portrait screen
  uint8_t yearLayout = YEAR_LAYOUT_GRID;
  uint8_t yearDotShape = DOTS_SHAPE_CIRCLE;
  uint8_t yearDotSize = DOTS_SIZE_MEDIUM;
  uint8_t yearPastStyle = YEAR_DOT_LIGHT_GRAY;
  uint8_t yearTodayStyle = YEAR_TODAY_AS_PAST;
  uint8_t yearFutureStyle = YEAR_DOT_SOLID;
  uint8_t yearTitle = YEAR_TEXT_YEAR;
  uint8_t yearSubtitle = YEAR_TEXT_PERCENT;
  // Reading Heatmap
  uint8_t heatDays = 30;
  uint8_t heatLayout = HEAT_LAYOUT_WEEKS;
  uint8_t heatColumns = 6;
  uint8_t heatWeekStart = WEEK_START_MONDAY;
  uint8_t heatScale = HEAT_SCALE_AUTO;
  uint8_t heatEmptyStyle = HEAT_EMPTY_OUTLINE;
  uint8_t heatShape = DOTS_SHAPE_ROUNDED;
  uint8_t heatDotSize = DOTS_SIZE_HUGE;
  uint8_t heatTitle = HEAT_TEXT_TOTAL_TIME;
  uint8_t heatSubtitle = HEAT_TEXT_STREAK;
  // Home-screen widgets
  static constexpr uint8_t HOME_WIDGET_SLOTS = 4;
  uint8_t homeWidget1 = HW_CLOCK;
  uint8_t homeWidget2 = HW_TODAY;
  uint8_t homeWidget3 = HW_STREAK;
  uint8_t homeWidget4 = HW_NONE;
  uint8_t weatherUnit = WEATHER_CELSIUS;
  uint8_t weatherAutoRefresh = WEATHER_REFRESH_MANUAL;
  // Calendar sleep screen: upcoming days listed (real day count, 1..7).
  uint8_t calendarDays = 3;
  uint8_t calendarSleepRefresh = CAL_REFRESH_10M;
  // Pomodoro lengths in minutes, edited inside the Focus Timer activity.
  uint8_t pomodoroFocusMin = 25;
  uint8_t pomodoroBreakMin = 5;

  static constexpr uint8_t MIN_SLEEP_TIMEOUT_MINUTES = 1;
  static constexpr uint8_t SLEEP_TIMEOUT_NEVER_MINUTES = 31;
  static constexpr uint8_t MAX_SLEEP_TIMEOUT_MINUTES = SLEEP_TIMEOUT_NEVER_MINUTES;

  // Callback to resolve SD card font IDs. Set by SdCardFontSystem::begin().
  // Returns font ID or 0 if not found.
  using SdFontIdResolver = int (*)(void* ctx, const char* familyName, uint8_t fontSize);
  SdFontIdResolver sdFontIdResolver = nullptr;
  void* sdFontResolverCtx = nullptr;

  uint16_t getPowerButtonDuration() const {
    return (shortPwrBtn == CrossPointSettings::SHORT_PWRBTN::SLEEP) ? 10 : 400;
  }
  int getReaderFontId() const;

  // Drop the SD font selection and fall back to the built-in family. The reader
  // point size comes back into BUILTIN_READER_POINT_SIZES with it, since that is
  // the only set a built-in family ships — otherwise the settings UI would keep
  // offering a size nothing renders at. Both fields are persisted in one write.
  void clearSdFontFamily();

  // Resolved status-bar composition. Consumers read the spec; only settings
  // editors read the raw fields.
  //
  // Deliberately NOT built under storeMutex: every field it reads is a single
  // byte, so a concurrent settings write can never produce a corrupt value —
  // only a snapshot mixing pre- and post-change fields. That costs at most one
  // e-ink frame drawn with a mixed status bar, which self-corrects on the next
  // refresh. Locking here would instead put a mutex on the render path and
  // stall it behind the SD write inside saveToFile(). Don't add one back.
  struct StatusBarSpec {
    bool showChapterPageCount = false;
    bool showBookProgressPercent = false;
    uint8_t titleMode = HIDE_TITLE;  // STATUS_BAR_TITLE
    bool showBattery = false;
    bool showBatteryPercent = false;
    uint8_t clockMode = STATUS_BAR_CLOCK_HIDE;  // STATUS_BAR_CLOCK_MODE
    bool clock12h = false;
    uint8_t clockUtcOffsetQ = 48;             // 48 = UTC+0
    uint8_t progressBarMode = HIDE_PROGRESS;  // STATUS_BAR_PROGRESS_BAR
    uint8_t progressBarHeightPx = 0;          // (thickness+1)*2; 0 when the bar is hidden
    uint8_t xtcMode = XTC_STATUS_BAR_HIDE;    // XTC_STATUS_BAR_MODE

    bool showsProgressBar() const { return progressBarMode != HIDE_PROGRESS; }
    bool showsTitle() const { return titleMode != HIDE_TITLE; }
    bool showsClock() const { return clockMode != STATUS_BAR_CLOCK_HIDE; }
    // Visibility of the text lane. Clock hardware presence is the caller's
    // concern: pass halClock.isAvailable(), or true for layout reservation.
    bool textLaneVisible(bool clockAvailable) const {
      return showChapterPageCount || showBookProgressPercent || showsTitle() || showBattery ||
             (showsClock() && clockAvailable);
    }
  };
  StatusBarSpec statusBarSpec() const;

  // Resolved text-rendering configuration for the Epub layout engine. The
  // viewport is renderer/orientation-derived, so the caller supplies it —
  // passing it in keeps a spec from ever existing in a half-filled state.
  // Unlocked for the same reason as statusBarSpec(); see the note above.
  ReaderRenderSpec readerRenderSpec(uint16_t viewportWidth, uint16_t viewportHeight) const;

  static const char* getFilePath() { return "/.crosspoint/settings.json"; }
  void toJson(JsonDocument& doc) const;
  bool fromJson(JsonVariantConst doc);

  static void validateFrontButtonMapping(CrossPointSettings& settings);
  static uint8_t sleepTimeoutEnumToMinutes(uint8_t legacyValue);

  float getReaderLineCompression() const;
  unsigned long getSleepTimeoutMs() const;
  int getRefreshFrequency() const;
};

// Helper macro to access settings
#define SETTINGS CrossPointSettings::getInstance()
