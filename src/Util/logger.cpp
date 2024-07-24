/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <sys/stat.h>
#include <cstdarg>
#include <iostream>
#include "logger.h"
#include "onceToken.h"
#include "File.h"
#include "NoticeCenter.h"

#if defined(_WIN32)
#include "strptime_win.h"
#endif
#ifdef ANDROID
#include <android/log.h>
#endif //ANDROID

#if defined(__MACH__) || ((defined(__linux) || defined(__linux__)) && !defined(ANDROID))
#include <sys/syslog.h>
#endif

using namespace std;

namespace toolkit {
#ifdef _WIN32
#define CLEAR_COLOR 7
static const WORD LOG_CONST_TABLE[][3] = {
        {0x97, 0x09 , 'T'},//蓝底灰字，黑底蓝字，window console默认黑底
        {0xA7, 0x0A , 'D'},//绿底灰字，黑底绿字
        {0xB7, 0x0B , 'I'},//天蓝底灰字，黑底天蓝字
        {0xE7, 0x0E , 'W'},//黄底灰字，黑底黄字
        {0xC7, 0x0C , 'E'} };//红底灰字，黑底红字

bool SetConsoleColor(WORD Color)
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == 0)
        return false;

    BOOL ret = SetConsoleTextAttribute(handle, Color);
    return(ret == TRUE);
}
#else
#define CLEAR_COLOR "\033[0m"
static const char *LOG_CONST_TABLE[][3] = {
        {"\033[44;37m", "\033[34m", "T"},
        {"\033[42;37m", "\033[32m", "D"},
        {"\033[46;37m", "\033[36m", "I"},
        {"\033[43;37m", "\033[33m", "W"},
        {"\033[41;37m", "\033[31m", "E"}};
#endif

Logger *g_defaultLogger = nullptr;

Logger &getLogger() {
    if (!g_defaultLogger) {
        g_defaultLogger = &Logger::Instance();
    }
    return *g_defaultLogger;
}

void setLogger(Logger *logger) {
    g_defaultLogger = logger;
}

///////////////////Logger///////////////////

INSTANCE_IMP(Logger, exeName())

Logger::Logger(const string &loggerName) {
    _logger_name = loggerName;
    _last_log = std::make_shared<LogContext>();
    _default_channel = std::make_shared<ConsoleChannel>("default", LTrace);

#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif
}

Logger::~Logger() {
    _writer.reset();
    {
        LogContextCapture(*this, LInfo, __FILE__, __FUNCTION__, __LINE__);
    }
    _channels.clear();
}

void Logger::add(const std::shared_ptr<LogChannel> &channel) {
    _channels[channel->name()] = channel;
}

void Logger::del(const string &name) {
    _channels.erase(name);
}

std::shared_ptr<LogChannel> Logger::get(const string &name) {
    auto it = _channels.find(name);
    if (it == _channels.end()) {
        return nullptr;
    }
    return it->second;
}

void Logger::setWriter(const std::shared_ptr<LogWriter> &writer) {
    _writer = writer;
}

void Logger::write(const LogContextPtr &ctx) {
    if (_writer) {
        _writer->write(ctx, *this);
    } else {
        writeChannels(ctx);
    }
}

void Logger::setLevel(LogLevel level) {
    for (auto &chn : _channels) {
        chn.second->setLevel(level);
    }
}

void Logger::writeChannels_l(const LogContextPtr &ctx) {
    if (_channels.empty()) {
        _default_channel->write(*this, ctx);
    } else {
        for (auto &chn : _channels) {
            chn.second->write(*this, ctx);
        }
    }
    _last_log = ctx;
    _last_log->_repeat = 0;
}

//返回毫秒
static int64_t timevalDiff(struct timeval &a, struct timeval &b) {
    return (1000 * (b.tv_sec - a.tv_sec)) + ((b.tv_usec - a.tv_usec) / 1000);
}

