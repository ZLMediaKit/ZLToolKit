/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef UTIL_LOGGER_H_
#define UTIL_LOGGER_H_

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <deque>
#include <map>
#include <ctime>
#include <string.h>
#include <cstdlib>
#include <thread>
#include <memory>
#include <mutex>
#include <time.h>
#include <condition_variable>
#include "Util/util.h"
#include "Thread/semaphore.h"

using namespace std;

namespace toolkit {

typedef enum { LTrace = 0, LDebug, LInfo, LWarn, LError} LogLevel;
static const char *LogLevelStr[] = { "T", "D", "I", "W", "E" };

#define CLEAR_COLOR "\033[0m"
#define UNDERLINE "\033[4m"

static const char *COLOR[5][2] = {
        {"\033[44;37m", "\033[34m" },
        {"\033[42;37m", "\033[32m" },
        {"\033[46;37m", "\033[36m" },
        {"\033[43;37m", "\033[33m" },
        {"\033[41;37m", "\033[31m" } };

class Logger;
class LogWriter;
class LogChannel;
class LogInfo;
class LogInfoMaker;
typedef std::shared_ptr<LogInfo> LogInfoPtr;

class LogChannel {
public:
	LogChannel(const string& name, LogLevel level = LDebug) :_name(name), _level(level) {}
	virtual ~LogChannel() {}
	virtual void write(const LogInfoPtr & stream)=0;

	const string &name() const { return _name; }
	LogLevel level() const { return _level;}
	void setLevel(LogLevel level) { _level = level; }
protected:
	string _name;
	LogLevel _level;
};

class LogWriter {
public:
	LogWriter() {}
	virtual ~LogWriter() {}
	virtual void write(const LogInfoPtr &stream) = 0;
};

class Logger : public std::enable_shared_from_this<Logger> {
public:
    friend class AsyncLogWriter;
    typedef std::shared_ptr<Logger> Ptr;

    static Logger &Instance();
    static void Destory();
    ~Logger() {
        _writer.reset();
        _channels.clear();
    }

    void add(const std::shared_ptr<LogChannel> &channel) {
        _channels[channel->name()] = channel;
    }

    void del(const string &name) {
        _channels.erase(name);
    }

    std::shared_ptr<LogChannel> get(const string &name) {
        auto it = _channels.find(name);
        if (it == _channels.end()) {
            return nullptr;
        }
        return it->second;
    }

    void setWriter(const std::shared_ptr<LogWriter> &writer) {
        _writer = writer;
    }

    void write(const LogInfoPtr &stream) {
        if (_writer) {
            _writer->write(stream);
        }else{
            writeChannels(stream);
        }
    }

    void setLevel(LogLevel level) {
        for (auto &chn : _channels) {
            chn.second->setLevel(level);
        }
    }

private:
    Logger() {}
    // Non-copyable and non-movable
    Logger(const Logger &); // = delete;
    Logger(Logger &&); // = delete;
    Logger &operator=(const Logger &); // = delete;
    Logger &operator=(Logger &&); // = delete;

   void writeChannels(const LogInfoPtr &stream){
       for (auto &chn : _channels) {
           chn.second->write(stream);
       }
   }
private:
    map<string, std::shared_ptr<LogChannel> > _channels;
    std::shared_ptr<LogWriter> _writer;
};

class LogInfo {
public:
    friend class LogInfoMaker;
    void format(ostream &ost,
                bool enableColor = true,
                bool enableDetail = true) {

        if (!enableDetail && _message.str().empty()) {
            //没有任何信息打印
            return;
        }

        if (enableDetail) {
            static string appName = exeName();
#if defined(_WIN32)
            ost << appName <<"(" << GetCurrentProcessId() << ") " << _file << " " << _line << endl;
#else
            ost << appName << "(" << getpid() << ") " << _file << " " << _line << endl;
#endif
        }

        if (enableColor) {
            ost << COLOR[_level][1];
        }

        ost << printTime(_tv) << " " << LogLevelStr[_level] << " | ";

        if (enableDetail) {
            ost << _function << " ";
        }

        ost << _message.str();

        if (enableColor) {
            ost << CLEAR_COLOR;
        }

        ost << endl;
    }

    static std::string printTime(const timeval &tv) {
        time_t sec_tmp = tv.tv_sec;
        struct tm *tm = localtime(&sec_tmp);
        char buf[128];
        snprintf(buf, sizeof(buf), "%d-%02d-%02d %02d:%02d:%02d.%03d",
                 1900 + tm->tm_year,
                 1 + tm->tm_mon,
                 tm->tm_mday,
                 tm->tm_hour,
                 tm->tm_min,
                 tm->tm_sec,
                 (int) (tv.tv_usec / 1000));
        return buf;
    }
public:
    LogLevel _level;
    int _line;
    string _file;
    string _function;
    timeval _tv;
    ostringstream _message;
private:
    LogInfo(LogLevel level,
            const char *file,
            const char *function,
            int line) :
            _level(level),
            _line(line),
            _file(file),
            _function(function) {
        gettimeofday(&_tv, NULL);
    }
};

class LogInfoMaker {
public:
    LogInfoMaker(LogLevel level,
                 const char *file,
                 const char *function,
                 int line) :
            _logInfo(new LogInfo(level, file, function, line)) {
    }

