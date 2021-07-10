/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xia-chu/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UTIL_LOGGER_H_
#define UTIL_LOGGER_H_

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <set>
#include <map>
#include <deque>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <memory>
#include <mutex>
#include "Util/util.h"
#include "Util/List.h"
#include "Thread/semaphore.h"

using namespace std;

namespace toolkit {

class LogContext;
class LogChannel;
class LogWriter;
class Logger;

typedef std::shared_ptr<LogContext> LogContextPtr;
typedef enum {
    LTrace = 0, LDebug, LInfo, LWarn, LError
} LogLevel;

Logger &getLogger();
void setLogger(Logger *logger);

/**
* 日志类
*/
class Logger : public std::enable_shared_from_this<Logger>, public noncopyable {
public:
    friend class AsyncLogWriter;
    typedef std::shared_ptr<Logger> Ptr;

    /**
     * 获取日志单例
     * @return
     */
    static Logger &Instance();

    Logger(const string &loggerName);
    ~Logger();

    /**
     * 添加日志通道，非线程安全的
     * @param channel log通道
     */
    void add(const std::shared_ptr<LogChannel> &channel);

    /**
     * 删除日志通道，非线程安全的
     * @param name log通道名
     */
    void del(const string &name);

    /**
     * 获取日志通道，非线程安全的
     * @param name log通道名
     * @return 线程通道
     */
    std::shared_ptr<LogChannel> get(const string &name);

    /**
     * 设置写log器，非线程安全的
     * @param writer 写log器
     */
    void setWriter(const std::shared_ptr<LogWriter> &writer);

    /**
     * 设置所有日志通道的log等级
     * @param level log等级
     */
    void setLevel(LogLevel level);

    /**
     * 获取logger名
     * @return logger名
     */
    const string &getName() const;

    /**
     * 写日志
     * @param ctx 日志信息
     */
    void write(const LogContextPtr &ctx);

private:
    /**
     * 写日志到各channel，仅供AsyncLogWriter调用
     * @param ctx 日志信息
     */
    void writeChannels(const LogContextPtr &ctx);

private:
    map<string, std::shared_ptr<LogChannel> > _channels;
    std::shared_ptr<LogWriter> _writer;
    string _loggerName;
};

///////////////////LogContext///////////////////
/**
* 日志上下文
*/
class LogContext : public ostringstream {
public:
    //_file,_function改成string保存，目的是有些情况下，指针可能会失效
    //比如说动态库中打印了一条日志，然后动态库卸载了，那么指向静态数据区的指针就会失效

    LogContext(LogLevel level, const char *file, const char *function, int line);
    ~LogContext() = default;

    LogLevel _level;
    int _line;
    string _file;
    string _function;
    string _thread_name;
    struct timeval _tv;
};

/**
 * 日志上下文捕获器
 */
class LogContextCapturer {
public:
    typedef std::shared_ptr<LogContextCapturer> Ptr;
    LogContextCapturer(Logger &logger, LogLevel level, const char *file, const char *function, int line);
    LogContextCapturer(const LogContextCapturer &that);
    ~LogContextCapturer();

    /**
     * 输入std::endl(回车符)立即输出日志
     * @param f std::endl(回车符)
     * @return 自身引用
     */
    LogContextCapturer &operator<<(ostream &(*f)(ostream &));

    template<typename T>
    LogContextCapturer &operator<<(T &&data) {
        if (!_ctx) {
            return *this;
        }
        (*_ctx) << std::forward<T>(data);
        return *this;
    }

    void clear();

private:
    LogContextPtr _ctx;
    Logger &_logger;
};


///////////////////LogWriter///////////////////
/**
 * 写日志器
 */
class LogWriter : public noncopyable {
public:
    LogWriter() {}
    virtual ~LogWriter() {}
    virtual void write(const LogContextPtr &ctx, Logger &logger) = 0;
};

class AsyncLogWriter : public LogWriter {
public:
    AsyncLogWriter();
    ~AsyncLogWriter();

private:
    void run();
    void flushAll();
    void write(const LogContextPtr &ctx, Logger &logger) override;

private:
    bool _exit_flag;
    mutex _mutex;
    semaphore _sem;
    std::shared_ptr<thread> _thread;
    List<std::pair<LogContextPtr,Logger *> > _pending;
};

///////////////////LogChannel///////////////////
/**
 * 日志通道
 */
class LogChannel : public noncopyable {
public:
    LogChannel(const string &name, LogLevel level = LTrace);
    virtual ~LogChannel();