void Logger::writeChannels(const LogContextPtr &ctx) {
    if (ctx->_line == _last_log->_line && ctx->_file == _last_log->_file && ctx->str() == _last_log->str() && ctx->_thread_name == _last_log->_thread_name) {
        //重复的日志每隔500ms打印一次，过滤频繁的重复日志
        ++_last_log->_repeat;
        if (timevalDiff(_last_log->_tv, ctx->_tv) > 500) {
            ctx->_repeat = _last_log->_repeat;
            writeChannels_l(ctx);
        }
        return;
    }
    if (_last_log->_repeat) {
        writeChannels_l(_last_log);
    }
    writeChannels_l(ctx);
}

const string &Logger::getName() const {
    return _logger_name;
}

///////////////////LogContext///////////////////
static inline const char *getFileName(const char *file) {
    auto pos = strrchr(file, '/');
#ifdef _WIN32
    if(!pos){
        pos = strrchr(file, '\\');
    }
#endif
    return pos ? pos + 1 : file;
}

static inline const char *getFunctionName(const char *func) {
#ifndef _WIN32
    return func;
#else
    auto pos = strrchr(func, ':');
    return pos ? pos + 1 : func;
#endif
}

LogContext::LogContext(LogLevel level, const char *file, const char *function, int line, const char *module_name, const char *flag)
        : _level(level), _line(line), _file(getFileName(file)), _function(getFunctionName(function)),
          _module_name(module_name), _flag(flag) {
    gettimeofday(&_tv, nullptr);
    _thread_name = getThreadName();
}

const string &LogContext::str() {
    if (_got_content) {
        return _content;
    }
    _content = ostringstream::str();
    _got_content = true;
    return _content;
}

///////////////////AsyncLogWriter///////////////////

static string s_module_name = exeName(false);

LogContextCapture::LogContextCapture(Logger &logger, LogLevel level, const char *file, const char *function, int line, const char *flag) :
        _ctx(new LogContext(level, file, function, line, s_module_name.c_str() ? s_module_name.c_str() : "", flag)), _logger(logger) {
}

LogContextCapture::LogContextCapture(const LogContextCapture &that) : _ctx(that._ctx), _logger(that._logger) {
    const_cast<LogContextPtr &>(that._ctx).reset();
}

LogContextCapture::~LogContextCapture() {
    *this << endl;
}

LogContextCapture &LogContextCapture::operator<<(ostream &(*f)(ostream &)) {
    if (!_ctx) {
        return *this;
    }
    _logger.write(_ctx);
    _ctx.reset();
    return *this;
}

void LogContextCapture::clear() {
    _ctx.reset();
}

///////////////////AsyncLogWriter///////////////////

AsyncLogWriter::AsyncLogWriter() : _exit_flag(false) {
    _thread = std::make_shared<thread>([this]() { this->run(); });
}

AsyncLogWriter::~AsyncLogWriter() {
    _exit_flag = true;
    _sem.post();
    _thread->join();
    flushAll();
}

void AsyncLogWriter::write(const LogContextPtr &ctx, Logger &logger) {
    {
        lock_guard<mutex> lock(_mutex);
        _pending.emplace_back(std::make_pair(ctx, &logger));
    }
    _sem.post();
}

void AsyncLogWriter::run() {
    setThreadName("async log");
    while (!_exit_flag) {
        _sem.wait();
        flushAll();
    }
}

void AsyncLogWriter::flushAll() {
    decltype(_pending) tmp;
    {
        lock_guard<mutex> lock(_mutex);
        tmp.swap(_pending);
    }

    tmp.for_each([&](std::pair<LogContextPtr, Logger *> &pr) {
        pr.second->writeChannels(pr.first);
    });
}

///////////////////EventChannel////////////////////

const string EventChannel::kBroadcastLogEvent = "kBroadcastLogEvent";

EventChannel::EventChannel(const string &name, LogLevel level) : LogChannel(name, level) {}

void EventChannel::write(const Logger &logger, const LogContextPtr &ctx) {
    if (_level > ctx->_level) {
        return;
    }
    NOTICE_EMIT(BroadcastLogEventArgs, kBroadcastLogEvent, logger, ctx);
}

