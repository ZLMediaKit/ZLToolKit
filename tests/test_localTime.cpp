/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#include "Util/local_time.h"
#include "Util/util.h"

using namespace toolkit;

//本用例覆盖时区偏移的正确性，判定标准统一为"与libc的localtime结果完全一致"——
//这比写死期望值更强，也不必随时区数据库的更新而维护。
//时区一律使用POSIX TZ字符串而非IANA名称，因为Windows的_tzset只认前者；而且它只认
//"三字母名[+|-]hh[:mm]三字母名"这一种写法，规则部分被忽略(按美国规则、固定一小时)，所以下面的
//名字一律三个字母，好让Windows至少把偏移解析出来。半小时夏令时、负夏令时这些规则在Windows上
//测不到，那里只剩"本库与CRT一致"这一层。
//This case covers the correctness of the timezone offset. The criterion is always
//"identical to what libc's localtime returns", which is a stronger check than hardcoded
//expectations and needs no maintenance when the timezone database is updated.
//Timezones are given as POSIX TZ strings rather than IANA names, because the _tzset of
//Windows only understands the former; and it only understands the form
//"three-letter name[+|-]hh[:mm]three-letter name", ignoring the rule part (US rules, a fixed
//hour), so every name below has three letters so that Windows at least parses the offset.
//Half hour and negative daylight saving rules cannot be exercised there, only "this library
//agrees with the CRT" remains on Windows.
static void useTimezone(const char *tz) {
#ifdef _WIN32
    _putenv_s("TZ", tz);
    _tzset();
#else
    setenv("TZ", tz, 1);
    tzset();
#endif
    //库只在启动时把时区算成一张表，换了时区必须重建。重建要求没有其它线程在读表，
    //本文件的并发用例是在起线程之前换好时区的
    //The library computes its timezone table once at start up, so the table has to be rebuilt
    //after a change of timezone. Rebuilding requires that no other thread reads the table, and
    //the concurrent case in this file changes the timezone before starting any thread
    local_time_init();
}

static struct tm libcLocalTime(time_t sec) {
    struct tm tm;
#ifdef _WIN32
    localtime_s(&tm, &sec);
#else
    localtime_r(&sec, &tm);
#endif
    return tm;
}

//在t上加days天，超出time_t范围则截到上限之前(32位time_t到2038年为止)
//Add days to t, clamped just below the upper bound of time_t (which is 2038 on 32 bit)
static time_t daysLater(time_t t, long days) {
    const long long day = 24 * 3600;
    long long v = (long long)t + days * day;
    long long max = (long long)std::numeric_limits<time_t>::max() - day;
    return (time_t)(v < max ? v : max);
}

