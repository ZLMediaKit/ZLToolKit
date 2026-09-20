//
// Created by alex on 2022/5/29.
//

#ifndef UTIL_LOCALTIME_H
#define UTIL_LOCALTIME_H
#include <time.h>

namespace toolkit {
void no_locks_localtime(struct tm *tmp, time_t t);
void local_time_init();
/* 刷新缓存的夏令时状态，使夏令时切换后的本地时间依然正确。
 * 该函数内部调用localtime()，会加锁且不是fork安全的，
 * 故调用方必须避开热点路径并控制调用频次。
 * Refresh the cached daylight saving time flag, so that the local time stays
 * correct after a daylight saving time switch. This function calls localtime()
 * internally, which takes a lock and is not fork() friendly, so callers must
 * keep it out of the hot path and call it at a low rate. */
void local_time_refresh();
int get_daylight_active();
/* 获取本地时间相对UTC的偏移，单位为秒，已包含夏令时修正
 * Offset of the local time from UTC in seconds, daylight saving time included. */
long get_local_gmtoff();

} // namespace toolkit
#endif // UTIL_LOCALTIME_H