    virtual void write(const Logger &logger, const LogContextPtr &ctx) = 0;
    const string &name() const;
    void setLevel(LogLevel level);
    static std::string printTime(const timeval &tv);

protected:
    /**
    * 打印日志至输出流
    * @param ost 输出流
    * @param enableColor 是否启用颜色
    * @param enableDetail 是否打印细节(函数名、源码文件名、源码行)
    */
    virtual void format(const Logger &logger, ostream &ost, const LogContextPtr &ctx, bool enableColor = true, bool enableDetail = true);

protected:
    string _name;
    LogLevel _level;
};

/**
 * 输出日志至终端，支持输出日志至android logcat
 */
class ConsoleChannel : public LogChannel {
public:
    ConsoleChannel(const string &name = "ConsoleChannel", LogLevel level = LTrace);
    ~ConsoleChannel();
    void write(const Logger &logger, const LogContextPtr &logContext) override;
};

/**
 * 输出日志至文件
 */
class FileChannelBase : public LogChannel {
public:
    FileChannelBase(const string &name = "FileChannelBase", const string &path = exePath() + ".log", LogLevel level = LTrace);
    ~FileChannelBase();

    void write(const Logger &logger, const LogContextPtr &ctx) override;
    bool setPath(const string &path);
    const string &path() const;

protected:
    virtual bool open();
    virtual void close();
    virtual size_t size();

protected:
    string _path;
    ofstream _fstream;
};

class Ticker;

/**
 * 自动清理的日志文件通道
 * 默认最多保存30天的日志
 */
class FileChannel : public FileChannelBase {
public:
    FileChannel(const string &name = "FileChannel", const string &dir = exeDir() + "log/", LogLevel level = LTrace);
    ~FileChannel() override;

    /**
     * 写日志时才会触发新建日志文件或者删除老的日志文件
     * @param logger
     * @param stream
     */
    void write(const Logger &logger, const LogContextPtr &ctx) override;

    /**
     * 设置日志最大保存天数
     * @param max_day 天数
     */
    void setMaxDay(size_t max_day);

    /**
     * 设置日志切片文件最大大小
     * @param max_size 单位MB
     */
    void setFileMaxSize(size_t max_size);

    /**
     * 设置日志切片文件最大个数
     * @param max_count 个数
     */
    void setFileMaxCount(size_t max_count);

private:
    /**
     * 删除日志切片文件，条件为超过最大保存天数与最大切片个数
     */
    void clean();

    /**
     * 检查当前日志切片文件大小，如果超过限制，则创建新的日志切片文件
     */
    void checkSize(time_t second);

    /**
     * 创建并切换到下一个日志切片文件
     */
    void changeFile(time_t second);

private:
    bool _can_write = false;
    //默认最多保存30天的日志文件
    size_t _log_max_day = 30;
    //每个日志切片文件最大默认128MB
    size_t _log_max_size = 128;
    //最多默认保持30个日志切片文件
    size_t _log_max_count = 30;
    //当前日志切片文件索引
    size_t _index = 0;
    int64_t _last_day = -1;
    time_t _last_check_time = 0;
    string _dir;
    set<string> _log_file_map;
};

#if defined(__MACH__) || ((defined(__linux) || defined(__linux__)) && !defined(ANDROID))
class SysLogChannel : public LogChannel {
public:
    SysLogChannel(const string &name = "SysLogChannel" , LogLevel level = LTrace) ;
    ~SysLogChannel();
    void write(const Logger &logger , const LogContextPtr &logContext) override;
};
#endif//#if defined(__MACH__) || ((defined(__linux) || defined(__linux__)) &&  !defined(ANDROID))

//可重置默认值
extern Logger *g_defaultLogger;
#define TraceL LogContextCapturer(getLogger(), LTrace, __FILE__,__FUNCTION__, __LINE__)
#define DebugL LogContextCapturer(getLogger(),LDebug, __FILE__,__FUNCTION__, __LINE__)
#define InfoL LogContextCapturer(getLogger(),LInfo, __FILE__,__FUNCTION__, __LINE__)
#define WarnL LogContextCapturer(getLogger(),LWarn,__FILE__, __FUNCTION__, __LINE__)
#define ErrorL LogContextCapturer(getLogger(),LError,__FILE__, __FUNCTION__, __LINE__)
#define WriteL(level) LogContextCapturer(getLogger(),level,__FILE__, __FUNCTION__, __LINE__)
} /* namespace toolkit */
#endif /* UTIL_LOGGER_H_ */