//逐字段比较本库与libc对时刻sec的换算结果
//Compare the result of this library with libc field by field for the moment sec
static bool matchesLibc(const char *tz, time_t sec) {
    auto got = getLocalTime(sec);
    auto expect = libcLocalTime(sec);

    if (got.tm_year != expect.tm_year || got.tm_mon != expect.tm_mon || got.tm_mday != expect.tm_mday
        || got.tm_hour != expect.tm_hour || got.tm_min != expect.tm_min || got.tm_sec != expect.tm_sec
        || got.tm_isdst != expect.tm_isdst) {
        printf("[FAIL] TZ=%s 时刻%lld不一致: 本库=%04d-%02d-%02d %02d:%02d:%02d(isdst=%d) libc=%04d-%02d-%02d %02d:%02d:%02d(isdst=%d)\n",
               tz, (long long)sec, 1900 + got.tm_year, 1 + got.tm_mon, got.tm_mday, got.tm_hour, got.tm_min,
               got.tm_sec, got.tm_isdst, 1900 + expect.tm_year, 1 + expect.tm_mon, expect.tm_mday, expect.tm_hour,
               expect.tm_min, expect.tm_sec, expect.tm_isdst);
        return false;
    }
#ifndef _WIN32
    //tm_gmtoff是glibc/BSD的扩展字段，Windows的struct tm没有该成员
    //tm_gmtoff is a glibc/BSD extension, the struct tm of Windows has no such member
    if (got.tm_gmtoff != expect.tm_gmtoff) {
        printf("[FAIL] TZ=%s 时刻%lld偏移不一致: 本库=%ld libc=%ld\n", tz, (long long)sec, (long)got.tm_gmtoff,
               (long)expect.tm_gmtoff);
        return false;
    }
#endif

    //以%Z格式化会读取tm_zone。该字段一旦没有被填写，读到的就是调用方栈上的残留指针，
    //轻则输出乱码、重则直接崩溃，且不一定每次都发作，故此处与libc逐字比对加以固定
    //Formatting with %Z reads tm_zone. If that field is left unset, whatever pointer happens
    //to be on the caller's stack gets dereferenced: garbled output at best, a crash at worst,
    //and not necessarily on every run; comparing it against libc pins the behaviour down
    char got_str[64], expect_str[64];
    if (!strftime(got_str, sizeof(got_str), "%Y-%m-%d %H:%M:%S %z [%Z]", &got)
        || !strftime(expect_str, sizeof(expect_str), "%Y-%m-%d %H:%M:%S %z [%Z]", &expect)) {
        printf("[FAIL] TZ=%s 时刻%lld strftime失败\n", tz, (long long)sec);
        return false;
    }
    if (strcmp(got_str, expect_str) != 0) {
        printf("[FAIL] TZ=%s 时刻%lld格式化结果不一致: 本库=%s libc=%s\n", tz, (long long)sec, got_str, expect_str);
        return false;
    }

    //getTimeStr()是对外接口，下游拿它格式化时区名的可能性最大，一并覆盖
    //getTimeStr() is the public interface and the most likely place for a downstream project
    //to format the timezone name, so it is covered as well
    if (getTimeStr("%Y-%m-%d %H:%M:%S %z [%Z]", sec) != expect_str) {
        printf("[FAIL] TZ=%s 时刻%lld getTimeStr()结果不一致: 本库=%s libc=%s\n", tz, (long long)sec,
               getTimeStr("%Y-%m-%d %H:%M:%S %z [%Z]", sec).data(), expect_str);
        return false;
    }
    return true;
}

static bool sameAsLibc(const char *tz) {
    useTimezone(tz);
    return matchesLibc(tz, time(nullptr));
}

//换算出的时刻必须与同一结构体里的tm_gmtoff同源，
//否则会出现"时刻按夏令时算、偏移却按标准时标注"这种自相矛盾的输出
//The broken down time has to come from the same offset as the tm_gmtoff in that very
//structure, otherwise the output contradicts itself: the time following the daylight saving
//rule while the offset next to it states standard time
#ifndef _WIN32
static bool selfConsistent(const char *tz) {
    useTimezone(tz);
    time_t now = time(nullptr);
    auto tm = getLocalTime(now);
    time_t local = now + tm.tm_gmtoff;
    if (tm.tm_hour != (int)(local / 3600 % 24) || tm.tm_min != (int)(local / 60 % 60)
        || tm.tm_sec != (int)(local % 60)) {
        printf("[FAIL] TZ=%s 时刻与tm_gmtoff不同源: %02d:%02d:%02d vs 由偏移%ld反推的%02d:%02d:%02d\n", tz,
               tm.tm_hour, tm.tm_min, tm.tm_sec, (long)tm.tm_gmtoff, (int)(local / 3600 % 24),
               (int)(local / 60 % 60), (int)(local % 60));
        return false;
    }
    //getGMTOff()与getLocalTime()必须取自同一份数据，不能各自缓存一份
    //getGMTOff() and getLocalTime() must read the same data instead of caching one copy each
    if (getGMTOff() != tm.tm_gmtoff) {
        printf("[FAIL] TZ=%s getGMTOff()=%ld 与 tm_gmtoff=%ld 不一致\n", tz, getGMTOff(), (long)tm.tm_gmtoff);
        return false;
    }
    return true;
}
#endif