const std::string &EventChannel::getBroadcastLogEventName() { return kBroadcastLogEvent;}

///////////////////ConsoleChannel///////////////////

ConsoleChannel::ConsoleChannel(const string &name, LogLevel level) : LogChannel(name, level) {}

void ConsoleChannel::write(const Logger &logger, const LogContextPtr &ctx) {
    if (_level > ctx->_level) {
        return;
    }

#if defined(OS_IPHONE)
    //ios禁用日志颜色
    format(logger, std::cout, ctx, false);
#elif defined(ANDROID)
    static android_LogPriority LogPriorityArr[10];
    static onceToken s_token([](){
        LogPriorityArr[LTrace] = ANDROID_LOG_VERBOSE;
        LogPriorityArr[LDebug] = ANDROID_LOG_DEBUG;
        LogPriorityArr[LInfo] = ANDROID_LOG_INFO;
        LogPriorityArr[LWarn] = ANDROID_LOG_WARN;
        LogPriorityArr[LError] = ANDROID_LOG_ERROR;
    });
    __android_log_print(LogPriorityArr[ctx->_level],"JNI","%s %s",ctx->_function.data(),ctx->str().data());
#else
    //linux/windows日志启用颜色并显示日志详情
    format(logger, std::cout, ctx);
#endif
}

///////////////////SysLogChannel///////////////////

#if defined(__MACH__) || ((defined(__linux) || defined(__linux__)) && !defined(ANDROID))

SysLogChannel::SysLogChannel(const string &name, LogLevel level) : LogChannel(name, level) {}

void SysLogChannel::write(const Logger &logger, const LogContextPtr &ctx) {
    if (_level > ctx->_level) {
        return;
    }
    static int s_syslog_lev[10];
    static onceToken s_token([]() {
        s_syslog_lev[LTrace] = LOG_DEBUG;
        s_syslog_lev[LDebug] = LOG_INFO;
        s_syslog_lev[LInfo] = LOG_NOTICE;
        s_syslog_lev[LWarn] = LOG_WARNING;
        s_syslog_lev[LError] = LOG_ERR;
    }, nullptr);

    syslog(s_syslog_lev[ctx->_level], "-> %s %d\r\n", ctx->_file.data(), ctx->_line);
    syslog(s_syslog_lev[ctx->_level], "## %s %s | %s %s\r\n", printTime(ctx->_tv).data(),
           LOG_CONST_TABLE[ctx->_level][2], ctx->_function.data(), ctx->str().data());
}

#endif//#if defined(__MACH__) || ((defined(__linux) || defined(__linux__)) &&  !defined(ANDROID))

///////////////////LogChannel///////////////////
LogChannel::LogChannel(const string &name, LogLevel level) : _name(name), _level(level) {}

LogChannel::~LogChannel() {}

const string &LogChannel::name() const { return _name; }

void LogChannel::setLevel(LogLevel level) { _level = level; }

std::string LogChannel::printTime(const timeval &tv) {
    auto tm = getLocalTime(tv.tv_sec);
    char buf[128];
    snprintf(buf, sizeof(buf), "%d-%02d-%02d %02d:%02d:%02d.%03d",
             1900 + tm.tm_year,
             1 + tm.tm_mon,
             tm.tm_mday,
             tm.tm_hour,
             tm.tm_min,
             tm.tm_sec,
             (int) (tv.tv_usec / 1000));
    return buf;
}

#ifdef _WIN32
#define printf_pid() GetCurrentProcessId()
#else
#define printf_pid() getpid()
#endif

