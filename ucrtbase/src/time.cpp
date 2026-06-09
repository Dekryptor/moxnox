module;

#include <time.h>
#include <stdint.h>
#include <span>
#include <string_view>
#include <charconv>

export module time;

import nt;
import ucrtbase;

using namespace std;

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

template<size_t N>
bool strftime_index(span<char>& sp, int num, const array<string_view, N>& str) {
    if (num < 0 || (size_t)num >= str.size()) {
        if (sp.empty())
            return false;

        sp[0] = '?';
        sp = span(sp.data() + 1, sp.size() - 1);

        return true;
    }

    auto s = str[num];

    if (sp.size() < s.size())
        return false;

    memcpy(sp.data(), s.data(), s.size());
    sp = span(sp.data() + s.size(), sp.size() - s.size());

    return true;
}

static bool strftime_abbrev_weekday(span<char>& sp, int wday) {
    static const array<string_view, 7> str = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

    return strftime_index(sp, wday, str);
}

static bool strftime_weekday(span<char>& sp, int wday) {
    static const array<string_view, 7> str = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

    return strftime_index(sp, wday, str);
}

static bool strftime_abbrev_month_name(span<char>& sp, int mon) {
    static const array<string_view, 12> str = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    return strftime_index(sp, mon, str);
}

static bool strftime_month_name(span<char>& sp, int mon) {
    static const array<string_view, 12> str = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

    return strftime_index(sp, mon, str);
}

template<unsigned int N>
bool strftime_number(span<char>& sp, int num) {
    if (num < 0) { // FIXME - can have negative years?
        if (sp.empty())
            return false;

        sp[0] = '?';
        sp = span(sp.data() + 1, sp.size() - 1);

        return true;
    }

    if (num == 0) {
        if (sp.size() < N)
            return false;

        for (unsigned int i = 0; i < N; i++) {
            sp[i] = '0';
        }

        sp = span(sp.data() + N, sp.size() - N);
        return true;
    }

    if (N > 1) {
        unsigned int limit = 1;

        for (unsigned int i = 1; i < N; i++) {
            limit *= 10;
        }

        while ((unsigned int)num < limit) {
            if (sp.empty())
                return false;

            sp[0] = '0';
            sp = span(sp.data() + 1, sp.size() - 1);

            if (limit == 10)
                break;

            limit /= 10;
        }
    }

    auto tcr = to_chars(sp.data(), sp.data() + sp.size(), num);

    if (tcr.ec != errc{})
        return false;

    size_t written = tcr.ptr - sp.data();

    sp = span(sp.data() + written, sp.size() - written);

    return true;
}