#ifndef _WIN32
//并发下tm_zone必须稳定且与libc一致。这条用例针对的是一个具体的教训：曾经把tm_zone直接指向
//tzname[]，而glibc在每一次localtime_r()中都会改写该数组——先置NULL、算完再写回，全程持
//tzset_lock，本库却是不持锁读的，于是并发下读到NULL的比例可高达五成。单线程用例完全覆盖不到它。
//现在tm_zone指向库自己表里的副本，本用例守住的就是"不得再指回libc的可变存储"这一点。
//tm_zone must stay stable and equal to libc under concurrency. This case guards a concrete
//lesson: tm_zone used to point straight into tzname[], which glibc rewrites on every
//localtime_r() call, setting it to NULL first and writing it back afterwards, all under
//tzset_lock while this library reads without any lock; the share of NULL readings reached
//fifty percent. A single threaded case cannot catch that at all. tm_zone now points into a copy
//in the library's own table, and what this case defends is that it never points back into the
//mutable storage of libc.
static bool zoneStableUnderConcurrency() {
    //必须用读取时区数据库的时区名，不能用POSIX TZ字符串：glibc只有在走tzfile这条路径时
    //才会反复改写tzname[]，用"CST-8"之类的字符串跑，这条用例会静默地什么都测不到
    //A timezone name backed by the timezone database is required here, a POSIX TZ string will
    //not do: glibc only rewrites tzname[] on the tzfile code path, so running this case with
    //something like "CST-8" would silently exercise nothing
    useTimezone("Asia/Shanghai");
    //缺少时区数据库时(精简容器、交叉编译的根文件系统等)，glibc会把上面的名字当作POSIX
    //TZ字符串解析失败后退回UTC，于是不再走tzfile那条会改写tzname[]的路径，这条用例就会
    //静默地什么都测不到。此处显式断言环境可用，宁可失败也不要假绿。
    //Without a timezone database (a slim container, a cross compiled rootfs and so on) glibc
    //parses the name above as a POSIX TZ string, fails and falls back to UTC, no longer taking
    //the tzfile path that rewrites tzname[], and this case would silently exercise nothing. The
    //environment is asserted explicitly here: failing is better than being falsely green.
    if (getGMTOff() != 8 * 3600) {
        printf("[FAIL] 时区数据库不可用(Asia/Shanghai解析为偏移%ld)，并发用例无法生效\n", getGMTOff());
        return false;
    }
    //起线程之前先把libc给出的缩写复制一份作为期望值
    //Take a copy of the abbreviation libc reports as the expectation before any thread starts
    char expect[16];
    {
        auto tm = libcLocalTime(time(nullptr));
        snprintf(expect, sizeof(expect), "%s", tm.tm_zone ? tm.tm_zone : "");
    }

    std::atomic<bool> running { true };
    std::atomic<uint64_t> calls { 0 }, bad { 0 };
    //持续触发localtime_r，模拟宿主自身的时间调用以及日志清理时的mktime
    //Keep triggering localtime_r to mimic the time calls of the host program itself and the
    //mktime of the log cleanup
    std::thread churn([&]() {
        while (running) {
            libcLocalTime(time(nullptr));
        }
    });
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&]() {
            while (running) {
                auto tm = getLocalTime(time(nullptr));
                ++calls;
                if (!tm.tm_zone || strcmp(tm.tm_zone, expect) != 0) {
                    ++bad;
                }
            }
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running = false;
    churn.join();
    for (auto &t : readers) {
        t.join();
    }
    if (bad) {
        printf("[FAIL] 并发下tm_zone为空或不等于%s: %llu/%llu 次\n", expect, (unsigned long long)bad.load(),
               (unsigned long long)calls.load());
        return false;
    }
    return true;
}
#endif