void LogChannel::format(const Logger &logger, ostream &ost, const LogContextPtr &ctx, bool enable_color, bool enable_detail) {
    if (!enable_detail && ctx->str().empty()) {
        // 没有任何信息打印
        return;
    }

    if (enable_color) {
        // color console start
#ifdef _WIN32
        SetConsoleColor(LOG_CONST_TABLE[ctx->_level][1]);
#else
        ost << LOG_CONST_TABLE[ctx->_level][1];
#endif
    }

    // print log time and level
#ifdef _WIN32
    ost << printTime(ctx->_tv) << " " << (char)LOG_CONST_TABLE[ctx->_level][2] << " ";
#else
    ost << printTime(ctx->_tv) << " " << LOG_CONST_TABLE[ctx->_level][2] << " ";
#endif

    if (enable_detail) {
        // tag or process name
        ost << "[" << (!ctx->_flag.empty() ? ctx->_flag : logger.getName()) << "] ";
        // pid and thread_name
        ost << "[" << printf_pid() << "-" << ctx->_thread_name << "] ";
        // source file location
        ost << ctx->_file << ":" << ctx->_line << " " << ctx->_function << " | ";
    }

    // log content
    ost << ctx->str();

    if (enable_color) {
        // color console end
#ifdef _WIN32
        SetConsoleColor(CLEAR_COLOR);
#else
        ost << CLEAR_COLOR;
#endif
    }

    if (ctx->_repeat > 1) {
        // log repeated
        ost << "\r\n    Last message repeated " << ctx->_repeat << " times";
    }

    // flush log and new line
    ost << endl;
}

///////////////////FileChannelBase///////////////////

FileChannelBase::FileChannelBase(const string &name, const string &path, LogLevel level) : LogChannel(name, level), _path(path) {}

FileChannelBase::~FileChannelBase() {
    close();
}

void FileChannelBase::write(const Logger &logger, const std::shared_ptr<LogContext> &ctx) {
    if (_level > ctx->_level) {
        return;
    }
    if (!_fstream.is_open()) {
        open();
    }
    //打印至文件，不启用颜色
    format(logger, _fstream, ctx, false);
}

bool FileChannelBase::setPath(const string &path) {
    _path = path;
    return open();
}

const string &FileChannelBase::path() const {
    return _path;
}

bool FileChannelBase::open() {
    // Ensure a path was set
    if (_path.empty()) {
        throw runtime_error("Log file path must be set");
    }
    // Open the file stream
    _fstream.close();
#if !defined(_WIN32)
    //创建文件夹
    File::create_path(_path, S_IRWXO | S_IRWXG | S_IRWXU);
#else
    File::create_path(_path,0);
#endif
    _fstream.open(_path.data(), ios::out | ios::app);
    if (!_fstream.is_open()) {
        return false;
    }
    //打开文件成功
    return true;
}

void FileChannelBase::close() {
    _fstream.close();
}

size_t FileChannelBase::size() {
    return (_fstream << std::flush).tellp();
}

///////////////////FileChannel///////////////////

static const auto s_second_per_day = 24 * 60 * 60;

//根据GMT UNIX时间戳生产日志文件名
static string getLogFilePath(const string &dir, time_t second, int32_t index) {
    auto tm = getLocalTime(second);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d-%02d-%02d_%02d.log", 1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday, index);
    return dir + buf;
}

//根据日志文件名返回GMT UNIX时间戳
static time_t getLogFileTime(const string &full_path) {
    auto name = getFileName(full_path.data());
    struct tm tm{0};
    if (!strptime(name, "%Y-%m-%d", &tm)) {
        return 0;
    }
    //此函数会把本地时间转换成GMT时间戳
    return mktime(&tm);
}

//获取1970年以来的第几天
static uint64_t getDay(time_t second) {
    return (second + getGMTOff()) / s_second_per_day;
}

FileChannel::FileChannel(const string &name, const string &dir, LogLevel level) : FileChannelBase(name, "", level) {
    _dir = dir;
    if (_dir.back() != '/') {
        _dir.append("/");
    }

    //收集所有日志文件
    File::scanDir(_dir, [this](const string &path, bool isDir) -> bool {
        if (!isDir && end_with(path, ".log")) {
            _log_file_map.emplace(path);
        }
        return true;
    }, false);

    //获取今天日志文件的最大index号
    auto log_name_prefix = getTimeStr("%Y-%m-%d_");
    for (auto it = _log_file_map.begin(); it != _log_file_map.end(); ++it) {
        auto name = getFileName(it->data());
        //筛选出今天所有的日志文件
        if (start_with(name, log_name_prefix)) {
            int tm_mday;  // day of the month - [1, 31]
            int tm_mon;   // months since January - [0, 11]
            int tm_year;  // years since 1900
            uint32_t index;
            //今天第几个文件
            int count = sscanf(name, "%d-%02d-%02d_%d.log", &tm_year, &tm_mon, &tm_mday, &index);
            if (count == 4) {
                _index = index >= _index ? index : _index;
            }
        }
    }
}