    LogInfoMaker(LogInfoMaker &&that) {
        _logInfo = that._logInfo;
        that._logInfo.reset();
    }

    LogInfoMaker(const LogInfoMaker &that) {
        _logInfo = that._logInfo;
        (const_cast<LogInfoMaker &>(that))._logInfo.reset();
    }

    ~LogInfoMaker() {
        *this << endl;
    }

    template<typename T>
    LogInfoMaker &operator<<(const T &data) {
        if (!_logInfo) {
            return *this;
        }
        _logInfo->_message << data;
        return *this;
    }

    LogInfoMaker &operator<<(const char *data) {
        if (!_logInfo) {
            return *this;
        }
        if (data) {
            _logInfo->_message << data;
        }
        return *this;
    }

    LogInfoMaker &operator<<(ostream &(*f)(ostream &)) {
        if (!_logInfo) {
            return *this;
        }
        Logger::Instance().write(_logInfo);
        _logInfo.reset();
        return *this;
    }

    void clear() {
        _logInfo.reset();
    }

private:
    LogInfoPtr _logInfo;
};

class AsyncLogWriter : public LogWriter {
public:
    AsyncLogWriter() : _exit_flag(false) {
        _logger = &Logger::Instance();
        _thread = std::make_shared<thread>([this]() { this->run(); });
    }

    ~AsyncLogWriter() {
        _exit_flag = true;
        _sem.post();
        _thread->join();
        flushAll();
    }

    virtual void write(const LogInfoPtr &stream) {
        {
            lock_guard<mutex> lock(_mutex);
            _pending.push_back(stream);
        }
        _sem.post();
    }

private:
    void run() {
        while (!_exit_flag) {
            _sem.wait();
            flushAll();
        }
    }
    void flushAll(){
        lock_guard<mutex> lock(_mutex);
        while (_pending.size()) {
            _logger->writeChannels(_pending.front());
            _pending.pop_front();
        }
    }
private:
    bool _exit_flag;
    std::shared_ptr<thread> _thread;
    deque<LogInfoPtr> _pending;
    semaphore _sem;
    mutex _mutex;
    Logger *_logger;
};

class ConsoleChannel : public LogChannel {
public:
    ConsoleChannel(const string &name,
                   LogLevel level = LDebug) :
            LogChannel(name, level) {}

    ~ConsoleChannel() {}

    virtual void write(const LogInfoPtr &logInfo) override {
        if (level() > logInfo->_level) {
            return;
        }
        logInfo->format(std::cout, true);
    }
};

class FileChannel : public LogChannel {
public:
    FileChannel(const string &name,
                const string &path,
                LogLevel level = LDebug) :
            LogChannel(name, level), _path(path) {}

    ~FileChannel() {
        close();
    }

    virtual void write(const std::shared_ptr<LogInfo> &stream) override {
        if (level() > stream->_level) {
            return;
        }
        if (!_fstream.is_open()) {
            open();
        }
        stream->format(_fstream, false);
    }

    void setPath(const string &path) {
        _path = path;
        open();
    }

    const string &path() const {
        return _path;
    }

protected:
    virtual void open() {
        // Ensure a path was set
        if (_path.empty()) {
            throw runtime_error("Log file path must be set.");
        }
        // Open the file stream
        _fstream.close();
        _fstream.open(_path.c_str(), ios::out | ios::app);
        // Throw on failure
        if (!_fstream.is_open()) {
            throw runtime_error("Failed to open log file: " + _path);
        }
    }

    virtual void close() {
        _fstream.close();
    }

protected:
    ofstream _fstream;
    string _path;
};

#define TraceL LogInfoMaker(LTrace, __FILE__,__FUNCTION__, __LINE__)
#define DebugL LogInfoMaker(LDebug, __FILE__,__FUNCTION__, __LINE__)
#define InfoL LogInfoMaker(LInfo, __FILE__,__FUNCTION__, __LINE__)
#define WarnL LogInfoMaker(LWarn,__FILE__, __FUNCTION__, __LINE__)
#define ErrorL LogInfoMaker(LError,__FILE__, __FUNCTION__, __LINE__)

} /* namespace toolkit */

#endif /* UTIL_LOGGER_H_ */
