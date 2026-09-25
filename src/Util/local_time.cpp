//
// Created by alex on 2022/5/29.
//

/*
 * Copyright (c) 2018, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstring>
#include <ctime>
#include <limits>

#include "local_time.h"

/* This is a safe version of localtime() which contains no locks and is
 * fork() friendly. Even the _r version of localtime() cannot be used safely
 * in Redis. Another thread may be calling localtime() while the main thread
 * forks(). Later when the child process calls localtime() again, for instance
 * in order to log something to the Redis log, it may deadlock: in the copy
 * of the address space of the forked process the lock will never be released.
 *
 * 本实现与Redis原版的区别: 原版只保存"当前"的时区偏移与夏令时标志, 要靠调用方在安全的
 * 时机定期刷新, 而且所有时间戳都按当前时刻的偏移换算。这里改为在local_time_init()里
 * 一次性把时区在一段时间范围内的全部切换点算成一张只读表, 每条时间戳按它落在哪一段
 * 取偏移: 运行期不需要任何刷新, 也不再存在"用当前偏移换算其它季节的时间戳"这一偏差;
 * 偏移直接取自libc填充的tm_gmtoff, 半小时夏令时、负夏令时等特殊规则自然正确。
 * Unlike the original Redis version, which keeps only the current offset and daylight
 * saving flag, relies on the caller to refresh them at safe moments and converts every
 * timestamp with the offset of the current moment, local_time_init() here computes every
 * switch of the timezone within a range of time into a read-only table once, and each
 * timestamp takes the offset of the segment it falls into: nothing needs refreshing at
 * run time, and a timestamp of another season is no longer converted with the offset of
 * the current moment. The offset comes straight from the tm_gmtoff filled in by libc, so
 * half hour and negative daylight saving times are correct by construction.
 *
 * Note that this function does not work for dates < 1/1/1970, it is solely
 * designed to work with what time(NULL) may return, and to support Redis
 * logging of the dates, it's not really a complete implementation. */