void FileChannel::write(const Logger &logger, const LogContextPtr &ctx) {
    //GMT UNIX时间戳
    time_t second = ctx->_tv.tv_sec;
    //这条日志所在第几天
    auto day = getDay(second);
    if ((int64_t) day != _last_day) {
        if (_last_day != -1) {
            //重置日志index
            _index = 0;
        }
        //这条日志是新的一天，记录这一天
        _last_day = day;
        //获取日志当天对应的文件，每天可能有多个日志切片文件
        changeFile(second);
    } else {
        //检查该天日志是否需要重新分片
        checkSize(second);
    }

    //写日志
    if (_can_write) {
        FileChannelBase::write(logger, ctx);
    }
}

void FileChannel::clean() {
    //获取今天是第几天
    auto today = getDay(time(nullptr));
    //遍历所有日志文件，删除超过若干天前的过期日志文件
    for (auto it = _log_file_map.begin(); it != _log_file_map.end();) {
        auto day = getDay(getLogFileTime(it->data()));
        if (today < day + _log_max_day) {
            //这个日志文件距今尚未超过一定天数,后面的文件更新，所以停止遍历
            break;
        }
        //这个文件距今超过了一定天数，则删除文件
        File::delete_file(*it);
        //删除这条记录
        it = _log_file_map.erase(it);
    }

    //按文件个数清理，限制最大文件切片个数
    while (_log_file_map.size() > _log_max_count) {
        auto it = _log_file_map.begin();
        if (*it == path()) {
            //当前文件，停止删除
            break;
        }
        //删除文件
        File::delete_file(*it);
        //删除这条记录
        _log_file_map.erase(it);
    }
}

void FileChannel::checkSize(time_t second) {
    //每60秒检查一下文件大小，防止频繁flush日志文件
    if (second - _last_check_time > 60) {
        if (FileChannelBase::size() > _log_max_size * 1024 * 1024) {
            changeFile(second);
        }
        _last_check_time = second;
    }
}

void FileChannel::changeFile(time_t second) {
    auto log_file = getLogFilePath(_dir, second, _index++);
    //记录所有的日志文件，以便后续删除老的日志
    _log_file_map.emplace(log_file);
    //打开新的日志文件
    _can_write = setPath(log_file);
    if (!_can_write) {
        ErrorL << "Failed to open log file: " << _path;
    }
    //尝试删除过期的日志文件
    clean();
}

void FileChannel::setMaxDay(size_t max_day) {
    _log_max_day = max_day > 1 ? max_day : 1;
}

void FileChannel::setFileMaxSize(size_t max_size) {
    _log_max_size = max_size > 1 ? max_size : 1;
}

void FileChannel::setFileMaxCount(size_t max_count) {
    _log_max_count = max_count > 1 ? max_count : 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void LoggerWrapper::printLogV(Logger &logger, int level, const char *file, const char *function, int line, const char *fmt, va_list ap) {
    LogContextCapture info(logger, (LogLevel) level, file, function, line);
    char *str = nullptr;
    if (vasprintf(&str, fmt, ap) >= 0 && str) {
        info << str;
#ifdef ASAN_USE_DELETE
        delete [] str; // 开启asan后，用free会卡死
#else
        free(str);
#endif
    }
}

void LoggerWrapper::printLog(Logger &logger, int level, const char *file, const char *function, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printLogV(logger, level, file, function, line, fmt, ap);
    va_end(ap);
}

} /* namespace toolkit */

