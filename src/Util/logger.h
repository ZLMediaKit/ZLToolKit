/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
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
typedef std::shared_ptr<LogContext> LogContextPtr;
typedef enum { LTrace = 0, LDebug, LInfo, LWarn, LError} LogLevel;

/**
 * 日志类
 */
class Logger : public std::enable_shared_from_this<Logger> , public noncopyable {
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
class LogContext : public ostringstream{
    //_file,_function改成string保存，目的是有些情况下，指针可能会失效
    //比如说动态库中打印了一条日志，然后动态库卸载了，那么指向静态数据区的指针就会失效
public:
    LogContext(LogLevel level,const char *file,const char *function,int line);
    ~LogContext() = default;
    LogLevel _level;
    int _line;
    string _file;
    string _function;
    struct timeval _tv;
};

/**
 * 日志上下文捕获器
 */
class LogContextCapturer {
public:
    typedef std::shared_ptr<LogContextCapturer> Ptr;
    LogContextCapturer(Logger &logger,LogLevel level, const char *file, const char *function, int line);
    LogContextCapturer(const LogContextCapturer &that);

    ~LogContextCapturer();

    /**
     * 输入std::endl(回车符)立即输出日志
     * @param f std::endl(回车符)
     * @return 自身引用
     */
    LogContextCapturer &operator << (ostream &(*f)(ostream &));

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
    virtual void write(const LogContextPtr &ctx) = 0;
};

class AsyncLogWriter : public LogWriter {
public:
    AsyncLogWriter(Logger &logger = Logger::Instance());
    ~AsyncLogWriter();
private:
    void run();
    void flushAll();
    void write(const LogContextPtr &ctx) override ;
private:
    bool _exit_flag;
    std::shared_ptr<thread> _thread;
    List<LogContextPtr> _pending;
    semaphore _sem;
    mutex _mutex;
    Logger &_logger;
};

///////////////////LogChannel///////////////////
/**
 * 日志通道
 */
class LogChannel : public noncopyable{
public:
    LogChannel(const string& name, LogLevel level = LTrace);
    virtual ~LogChannel();
    virtual void write(const Logger &logger,const LogContextPtr & ctx) = 0;
    const string &name() const ;
    void setLevel(LogLevel level);
    static std::string printTime(const timeval &tv);
protected:
    /**
    * 打印日志至输出流
    * @param ost 输出流
    * @param enableColor 是否启用颜色
    * @param enableDetail 是否打印细节(函数名、源码文件名、源码行)
    */
    virtual void format(const Logger &logger, ostream &ost, const LogContextPtr & ctx, bool enableColor = true, bool enableDetail = true);
protected:
    string _name;
    LogLevel _level;
};

/**
 * 输出日志至终端，支持输出日志至android logcat
 */
class ConsoleChannel : public LogChannel {
public:
    ConsoleChannel(const string &name = "ConsoleChannel" , LogLevel level = LTrace) ;
    ~ConsoleChannel();
    void write(const Logger &logger , const LogContextPtr &logContext) override;
};

/**
 * 输出日志至文件
 */
class FileChannelBase : public LogChannel {
public:
    FileChannelBase(const string &name = "FileChannelBase",const string &path = exePath() + ".log", LogLevel level = LTrace);
    ~FileChannelBase();

    void write(const Logger &logger , const LogContextPtr &ctx) override;
    bool setPath(const string &path);
    const string &path() const;
protected:
    virtual bool open();
    virtual void close();
protected:
    ofstream _fstream;
    string _path;
};

/**
 * 自动清理的日志文件通道
 * 默认最多保存30天的日志
 */
class FileChannel : public FileChannelBase {
public:
    FileChannel(const string &name = "FileChannel",const string &dir = exeDir() + "log/", LogLevel level = LTrace);
    ~FileChannel() override;

    /**
     * 写日志时才会触发新建日志文件或者删除老的日志文件
     * @param logger
     * @param stream
     */
    void write(const Logger &logger , const LogContextPtr &ctx) override;

    /**
     * 设置日志最大保存天数
     * @param max_day
     */
    void setMaxDay(int max_day);
private:
    /**
     * 获取1970年以来的第几天
     * @param second
     * @return
     */
    int64_t getDay(time_t second);
    /**
     * 删除老文件
     */
    void clean();
private:
    bool _canWrite = false;
    string _dir;
    int64_t _last_day = -1;
    map<uint64_t,string> _log_file_map;
    int _log_max_day = 30;
};



#if defined(__MACH__) || ((defined(__linux) || defined(__linux__)) &&  !defined(ANDROID))
class SysLogChannel : public LogChannel {
public:
    SysLogChannel(const string &name = "SysLogChannel" , LogLevel level = LTrace) ;
    ~SysLogChannel();
    void write(const Logger &logger , const LogContextPtr &logContext) override;
};
#endif//#if defined(__MACH__) || ((defined(__linux) || defined(__linux__)) &&  !defined(ANDROID))

//可重置默认值
extern Logger* g_defaultLogger;

#define TraceL LogContextCapturer(*g_defaultLogger, LTrace, __FILE__,__FUNCTION__, __LINE__)
#define DebugL LogContextCapturer(*g_defaultLogger,LDebug, __FILE__,__FUNCTION__, __LINE__)
#define InfoL LogContextCapturer(*g_defaultLogger,LInfo, __FILE__,__FUNCTION__, __LINE__)
#define WarnL LogContextCapturer(*g_defaultLogger,LWarn,__FILE__, __FUNCTION__, __LINE__)
#define ErrorL LogContextCapturer(*g_defaultLogger,LError,__FILE__, __FUNCTION__, __LINE__)
#define WriteL(level) LogContextCapturer(*g_defaultLogger,level,__FILE__, __FUNCTION__, __LINE__)

} /* namespace toolkit */
#endif /* UTIL_LOGGER_H_ */
