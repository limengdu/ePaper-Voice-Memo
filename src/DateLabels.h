#pragma once
#include <time.h>

// Days from the calendar-day of `from` to the calendar-day of `to`
// (both floored to local midnight). Positive = to is in the future.
inline int vmDayDistance(time_t from, time_t to)
{
    struct tm ta = {}, tb = {};
    localtime_r(&from, &ta);
    localtime_r(&to,   &tb);
    ta.tm_hour = ta.tm_min = ta.tm_sec = 0;
    tb.tm_hour = tb.tm_min = tb.tm_sec = 0;
    const time_t da = mktime(&ta);
    const time_t db = mktime(&tb);
    return static_cast<int>((db - da) / 86400);
}

// Short date-chip label for a future event.
// `days` = vmDayDistance(now, due)
// `wday` = tm_wday of the due datetime (0=Sun...6=Sat)
// Returns a string literal; returns nullptr for days >= 7 (caller formats "Mon DD").
inline const char* vmDateChipLabel(int days, int wday)
{
    static const char* kWeekdays[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday"
    };
    if (days == 0) return "Today";
    if (days == 1) return "Tomorrow";
    if (days == 2) return "Day after";
    if (days >= 3 && days <= 6)
        return kWeekdays[(wday >= 0 && wday < 7) ? wday : 0];
    return nullptr;
}
