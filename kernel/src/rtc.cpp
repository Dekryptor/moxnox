module;

#include <chrono>

export module rtc;

import inout;

using namespace std;

// timer.cpp
extern "C" volatile int64_t system_time;
extern "C" volatile uint64_t system_time_offset;

template<unsigned N>
static uint8_t get_rtc_register(uint16_t port) {
    outb(port, N);

    return inb(port + 1);
}

static bool get_update_in_progress(uint16_t port) {
    return get_rtc_register<0xa>(port) & 0x80;
}

static constexpr int64_t ymd_to_num(const chrono::year_month_day& d) noexcept {
    int m2 = ((int)(unsigned int)d.month() - 14) / 12;
    int64_t n;

    n = (1461 * ((int)d.year() + 4800 + m2)) / 4;
    n += (367 * ((int)(unsigned int)d.month() - 2 - (12 * m2))) / 12;
    n -= (3 * (((int)d.year() + 4900 + m2)/100)) / 4;
    n += (unsigned int)d.day();
    n -= 2337889;

    return n;
}

export void init_rtc(uint16_t port) {
    struct ymdhms {
        int year;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;

        bool operator==(const ymdhms& that) const noexcept {
            return year == that.year && month == that.month &&
                   day == that.day && hour == that.hour &&
                   minute == that.minute && second == that.second;
        }
    };

    ymdhms now, last;

    __asm("cli");

    while (get_update_in_progress(port)) { }

    now.second = get_rtc_register<0x00>(port);
    now.minute = get_rtc_register<0x02>(port);
    now.hour = get_rtc_register<0x04>(port);
    now.day = get_rtc_register<0x07>(port);
    now.month = get_rtc_register<0x08>(port);
    now.year = get_rtc_register<0x09>(port);

    do {
        last = now;

        while (get_update_in_progress(port)) { }

        now.second = get_rtc_register<0x00>(port);
        now.minute = get_rtc_register<0x02>(port);
        now.hour = get_rtc_register<0x04>(port);
        now.day = get_rtc_register<0x07>(port);
        now.month = get_rtc_register<0x08>(port);
        now.year = get_rtc_register<0x09>(port);
    } while (last != now);

    auto register_b = get_rtc_register<0x0b>(port);

    __asm("sti");

    if (!(register_b & 0x04)) {
        now.second = (now.second & 0x0f) + (uint8_t)((now.second >> 4) * 10);
        now.minute = (now.minute & 0x0f) + (uint8_t)((now.minute >> 4) * 10);
        now.hour = ((now.hour & 0x0f) + (uint8_t)(((now.hour & 0x70) >> 4) * 10)) | (now.hour & 0x80);
        now.day = (now.day & 0x0f) + (uint8_t)((now.day >> 4) * 10);
        now.month = (now.month & 0x0f) + (uint8_t)((now.month >> 4) * 10);
        now.year = (now.year & 0x0f) + (uint8_t)((now.year >> 4) * 10);
    }

    if (!(register_b & 0x02) && now.hour & 0x80)
        now.hour = (uint8_t)(((now.hour & 0x7f) + 12) % 24);

    if (now.year >= 70)
        now.year += 1900;
    else
        now.year += 2000;

    auto ymd = chrono::year_month_day{chrono::year{now.year}, chrono::month{now.month},
                                      chrono::day{now.day}};

    auto day_num = ymd_to_num(ymd);

    system_time_offset = (uint64_t)((((day_num * 86400) + (now.hour * 3600) + (now.minute * 60) + now.second) * 10000000) - system_time);
}
