//
// Created by alex on 2022/5/29.
//

#ifndef UTIL_LOCALTIME_H
#define UTIL_LOCALTIME_H
#include <time.h>

namespace toolkit {
void no_locks_localtime(struct tm *tmp, time_t t);
/* 把本地时区在[启动前一年, 启动后二十年]内的全部切换点(夏令时起止、偏移或缩写的变化)
 * 算成一张只读表, 此后no_locks_localtime()与get_local_gmtoff()只查表, 不再调用libc。
 * 本函数内部调用tzset()/localtime(), 会取libc的时区锁; 重建表时也不允许有其它线程在读表。
 * 故它只应在进程的静态初始化阶段调用一次, 或者在确知没有其它线程的时候调用。
 * 运行期修改时区或升级时区数据库的程序需要重启; 早于表起点的时刻按首段换算, 晚于终点的按末段。
 * 相邻两次按天探测之间出现又消失的短暂状态不会进表(IANA时区数据里没有这种情况)。
 * Compute every switch of the local timezone (start and end of daylight saving time, any
 * change of offset or abbreviation) within [one year before start up, twenty years after]
 * into a read-only table; no_locks_localtime() and get_local_gmtoff() only look that table
 * up afterwards and never call into libc again. This function calls tzset()/localtime()
 * and therefore takes the timezone lock of libc, and no other thread may read the table
 * while it is being rebuilt. Hence it is meant to be called once during the static
 * initialization of the process, or when no other thread is known to exist. A program
 * that changes its timezone or updates the timezone database at run time needs a restart;
 * a moment before the table is converted with its first segment, one beyond it with the last.
 * A short-lived state that begins and ends between two daily probes does not make it into the
 * table (the IANA data has no such case). */
void local_time_init();
/* 时刻t的本地时间相对UTC的偏移, 单位为秒, 已包含夏令时修正
 * Offset of the local time from UTC at the moment t in seconds, daylight saving time included. */
long get_local_gmtoff(time_t t);

} // namespace toolkit
#endif // UTIL_LOCALTIME_H