namespace toolkit {

/* 表的一段: 从start(含)到下一段的start(不含), 本地时间的偏移、夏令时标志与缩写保持不变
 * One segment of the table: from start (inclusive) to the start of the next segment
 * (exclusive) the offset, the daylight saving flag and the abbreviation of the local time
 * stay the same */
struct Segment {
    time_t start;
    long gmtoff;
    int isdst;
    /* 缩写保存的是内容而非localtime()给出的指针: 各家libc对该指针所指内存的生命周期约定
     * 不同, musl上TZ一变就会munmap掉(保存指针会悬垂、解引用即崩溃, 已实测); glibc则在每次
     * localtime()里持锁改写tzname[], 而本文件是不持锁读的。POSIX要求缩写至少支持6个字符,
     * 现实中也没有更长的。
     * The abbreviation is kept by content rather than as the pointer localtime() hands out:
     * libcs differ on the lifetime of that memory, on musl a change of TZ munmaps it, so a
     * saved pointer dangles and crashes on dereference (measured), and glibc rewrites
     * tzname[] under its lock on every localtime() while this file reads without any lock.
     * POSIX requires abbreviations of at least six characters to be supported, and no real
     * one is longer. */
    char zone[16];
};

/* tzdata里单个时区一年最多出现4次切换(如斋月期间暂停夏令时的Asia/Gaza), 21年不到60段, 留有余量
 * tzdata has at most four switches a year for a single zone (Asia/Gaza, which suspends daylight
 * saving time during Ramadan), under 60 segments in 21 years, with headroom to spare */
static const int kMaxSegments = 128;

/* 表与段数只在local_time_init()里写, 之后只读。用静态数组而不是堆: tm_zone直接指向表项,
 * 表的生命周期等于进程, 该指针永远有效; 并且全部是常量初始化, 不依赖各编译单元的动态
 * 初始化顺序——即便local_time_init()尚未运行, 表里也有一个可用的UTC占位项。
 * 第0项的start是time_t的最小值, 所以任何时刻都能落进某一段。
 * The table and its size are written in local_time_init() only and read-only afterwards.
 * A static array rather than the heap: tm_zone points straight into the table, whose
 * lifetime is that of the process, so the pointer stays valid forever; and everything is
 * constant initialized, so nothing depends on the dynamic initialization order across
 * translation units: even before local_time_init() has run the table holds a usable UTC
 * placeholder. The start of entry 0 is the minimum of time_t, so every moment falls into
 * some segment. */
static Segment s_segments[kMaxSegments] = { { std::numeric_limits<time_t>::min(), 0, 0, "UTC" } };
static int s_count = 1;

/* 找到时刻t所在的段: 各段start严格递增, 二分即可, 至多7次比较。表建好后是纯只读数据,
 * 查表不写任何共享状态, 多少线程同时查都互不影响, 也不需要原子量
 * Find the segment the moment t falls into: the starts strictly increase, so a binary search
 * does, seven comparisons at most. Once built the table is pure read-only data and a lookup
 * writes no shared state, so any number of threads can look it up at once, without atomics */
static const Segment &segment_at(time_t t) {
    int lo = 0, hi = s_count - 1;
    while (lo < hi) {
        int mid = lo + (hi - lo + 1) / 2;
        if (t >= s_segments[mid].start) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return s_segments[lo];
}

long get_local_gmtoff(time_t t) {
    return segment_at(t).gmtoff;
}

static int is_leap_year(time_t year) {
    if (year % 4)
        return 0; /* A year not divisible by 4 is not leap. */
    else if (year % 100)
        return 1; /* If div by 4 and not 100 is surely leap. */
    else if (year % 400)
        return 0; /* If div by 100 *and* not by 400 is not leap. */
    else
        return 1; /* If div by 100 and 400 is leap. */
}

void no_locks_localtime(struct tm *tmp, time_t t) {
    const time_t secs_min = 60;
    const time_t secs_hour = 3600;
    const time_t secs_day = 3600 * 24;

    /* 时刻、tm_gmtoff、tm_isdst与tm_zone全部取自同一段, 必定互洽
     * The broken down time, tm_gmtoff, tm_isdst and tm_zone all come from the same segment
     * and always agree with each other */
    const Segment &seg = segment_at(t);

    t += seg.gmtoff; /* Adjust for timezone and daylight time. */
    time_t days = t / secs_day; /* Days passed since epoch. */
    time_t seconds = t % secs_day; /* Remaining seconds. */

    tmp->tm_isdst = seg.isdst;
    tmp->tm_hour = seconds / secs_hour;
    tmp->tm_min = (seconds % secs_hour) / secs_min;
    tmp->tm_sec = (seconds % secs_hour) % secs_min;
#ifndef _WIN32
    tmp->tm_gmtoff = seg.gmtoff;
    /* tm_zone不填的话就是调用方栈上的未初始化指针, 一旦用%Z格式化便会读到非法内存。
     * 表项与进程同寿命, 这个指针永远有效。BSD系(含macOS)的tm_zone是char*, 故去掉const
     * Leaving tm_zone alone would keep whatever uninitialized pointer the caller has on its
     * stack, and formatting with %Z would then read invalid memory. The table entry lives as
     * long as the process, so the pointer stays valid forever. tm_zone is a char* on the
     * BSDs (macOS included), hence the const_cast */
    tmp->tm_zone = const_cast<char *>(seg.zone);
#endif
    /* 1/1/1970 was a Thursday, that is, day 4 from the POV of the tm structure
     * where sunday = 0, so to calculate the day of the week we have to add 4
     * and take the modulo by 7. */
    tmp->tm_wday = (days + 4) % 7;

    /* Calculate the current year. */
    tmp->tm_year = 1970;
    while (1) {
        /* Leap years have one day more. */
        time_t days_this_year = 365 + is_leap_year(tmp->tm_year);
        if (days_this_year > days)
            break;
        days -= days_this_year;
        tmp->tm_year++;
    }
    tmp->tm_yday = days; /* Number of day of the current year. */

    /* We need to calculate in which month and day of the month we are. To do
     * so we need to skip days according to how many days there are in each
     * month, and adjust for the leap year that has one more day in February. */
    int mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    mdays[1] += is_leap_year(tmp->tm_year);

    tmp->tm_mon = 0;
    while (days >= mdays[tmp->tm_mon]) {
        days -= mdays[tmp->tm_mon];
        tmp->tm_mon++;
    }

    tmp->tm_mday = days + 1; /* Add 1 since our 'days' is zero-based. */
    tmp->tm_year -= 1900; /* Surprisingly tm_year is year-1900. */
}

/* 取时刻t的本地时间状态; 时刻超出libc能表示的范围时返回false
 * Capture the state of the local time at the moment t; returns false when the moment is
 * outside what libc can represent */
static bool capture(time_t t, Segment &out) {
    struct tm aux;
#ifdef _WIN32
    if (localtime_s(&aux, &t) != 0) {
        return false;
    }
    /* _mkgmtime会就地改写整个struct tm, 故先把tm_isdst取出来
     * _mkgmtime rewrites the whole struct tm in place, so tm_isdst is read out first */
    out.isdst = aux.tm_isdst > 0 ? 1 : 0;
    /* Windows的struct tm没有tm_gmtoff, 把本地时间当成UTC反解即可得到偏移
     * The struct tm of Windows has no tm_gmtoff, interpreting the local time as if it were
     * UTC gives the offset back */
    time_t as_utc = _mkgmtime(&aux);
    if (as_utc == (time_t)-1) {
        return false;
    }
    out.gmtoff = (long)(as_utc - t);
    /* Windows的struct tm也没有tm_zone, %Z由CRT自己处理, 缩写留空
     * The struct tm of Windows has no tm_zone either and %Z is handled by the CRT itself,
     * the abbreviation is left empty */
    out.zone[0] = '\0';
#else
    if (!localtime_r(&t, &aux)) {
        return false;
    }
    out.isdst = aux.tm_isdst > 0 ? 1 : 0;
    out.gmtoff = aux.tm_gmtoff;
    strncpy(out.zone, aux.tm_zone ? aux.tm_zone : "", sizeof(out.zone) - 1);
    out.zone[sizeof(out.zone) - 1] = '\0';
#endif
    return true;
}

/* 两个状态是否相同; 缩写也要比, 否则会漏掉偏移不变、只有夏令时标志或缩写变化的切换
 * Whether two states are the same; the abbreviation counts as well, otherwise a switch that
 * keeps the offset and only changes the daylight saving flag or the abbreviation is missed */
static bool same_state(const Segment &a, const Segment &b) {
    return a.gmtoff == b.gmtoff && a.isdst == b.isdst && strcmp(a.zone, b.zone) == 0;
}

/* 在(lo, hi]内二分出状态首次不同于cur的那一秒。前提: lo处的状态等于cur, hi处的不等于
 * Bisect (lo, hi] for the first second whose state differs from cur, given that the state at
 * lo equals cur and the one at hi does not */
static bool find_switch(time_t lo, time_t hi, const Segment &cur, time_t &at) {
    while (hi - lo > 1) {
        time_t mid = lo + (hi - lo) / 2;
        Segment probe;
        if (!capture(mid, probe)) {
            return false;
        }
        if (same_state(probe, cur)) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    at = hi;
    return true;
}

void local_time_init() {
    const time_t day = 24 * 3600;
    /* 让TZ环境变量生效 / Make the TZ environment variable take effect */
    tzset();
    time_t now = time(nullptr);
    /* 覆盖范围: 向过去一年, 使时钟回拨或稍早的时间戳仍能换算正确; 向未来二十年, 32位time_t
     * 到2038年为止(算不到就截断)。按天粗扫, 状态一变就二分到秒, 合计约8000余次localtime(),
     * 耗时以毫秒计, 且只发生在启动时。
     * 已知局限: 在相邻两次探测之间出现又消失的短暂状态(不足一天且回到原状态)看不见。IANA时区
     * 数据里没有这种情况(相邻切换至少相隔数天); 已知的例外只有glibc对"J1/J365"这类全年夏令时
     * POSIX串在每年年末按UTC年份多算出的几小时标准时, 不值得为它把扫描粒度缩小。
     * Coverage: one year into the past, so that a clock set back or a slightly older timestamp
     * still converts correctly; twenty years into the future, truncated to 2038 on a 32 bit
     * time_t. Scan day by day and bisect to the second once the state changes: some 8000 calls
     * of localtime() in all, a matter of milliseconds and only at start up.
     * Known limit: a short-lived state that begins and ends between two probes (shorter than a
     * day, returning to the previous state) is not seen. The IANA data has no such case
     * (consecutive switches are days apart); the only known exception is the few hours of
     * standard time glibc computes at the end of each year, by UTC year, for an all-year
     * daylight saving POSIX string such as "J1/J365", not worth a finer scan */
    time_t from = now - 366 * day;
    Segment cur;
    if (!capture(from, cur)) {
        /* libc给不出结果时保留UTC占位项 / The UTC placeholder is kept when libc cannot deliver */
        return;
    }
    const time_t span = (time_t)20 * 366 * day;
    const time_t max = std::numeric_limits<time_t>::max();
    time_t end = (max - span > now) ? now + span : max;

    s_count = 0;
    cur.start = std::numeric_limits<time_t>::min();
    s_segments[s_count++] = cur;
    for (time_t t = from; t < end && s_count < kMaxSegments;) {
        time_t next = (end - t > day) ? t + day : end;
        Segment probe;
        if (!capture(next, probe)) {
            break;
        }
        if (same_state(probe, cur)) {
            t = next;
            continue;
        }
        time_t at;
        if (!find_switch(t, next, cur, at) || !capture(at, cur)) {
            break;
        }
        cur.start = at;
        s_segments[s_count++] = cur;
        /* 从切换点继续, 同一天内的第二次切换(A->B->C)也不会漏; 回到原状态的(A->B->A)见上
         * Continue from the switch, a second one within the same day (A->B->C) is not missed;
         * one returning to the previous state (A->B->A) is the limit described above */
        t = at;
    }
}
} // namespace toolkit
