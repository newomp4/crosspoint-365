#pragma once

#include <cstdint>

// Proleptic Gregorian calendar arithmetic on "epoch days" (days since
// 1970-01-01), after Howard Hinnant's civil-date algorithms. Header-only so the
// clock HAL and the sleep-screen views share one definition.
namespace CivilDate {

inline int32_t daysFromCivil(int32_t year, const uint32_t month, const uint32_t day) {
  year -= month <= 2;
  const int32_t era = (year >= 0 ? year : year - 399) / 400;
  const uint32_t yearOfEra = static_cast<uint32_t>(year - era * 400);
  const uint32_t dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const uint32_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + static_cast<int32_t>(dayOfEra) - 719468;
}

inline void civilFromDays(int32_t days, uint16_t& year, uint8_t& month, uint8_t& day) {
  days += 719468;
  const int32_t era = (days >= 0 ? days : days - 146096) / 146097;
  const uint32_t dayOfEra = static_cast<uint32_t>(days - era * 146097);
  const uint32_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  const int32_t y = static_cast<int32_t>(yearOfEra) + era * 400;
  const uint32_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const uint32_t mp = (5 * dayOfYear + 2) / 153;
  const uint32_t d = dayOfYear - (153 * mp + 2) / 5 + 1;
  const uint32_t m = mp < 10 ? mp + 3 : mp - 9;
  year = static_cast<uint16_t>(y + (m <= 2));
  month = static_cast<uint8_t>(m);
  day = static_cast<uint8_t>(d);
}

inline bool isLeapYear(const uint16_t year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

inline uint8_t daysInMonth(const uint16_t year, const uint8_t month) {
  static constexpr uint8_t DAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 31;
  if (month == 2 && isLeapYear(year)) return 29;
  return DAYS[month - 1];
}

// 0 = Sunday .. 6 = Saturday (1970-01-01 was a Thursday).
inline uint8_t weekday(const int32_t epochDay) { return static_cast<uint8_t>(((epochDay % 7) + 11) % 7); }

}  // namespace CivilDate