extern "C"
size_t strftime(char* s, size_t max, const char* format, const struct tm* tm) {
    auto fmt = string_view(format);
    auto sp = span(s, max);

    while (true) {
        auto pc = fmt.find('%');

        if (pc == string_view::npos)
            pc = fmt.size();

        if (pc > 0) {
            if (sp.size() < pc)
                return 0;

            memcpy(sp.data(), fmt.data(), pc);

            sp = span(sp.data() + pc, sp.size() - pc);
            fmt = string_view(fmt.data() + pc, fmt.size() - pc);

            if (fmt.empty())
                break;
        }

        // skip %
        fmt = string_view(fmt.data() + 1, fmt.size() - 1);

        if (fmt.empty())
            break;

        switch (fmt[0]) {
            case 'a': // abbreviated weekday
                if (!strftime_abbrev_weekday(sp, tm->tm_wday))
                    return 0;
            break;

            case 'A': // weekday
                if (!strftime_weekday(sp, tm->tm_wday))
                    return 0;
            break;

            case 'b': // abbreviated month name
            case 'h':
                if (!strftime_abbrev_month_name(sp, tm->tm_mon))
                    return 0;
            break;

            case 'B': // month name
                if (!strftime_month_name(sp, tm->tm_mon))
                    return 0;
            break;

            // FIXME - c ("locale's appropriate date and time representation")

            case 'C': // year / 100
                if (!strftime_number<2>(sp, (tm->tm_year + 1900) / 100))
                    return 0;
            break;

            case 'd': // day of month as decimal number
                if (!strftime_number<2>(sp, tm->tm_mday))
                    return 0;
            break;

            case 'D': // %m/%d/%y
                if (!strftime_number<2>(sp, tm->tm_mon + 1))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = '/';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<2>(sp, tm->tm_mday))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = '/';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<2>(sp, tm->tm_year % 100))
                    return 0;
            break;

            // FIXME - e (day of month as decimal number, space-padded)
            // FIXME - F (%Y-%m-%d)
            // FIXME - g (last 2 digits of week-based year)
            // FIXME - G (week-based year)

            case 'H': // hour in 24-hour clock
                if (!strftime_number<2>(sp, tm->tm_hour))
                    return 0;
            break;

            case 'I': // hour in 12-hour clock
                if (!strftime_number<2>(sp, ((tm->tm_hour + 11) % 12) + 1))
                    return 0;
            break;

            case 'j': // day of year as decimal number
                if (!strftime_number<3>(sp, tm->tm_yday + 1))
                    return 0;
            break;

            case 'm': // month as decimal number
                if (!strftime_number<2>(sp, tm->tm_mon + 1))
                    return 0;
            break;

            case 'M': // minute as decimal number
                if (!strftime_number<2>(sp, tm->tm_min))
                    return 0;
            break;

            case 'n':
                if (sp.empty())
                    return 0;

                sp[0] = '\n';
                sp = span(sp.data() + 1, sp.size() - 1);
            break;

            // FIXME - p (a.m. or p.m.)
            // FIXME - r (%I:%M:%S %p)

            case 'R': // %H:%M
                if (!strftime_number<2>(sp, tm->tm_hour))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = ':';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<2>(sp, tm->tm_min))
                    return 0;
            break;

            case 'S': // second as decimal number
                if (!strftime_number<2>(sp, tm->tm_sec))
                    return 0;
            break;

            case 't':
                if (sp.empty())
                    return 0;

                sp[0] = '\t';
                sp = span(sp.data() + 1, sp.size() - 1);
            break;

            case 'T': // %H:%M:%S
                if (!strftime_number<2>(sp, tm->tm_hour))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = ':';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<2>(sp, tm->tm_min))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = ':';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<2>(sp, tm->tm_sec))
                    return 0;
            break;

            case 'u': // weekday as decimal number, 1 = Monday, 7 = Sunday
                if (!strftime_number<1>(sp, tm->tm_wday == 0 ? 7 : tm->tm_wday))
                    return 0;
            break;

            // FIXME - U (week number of year as decimal number)
            // FIXME - V (week number of year as decimal number, mark 2)

            case 'w': // weekday as decimal number, 0 = Sunday, 6 = Saturday
                if (!strftime_number<1>(sp, tm->tm_wday))
                    return 0;
            break;

            // FIXME - W (week number of year as decimal number, mark 3)
            // FIXME - x ("locale's appropriate date representation")
            // FIXME - X ("locale's appropriate time representation")

            case 'y': // year % 100
                if (!strftime_number<2>(sp, tm->tm_year % 100))
                    return 0;
            break;

            case 'Y': // year
                if (!strftime_number<4>(sp, tm->tm_year + 1900))
                    return 0;
            break;

            // FIXME - z (TZ offset)
            // FIXME - Z (TZ name or abbreviation)

            case '%':
                if (sp.empty())
                    return 0;

                sp[0] = '%';
                sp = span(sp.data() + 1, sp.size() - 1);
            break;
        }

        fmt = string_view(fmt.data() + 1, fmt.size() - 1);

        if (fmt.empty())
            break;

        if (sp.empty())
            return 0;
    }

    if (sp.empty())
        return 0;

    sp[0] = 0;

    return sp.data() - s;
}
