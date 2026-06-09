module;

#include <time.h>
#include <stdint.h>

export module time;

import nt;
import ucrtbase;

static const unsigned int SYSTEM_TIME_RESOLUTION = 10000000; // 100ns

static uint64_t init_time;

export void init_clock() {
    LARGE_INTEGER now;

    NtQuerySystemTime(&now);

    init_time = (uint64_t)now.QuadPart;
}

export __time64_t win_to_unix_time(int64_t t) {
    return (__time64_t)((t - 116444736000000000) / 10000000);
}

extern "C"
clock_t clock() {
    LARGE_INTEGER now;

    NtQuerySystemTime(&now);

    return (clock_t)(((uint64_t)now.QuadPart - init_time) / (SYSTEM_TIME_RESOLUTION / CLOCKS_PER_SEC));
}

extern "C"
__time64_t __time64(__time64_t* t) {
    LARGE_INTEGER now;
    __time64_t ret;

    NtQuerySystemTime(&now);

    ret = win_to_unix_time(now.QuadPart);

    if (t)
        *t = ret;

    return ret;
}

extern "C"
__time32_t __time32(__time32_t* t) {
    auto ret = (__time32_t)__time64(nullptr);

    if (t)
        *t = ret;

    return ret;
}

extern "C"
constexpr errno_t __gmtime64_s(struct tm* tm, const __time64_t* t) {
    auto v = *t;

    tm->tm_isdst = 0;

    auto secs = v % 86400;
    auto days = v / 86400;

    if (secs < 0) {
        secs += 86400;
        days--;
    }

    tm->tm_sec = (int)(secs % 60);
    tm->tm_min = (int)((secs % 3600) / 60);
    tm->tm_hour = (int)(secs / 3600);

    tm->tm_wday = (int)(((days + 4) % 7) + 7) % 7;

    days -= 11017; // shift epoch to March 1, 2000

    int64_t era = (days >= 0 ? days : (days - 146096)) / 146097;
    auto day_of_era = (unsigned int)(days - (era * 146097));
    unsigned int year_of_era = (day_of_era - (day_of_era / 1460) + (day_of_era / 36524) - (day_of_era / 146096)) / 365;
    unsigned int day_of_year = day_of_era - ((365 * year_of_era) + (year_of_era / 4) - (year_of_era / 100));
    unsigned int month_index = ((5 * day_of_year) + 2) / 153;

    tm->tm_mday = (int)(day_of_year - (((153 * month_index) + 2) / 5) + 1);
    tm->tm_mon = (int)((month_index < 10) ? (month_index + 2) : (month_index - 10));
    tm->tm_year = (int)(year_of_era + (era * 400) + 100);

    if (tm->tm_mon < 2)
        tm->tm_year++;

    bool leap = (year_of_era % 4 == 0 && (year_of_era % 100 != 0 || year_of_era % 400 == 0));

    tm->tm_yday = (month_index < 10) ? (int)(day_of_year + 59 + leap) : (int)(day_of_year - 306);

    return 0;
}

static constexpr bool test_gmtime(int64_t v, int exp_wday, int exp_mday,
                                  int exp_mon, int exp_year, int exp_yday) {
    struct tm tm;

    __gmtime64_s(&tm, &v);

    return tm.tm_wday == exp_wday && tm.tm_mday == exp_mday &&
           tm.tm_mon == exp_mon && tm.tm_year == exp_year &&
           tm.tm_yday == exp_yday;
}

static_assert(test_gmtime(-604800, 4, 25, 11, 69, 358));
static_assert(test_gmtime(-86400, 3, 31, 11, 69, 364));
static_assert(test_gmtime(-1, 3, 31, 11, 69, 364));
static_assert(test_gmtime(0, 4, 1, 0, 70, 0));
static_assert(test_gmtime(1781027287, 2, 9, 5, 126, 159));

extern "C"
struct tm* __gmtime64(const __time64_t* t) {
    static struct tm tm;

    auto ret = __gmtime64_s(&tm, t);
    if (ret != 0)
        return nullptr;

    return &tm;
}