//本地时间各字段与其unix时间戳之差，即偏移(秒)。只用struct tm的标准字段，三平台通用，
//并且与库自身按段查表的算法无关
//Difference between the fields of a local time and its unix timestamp, that is the offset in
//seconds. Only the standard fields of struct tm are used, so it works on all three platforms,
//and it shares nothing with the segment lookup of the library
static long offsetOf(const struct tm &tm, time_t sec) {
    long year = 1900L + tm.tm_year;
    //[1970, year)内的闰年个数
    //Number of leap years within [1970, year)
    auto leaps = [](long y) { --y; return y / 4 - y / 100 + y / 400; };
    long long days = 365LL * (year - 1970) + (leaps(year) - leaps(1970)) + tm.tm_yday;
    long long local = days * 24 * 3600 + tm.tm_hour * 3600LL + tm.tm_min * 60LL + tm.tm_sec;
    return (long)(local - (long long)sec);
}

struct LibcState {
    long offset;
    int isdst;
    bool operator==(const LibcState &o) const { return offset == o.offset && isdst == o.isdst; }
    bool operator!=(const LibcState &o) const { return !(*this == o); }
};

static LibcState libcStateAt(time_t sec) {
    auto tm = libcLocalTime(sec);
    LibcState ret = { offsetOf(tm, sec), tm.tm_isdst > 0 ? 1 : 0 };
    return ret;
}

//用libc算出[from, to)内的全部切换点：逐小时探测(偏移, 夏令时标志)，一变就逐秒回退到首秒。
//算法刻意朴素，与库里"按天粗扫再二分"的做法相互独立
//Every switch within [from, to) according to libc: probe (offset, daylight saving flag) hour by
//hour and, once it changes, step back second by second to the first second of the new state.
//Deliberately naive, so that it shares nothing with the "scan by day, then bisect" of the library
static std::vector<time_t> libcSwitches(time_t from, time_t to) {
    std::vector<time_t> ret;
    LibcState last = libcStateAt(from);
    for (time_t sec = from + 3600; sec < to; sec += 3600) {
        LibcState cur = libcStateAt(sec);
        if (cur != last) {
            time_t first = sec;
            while (libcStateAt(first - 1) == cur) {
                --first;
            }
            ret.push_back(first);
            last = cur;
        }
    }
    return ret;
}

//库在启动时算出的表必须与libc逐秒一致：切换点前后一秒、远处、过去，全部逐字段比对
//The table the library computes at start up has to agree with libc to the second: the second
//before and after every switch, far ahead, in the past, all compared field by field
static bool tableMatchesLibc(const char *tz, bool has_dst) {
    useTimezone(tz);
    time_t now = time(nullptr);
    auto switches = libcSwitches(now, daysLater(now, 2 * 366));
    if (has_dst ? switches.size() < 2 : !switches.empty()) {
        printf("[FAIL] TZ=%s 两年内找到%d个切换点，与预期不符\n", tz, (int)switches.size());
        return false;
    }
    //查询点：每个切换点T及其前一秒T-1(一段的首尾)、19年后(表的远端，证明覆盖二十年)、现在、
    //启动前200天(过去一年内)，不用改动时钟就把表的两端和每条边界都查到
    //Query points: each switch T and the second before it (both ends of a segment), 19 years
    //ahead (the far end of the table, proving twenty years of coverage), now, 200 days before
    //start up (within the past year); both ends of the table and every boundary are covered
    //without touching the clock
    std::vector<time_t> points;
    for (auto T : switches) {
        points.push_back(T);
        points.push_back(T - 1);
    }
    points.push_back(daysLater(now, 19 * 366));
    points.push_back(now);
    points.push_back(daysLater(now, -200));
    for (auto sec : points) {
        if (!matchesLibc(tz, sec)) {
            return false;
        }
    }
    return true;
}

