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
INSTANCE_IMP(Logger);

Logger::Logger() {}
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

void Logger::write(const LogContextPtr &stream) {
    if (_writer) {
        _writer->write(stream);
    }else{
        writeChannels(stream);
    }
}

void Logger::setLevel(LogLevel level) {
    for (auto &chn : _channels) {
        chn.second->setLevel(level);
    }
}

void Logger::writeChannels(const LogContextPtr &stream){
    for (auto &chn : _channels) {
        chn.second->write(stream);
    }
}

///////////////////LogContext///////////////////
void LogContext::format(ostream &ost, bool enableColor, bool enableDetail) {
    if (!enableDetail && str().empty()) {
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
        ost << LOG_CONST_TABLE[_level][1];
    }

    ost << printTime(_tv) << " " << LOG_CONST_TABLE[_level][2] << " | ";

    if (enableDetail) {
        ost << _function << " ";
    }

    ost << str();

    if (enableColor) {
        ost << CLEAR_COLOR;
    }

    ost << endl;
}

std::string LogContext::printTime(const timeval &tv) {
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

void AsyncLogWriter::write(const LogContextPtr &stream) {
    {
        lock_guard<mutex> lock(_mutex);
        _pending.push_back(stream);
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
    lock_guard<mutex> lock(_mutex);
    while (_pending.size()) {
        _logger.writeChannels(_pending.front());
        _pending.pop_front();
    }
}


///////////////////ConsoleChannel///////////////////

#ifdef ANDROID
#include <android/log.h>
#endif //ANDROID

ConsoleChannel::ConsoleChannel(const string &name, LogLevel level) : LogChannel(name, level) {}
ConsoleChannel:: ~ConsoleChannel() {}
void ConsoleChannel::write(const LogContextPtr &logContext)  {
    if (_level > logContext->_level) {
        return;
    }

#if defined(_WIN32) || defined(OS_IPHONE)
    logContext->format(std::cout, false);
#elif defined(ANDROID)
    static android_LogPriority LogPriorityArr[10];
    static onceToken s_token([](){
        LogPriorityArr[LTrace] = ANDROID_LOG_VERBOSE;
        LogPriorityArr[LDebug] = ANDROID_LOG_DEBUG;
        LogPriorityArr[LInfo] = ANDROID_LOG_INFO;
        LogPriorityArr[LWarn] = ANDROID_LOG_WARN;
        LogPriorityArr[LError] = ANDROID_LOG_ERROR;
    }, nullptr);
    __android_log_print(LogPriorityArr[logContext->_level],"JNI","%s %s",logContext->_function.c_str(),logContext->_message.str().c_str());
#else
    logContext->format(std::cout, true);
#endif

}

///////////////////LogChannel///////////////////
LogChannel::LogChannel(const string &name, LogLevel level) : _name(name), _level(level) {}
LogChannel::~LogChannel(){}
const string &LogChannel::name() const { return _name; }
void LogChannel::setLevel(LogLevel level) { _level = level; }

///////////////////FileChannel///////////////////
FileChannel::FileChannel(const string &name, const string &path, LogLevel level) :
        LogChannel(name, level), _path(path) {}

FileChannel::~FileChannel() {
    close();
}

void FileChannel::write(const std::shared_ptr<LogContext> &stream) {
    if (_level > stream->_level) {
        return;
    }
    if (!_fstream.is_open()) {
        open();
    }
    stream->format(_fstream, false);
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

