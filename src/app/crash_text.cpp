#include "app/crash_text.hpp"

#include <csignal>

namespace redgem::crash {
namespace {

constexpr std::int64_t kSecondsPerDay = 24 * 60 * 60;
constexpr int kSecondsPerHour = 60 * 60;
constexpr int kSecondsPerMinute = 60;
constexpr int kDecimalBase = 10;

// The calendar's arithmetic (days to a date in the proleptic Gregorian calendar), counted in
// eras of 400 years from 1 March of the year 0, where a leap day falls at the end of a year.
constexpr std::int64_t kDaysFromYearZeroTo1970 = 719468;  // from 0000-03-01 to 1970-01-01
constexpr std::int64_t kDaysPerEra = 146097;              // in 400 years
constexpr std::int64_t kYearsPerEra = 400;
constexpr std::int64_t kDaysPerYear = 365;
constexpr std::int64_t kDaysPerFourYears = 1460;    // one day short of four years: where a leap day goes
constexpr std::int64_t kDaysPerCentury = 36524;     // of an era's first three centuries
constexpr std::int64_t kDaysPerEraButOne = 146096;  // an era less its last day
constexpr std::int64_t kMonthCycleDays = 153;       // of the five months from March to July
constexpr std::int64_t kMonthCycleMonths = 5;
constexpr std::int64_t kMonthCycleShift = 2;  // lines the cycle up with the 31-day months
constexpr std::int64_t kYearsPerLeapYear = 4;
constexpr std::int64_t kYearsPerCenturyYear = 100;  // a leap year that is not
constexpr int kMonthsPerYear = 12;
constexpr int kMarch = 3;
constexpr int kFebruary = 2;

// Like `/`, rounding down instead of towards zero (a moment before 1970).
std::int64_t floor_divide(std::int64_t value, std::int64_t by) {
  const std::int64_t quotient = value / by;
  return quotient * by > value ? quotient - 1 : quotient;
}

// The month and the day of the day of a year that starts on 1 March.
void place_in_year(std::int64_t day_of_year, Moment& moment) {
  const std::int64_t from_march = (kMonthCycleMonths * day_of_year + kMonthCycleShift) / kMonthCycleDays;
  const std::int64_t month_start = (kMonthCycleDays * from_march + kMonthCycleShift) / kMonthCycleMonths;
  moment.day = static_cast<int>(day_of_year - month_start + 1);
  const int month = static_cast<int>(from_march) + kMarch;
  moment.month = month > kMonthsPerYear ? month - kMonthsPerYear : month;
}

void place_date(std::int64_t days_since_epoch, Moment& moment) {
  const std::int64_t days = days_since_epoch + kDaysFromYearZeroTo1970;
  const std::int64_t era = floor_divide(days, kDaysPerEra);
  const std::int64_t day_of_era = days - era * kDaysPerEra;
  const std::int64_t year_of_era =
      (day_of_era - day_of_era / kDaysPerFourYears + day_of_era / kDaysPerCentury - day_of_era / kDaysPerEraButOne) /
      kDaysPerYear;
  const std::int64_t leap_days = year_of_era / kYearsPerLeapYear - year_of_era / kYearsPerCenturyYear;
  place_in_year(day_of_era - (kDaysPerYear * year_of_era + leap_days), moment);
  // (January and February belong to the year that started the March before.)
  const bool next_calendar_year = moment.month <= kFebruary;
  moment.year = year_of_era + era * kYearsPerEra + (next_calendar_year ? 1 : 0);
}

}  // namespace

Moment utc_moment(std::int64_t seconds_since_epoch) {
  Moment moment;
  const std::int64_t days = floor_divide(seconds_since_epoch, kSecondsPerDay);
  const auto of_day = static_cast<int>(seconds_since_epoch - days * kSecondsPerDay);
  place_date(days, moment);
  moment.hour = of_day / kSecondsPerHour;
  moment.minute = of_day % kSecondsPerHour / kSecondsPerMinute;
  moment.second = of_day % kSecondsPerMinute;
  return moment;
}

int reversed_digits(std::int64_t value, char (&digits)[kMaxDigits]) {
  int count = 0;
  do {
    digits[count++] = static_cast<char>('0' + value % kDecimalBase);
    value /= kDecimalBase;
  } while (value > 0);  // (kMaxDigits is enough for any 64-bit number)
  return count;
}

namespace {

constexpr int kYearDigits = 4;

struct SignalName {
  int signal = 0;
  const char* text = nullptr;
};
constexpr SignalName kSignalNames[] = {
    {SIGSEGV, "SIGSEGV (invalid memory access)"}, {SIGABRT, "SIGABRT (the program aborted)"},
    {SIGFPE, "SIGFPE (arithmetic error)"},        {SIGILL, "SIGILL (illegal instruction)"},
#ifdef SIGBUS
    {SIGBUS, "SIGBUS (invalid memory access)"},
#endif
};
constexpr int kPartDigits = 2;  // of a month, a day, an hour...

}  // namespace

FixedText<kMomentTextSize> moment_text(const Moment& moment) {
  FixedText<kMomentTextSize> text;
  text.append_number(moment.year, kYearDigits);
  text.append('-');
  text.append_number(moment.month, kPartDigits);
  text.append('-');
  text.append_number(moment.day, kPartDigits);
  text.append(' ');
  text.append_number(moment.hour, kPartDigits);
  text.append(':');
  text.append_number(moment.minute, kPartDigits);
  text.append(':');
  text.append_number(moment.second, kPartDigits);
  text.append(" UTC");
  return text;
}

FixedText<kMomentTextSize> moment_stamp(const Moment& moment) {
  FixedText<kMomentTextSize> text;
  text.append_number(moment.year, kYearDigits);
  text.append_number(moment.month, kPartDigits);
  text.append_number(moment.day, kPartDigits);
  text.append('-');
  text.append_number(moment.hour, kPartDigits);
  text.append_number(moment.minute, kPartDigits);
  text.append_number(moment.second, kPartDigits);
  return text;
}

const char* signal_description(int signal) {
  for (const SignalName& name : kSignalNames) {
    if (name.signal == signal) return name.text;
  }
  return "a fatal signal";
}

}  // namespace redgem::crash
