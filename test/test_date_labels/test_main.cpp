#include <unity.h>
#include <time.h>
#include "DateLabels.h"

static time_t makeEpoch(int year, int mon, int day, int hour, int min)
{
    struct tm t = {};
    t.tm_year  = year - 1900;
    t.tm_mon   = mon - 1;
    t.tm_mday  = day;
    t.tm_hour  = hour;
    t.tm_min   = min;
    t.tm_isdst = -1;
    return mktime(&t);
}

void test_same_day_is_zero() {
    time_t now = makeEpoch(2026, 5, 30, 14, 0);
    time_t due = makeEpoch(2026, 5, 30, 20, 0);
    TEST_ASSERT_EQUAL_INT(0, vmDayDistance(now, due));
}

void test_tomorrow_is_one() {
    time_t now = makeEpoch(2026, 5, 30, 14, 0);
    time_t due = makeEpoch(2026, 5, 31,  9, 0);
    TEST_ASSERT_EQUAL_INT(1, vmDayDistance(now, due));
}

void test_day_after_is_two() {
    time_t now = makeEpoch(2026, 5, 30, 14, 0);
    time_t due = makeEpoch(2026,  6,  1,  9, 0);
    TEST_ASSERT_EQUAL_INT(2, vmDayDistance(now, due));
}

void test_cross_midnight() {
    time_t now = makeEpoch(2026, 5, 30, 23, 59);
    time_t due = makeEpoch(2026, 5, 31,  0,  1);
    TEST_ASSERT_EQUAL_INT(1, vmDayDistance(now, due));
}

void test_label_today() {
    TEST_ASSERT_EQUAL_STRING("Today",     vmDateChipLabel(0, 5));
}

void test_label_tomorrow() {
    TEST_ASSERT_EQUAL_STRING("Tomorrow",  vmDateChipLabel(1, 6));
}

void test_label_day_after() {
    TEST_ASSERT_EQUAL_STRING("Day after", vmDateChipLabel(2, 0));
}

void test_label_weekday_full() {
    TEST_ASSERT_EQUAL_STRING("Wednesday", vmDateChipLabel(3, 3));
    TEST_ASSERT_EQUAL_STRING("Sunday",    vmDateChipLabel(5, 0));
}

void test_label_null_for_seven_plus() {
    TEST_ASSERT_NULL(vmDateChipLabel(7,  1));
    TEST_ASSERT_NULL(vmDateChipLabel(14, 2));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_same_day_is_zero);
    RUN_TEST(test_tomorrow_is_one);
    RUN_TEST(test_day_after_is_two);
    RUN_TEST(test_cross_midnight);
    RUN_TEST(test_label_today);
    RUN_TEST(test_label_tomorrow);
    RUN_TEST(test_label_day_after);
    RUN_TEST(test_label_weekday_full);
    RUN_TEST(test_label_null_for_seven_plus);
    return UNITY_END();
}
