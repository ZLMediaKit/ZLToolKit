/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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


#include "logger.h"
#include "onceToken.h"

namespace toolkit {


#define CLEAR_COLOR "\033[0m"
static const char *LOG_CONST_TABLE[][3] = {
        {"\033[44;37m", "\033[34m" , "T"},
        {"\033[42;37m", "\033[32m" , "D"},
        {"\033[46;37m", "\033[36m" , "I"},
        {"\033[43;37m", "\033[33m" , "W"},
        {"\033[41;37m", "\033[31m" , "E"}};

///////////////////Logger///////////////////
INSTANCE_IMP(Logger,exeName());

Logger::Logger(const string &loggerName) {
    _loggerName = loggerName;
}
Logger::~Logger() {
    _writer.reset();
    {
        LogContextCapturer(*this, LInfo, __FILE__, __FUNCTION__, __LINE__);
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

void Logger::write(const LogContextPtr &logContext) {
    if (_writer) {
        _writer->write(logContext);
    }else{
        writeChannels(logContext);
    }
}

void Logger::setLevel(LogLevel level) {
    for (auto &chn : _channels) {
        chn.second->setLevel(level);
    }
}

void Logger::writeChannels(const LogContextPtr &logContext){
    for (auto &chn : _channels) {
        chn.second->write(*this,logContext);
    }
}

const string &Logger::getName() const{
    return _loggerName;
}
///////////////////LogContext///////////////////
LogContext::LogContext(LogLevel level,
        const char *file,
        const char *function,
        int line) :
        _level(level),
        _line(line),
        _file(file),
        _function(function) {
    gettimeofday(&_tv, NULL);
}

///////////////////AsyncLogWriter///////////////////
LogContextCapturer::LogContextCapturer(
        Logger &logger,
        LogLevel level,
        const char *file,
        const char *function,
        int line) :
        _logContext(new LogContext(level, file, function, line)),_logger(logger) {
}


LogContextCapturer::LogContextCapturer(const LogContextCapturer &that): _logContext(that._logContext),_logger(that._logger) {
    const_cast<LogContextPtr&>(that._logContext).reset();
}

LogContextCapturer::~LogContextCapturer() {
    *this << endl;
}

LogContextCapturer &LogContextCapturer::operator << (ostream &(*f)(ostream &)) {
    if (!_logContext) {
        return *this;
    }
    _logger.write(_logContext);
    _logContext.reset();
    return *this;
}

void LogContextCapturer::clear() {
    _logContext.reset();
}

///////////////////AsyncLogWriter///////////////////
AsyncLogWriter::AsyncLogWriter(Logger &logger) : _exit_flag(false),_logger(logger) {
    _thread = std::make_shared<thread>([this]() { this->run(); });
}

AsyncLogWriter::~AsyncLogWriter() {
    _exit_flag = true;
    _sem.post();
    _thread->join();
    flushAll();
}

void AsyncLogWriter::write(const LogContextPtr &logContext) {
    {
        lock_guard<mutex> lock(_mutex);
        _pending.emplace_back(logContext);
    }
    _sem.post();
}

void AsyncLogWriter::run() {
    while (!_exit_flag) {
        _sem.wait();
        flushAll();
    }
}
void AsyncLogWriter::flushAll(){
    List<LogContextPtr> tmp;
    {
        lock_guard<mutex> lock(_mutex);
        tmp.swap(_pending);
    }

    tmp.for_each([&](const LogContextPtr &ctx){
        _logger.writeChannels(ctx);
    });

}


///////////////////ConsoleChannel///////////////////

#ifdef ANDROID
#include <android/log.h>
#endif //ANDROID

ConsoleChannel::ConsoleChannel(const string &name, LogLevel level) : LogChannel(name, level) {}
ConsoleChannel:: ~ConsoleChannel() {}
void ConsoleChannel::write(const Logger &logger,const LogContextPtr &logContext)  {
    if (_level > logContext->_level) {
        return;
    }

#if defined(_WIN32) || defined(OS_IPHONE)
    format(logger,std::cout, logContext , false);
#elif defined(ANDROID)
    static android_LogPriority LogPriorityArr[10];
    static onceToken s_token([](){
        LogPriorityArr[LTrace] = ANDROID_LOG_VERBOSE;
        LogPriorityArr[LDebug] = ANDROID_LOG_DEBUG;
        LogPriorityArr[LInfo] = ANDROID_LOG_INFO;
        LogPriorityArr[LWarn] = ANDROID_LOG_WARN;
        LogPriorityArr[LError] = ANDROID_LOG_ERROR;
    }, nullptr);
    __android_log_print(LogPriorityArr[logContext->_level],"JNI","%s %s",logContext->_function,logContext->str().c_str());
#else
    format(logger,std::cout,logContext, true);
#endif
}


///////////////////SysLogChannel///////////////////
#if defined(__MACH__) || ((defined(__linux) || defined(__linux__)) &&  !defined(ANDROID))
#include <sys/syslog.h>
SysLogChannel::SysLogChannel(const string &name, LogLevel level) : LogChannel(name, level) {
}
SysLogChannel:: ~SysLogChannel() {
}
void SysLogChannel::write(const Logger &logger,const LogContextPtr &logContext)  {
    if (_level > logContext->_level) {
        return;
    }
    static int s_syslog_lev[10];
    static onceToken s_token([](){
        s_syslog_lev[LTrace] = LOG_DEBUG;
        s_syslog_lev[LDebug] = LOG_INFO;
        s_syslog_lev[LInfo] = LOG_NOTICE;
        s_syslog_lev[LWarn] = LOG_WARNING;
        s_syslog_lev[LError] = LOG_ERR;
    }, nullptr);

    syslog(s_syslog_lev[logContext->_level],"-> %s %d\r\n",logContext->_file,logContext->_line);
    syslog(s_syslog_lev[logContext->_level], "## %s %s | %s %s\r\n", printTime(logContext->_tv).data(),
           LOG_CONST_TABLE[logContext->_level][2], logContext->_function, logContext->str().c_str());
}

#endif//#if defined(__MACH__) || ((defined(__linux) || defined(__linux__)) &&  !defined(ANDROID))

///////////////////LogChannel///////////////////
LogChannel::LogChannel(const string &name, LogLevel level) : _name(name), _level(level) {}
LogChannel::~LogChannel(){}
const string &LogChannel::name() const { return _name; }
void LogChannel::setLevel(LogLevel level) { _level = level; }

std::string LogChannel::printTime(const timeval &tv) {
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

void LogChannel::format(const Logger &logger,ostream &ost,const LogContextPtr & logContext, bool enableColor, bool enableDetail) {
    if (!enableDetail && logContext->str().empty()) {
        //没有任何信息打印
        return;
    }

    if (enableDetail) {
#if defined(_WIN32)
        ost << logger.getName() <<"(" << GetCurrentProcessId() << ") " << logContext->_file << " " << logContext->_line << endl;
#else
        ost << logger.getName() << "(" << getpid() << ") " << logContext->_file << " " << logContext->_line << endl;
#endif
    }

    if (enableColor) {
        ost << LOG_CONST_TABLE[logContext->_level][1];
    }

    ost << printTime(logContext->_tv) << " " << LOG_CONST_TABLE[logContext->_level][2] << " | ";

    if (enableDetail) {
        ost << logContext->_function << " ";
    }

    ost << logContext->str();

    if (enableColor) {
        ost << CLEAR_COLOR;
    }

    ost << endl;
}

///////////////////FileChannel///////////////////
FileChannel::FileChannel(const string &name, const string &path, LogLevel level) :
        LogChannel(name, level), _path(path) {}

FileChannel::~FileChannel() {
    close();
}

void FileChannel::write(const Logger &logger,const std::shared_ptr<LogContext> &logContext) {
    if (_level > logContext->_level) {
        return;
    }
    if (!_fstream.is_open()) {
        open();
    }
    format(logger, _fstream, logContext ,false);
}

void FileChannel::setPath(const string &path) {
    _path = path;
    open();
}

const string &FileChannel::path() const {
    return _path;
}

void FileChannel::open() {
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

void FileChannel::close() {
    _fstream.close();
}



} /* namespace toolkit */