int main() {
    //记录原有TZ，测试结束后恢复
    //Remember the original TZ and restore it when the test is over
    const char *old_tz = getenv("TZ");
    std::string saved_tz = old_tz ? old_tz : "";

    //覆盖整小时、半小时、三刻钟偏移，以及夏令时为一小时与半小时两种情形。
    //豪勋爵岛(LHS)是关键用例:它的夏令时只有半小时,按"夏令时=固定一小时"推算会错30分钟。
    //夏令时规则一律用真实的M规则，不用"J1/J365"这种全年夏令时的写法：glibc按UTC年份算J规则，
    //会在每年年末留下几小时标准时，而库按天粗扫看不见这么短的窗口，门禁就会在那几小时内失败
    //Whole hour, half hour and three quarter offsets are covered, as well as daylight saving
    //times of one hour and of half an hour. Lord Howe Island (LHS) is the crucial case: its
    //daylight saving time is only half an hour, deriving it as a fixed hour is off by 30 minutes.
    //Daylight saving rules are always real M rules, never the all-year "J1/J365" form: glibc
    //evaluates J rules in the UTC year and leaves a few hours of standard time at the end of each
    //year, a window too short for the day by day scan of the library, and the gate would fail
    //during those hours
    static const char *TIMEZONES[] = {
        "UTC0",                                        //零偏移
        "CST-8",                                       //整小时: UTC+8
        "EST5",                                        //整小时: UTC-5
        "IST-5:30",                                    //半小时: UTC+5:30
        "NPT-5:45",                                    //三刻钟: UTC+5:45
        "EST5EDT,M3.2.0,M11.1.0",                      //一小时夏令时
        "LHS-10:30LHD-11,M10.1.0,M4.1.0",              //半小时夏令时(豪勋爵岛规则)
        "CHA-12:45CHD-13:45,M9.5.0/2:45,M4.1.0/3:45",  //三刻钟偏移叠加一小时夏令时(查塔姆群岛规则)
        "NUT11",                                       //UTC-11
        "LIN-14",                                      //UTC+14
    };

    int ret = 0;
    for (size_t i = 0; i < sizeof(TIMEZONES) / sizeof(TIMEZONES[0]); ++i) {
        if (!sameAsLibc(TIMEZONES[i])) {
            ret = 1;
            break;
        }
#ifndef _WIN32
        if (!selfConsistent(TIMEZONES[i])) {
            ret = 2;
            break;
        }
#endif
    }

#ifndef _WIN32
    if (ret == 0 && !zoneStableUnderConcurrency()) {
        ret = 3;
    }
#endif

    if (ret == 0) {
        //负夏令时是把夏季时间当作标准时、冬季反而标记为夏令时的写法(如爱尔兰)
        //A negative daylight saving time treats the summer time as standard and flags winter as
        //daylight saving instead (Ireland for instance)
        static const struct {
            const char *tz;
            bool has_dst;
        } RULES[] = {
            { "EST5EDT,M3.2.0,M11.1.0", true },          //一小时夏令时
            { "LHS-10:30LHD-11,M10.1.0,M4.1.0", true },  //半小时夏令时
            { "IST-1GMT0,M10.5.0,M3.5.0/1", true },      //负夏令时
            { "CST-8", false },                          //无夏令时
        };
        for (size_t i = 0; i < sizeof(RULES) / sizeof(RULES[0]); ++i) {
            if (!tableMatchesLibc(RULES[i].tz, RULES[i].has_dst)) {
                ret = 4;
                break;
            }
        }
    }

    //恢复原有时区，避免影响同一进程内的后续代码
    //Restore the original timezone so that later code in the same process is unaffected
    if (saved_tz.empty()) {
#ifdef _WIN32
        _putenv_s("TZ", "");
        _tzset();
#else
        unsetenv("TZ");
        tzset();
#endif
        //表同样要重建，否则本进程后续(例如日志)仍用着测试期间的最后一个时区
        //The table has to be rebuilt as well, otherwise the rest of this process, logging
        //included, would keep using whatever timezone the test left behind
        local_time_init();
    } else {
        useTimezone(saved_tz.data());
    }

    if (ret == 0) {
        printf("local time regression passed\n");
    }
    return ret;
}
