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
#include "File.h"
#include <sys/stat.h>

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
	static const char* LOG_CONST_TABLE[][3] = {
			{"\033[44;37m", "\033[34m" , "T"},
			{"\033[42;37m", "\033[32m" , "D"},
			{"\033[46;37m", "\033[36m" , "I"},
			{"\033[43;37m", "\033[33m" , "W"},
			{"\033[41;37m", "\033[31m" , "E"} };
#endif

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

#if defined(_WIN32)
    format(logger,std::cout, logContext , true, true);
#elif defined(OS_IPHONE)
	format(logger, std::cout, logContext, false);
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
#ifdef _WIN32
        SetConsoleColor(LOG_CONST_TABLE[logContext->_level][1]);
#else
        ost << LOG_CONST_TABLE[logContext->_level][1];
#endif
    }

#ifdef _WIN32
    ost << printTime(logContext->_tv) << " " << (char)LOG_CONST_TABLE[logContext->_level][2] << " | ";
#else
    ost << printTime(logContext->_tv) << " " << LOG_CONST_TABLE[logContext->_level][2] << " | ";
#endif

    if (enableDetail) {
        ost << logContext->_function << " ";
    }

    ost << logContext->str();

    if (enableColor) {
#ifdef _WIN32
		SetConsoleColor(CLEAR_COLOR);
#else
		ost << CLEAR_COLOR;
#endif
    }

    ost << endl;
}

///////////////////FileChannelBase///////////////////
FileChannelBase::FileChannelBase(const string &name, const string &path, LogLevel level) :
        LogChannel(name, level), _path(path) {}

FileChannelBase::~FileChannelBase() {
    close();
}

void FileChannelBase::write(const Logger &logger,const std::shared_ptr<LogContext> &logContext) {
    if (_level > logContext->_level) {
        return;
    }
    if (!_fstream.is_open()) {
        open();
    }
    format(logger, _fstream, logContext ,false);
}

void FileChannelBase::setPath(const string &path) {
    _path = path;
    open();
}

const string &FileChannelBase::path() const {
    return _path;
}

void FileChannelBase::open() {
    // Ensure a path was set
    if (_path.empty()) {
        throw runtime_error("Log file path must be set.");
    }
    // Open the file stream
    _fstream.close();
#if !defined(_WIN32)
    //创建文件夹
    File::createfile_path(_path.c_str(),S_IRWXO | S_IRWXG | S_IRWXU);
#else
    File::createfile_path(_path.c_str(),0);
#endif
    _fstream.open(_path.c_str(), ios::out | ios::app);
    // Throw on failure
    if (!_fstream.is_open()) {
        throw runtime_error("Failed to open log file: " + _path);
    }
}

void FileChannelBase::close() {
    _fstream.close();
}

///////////////////FileChannel///////////////////
FileChannel::FileChannel(const string &name, const string &dir, LogLevel level) : FileChannelBase(name, "", level) {
    _dir = dir;
    if(_dir.back() != '/'){
        _dir.append("/");
    }
}
FileChannel::~FileChannel() {}

static const auto s_second_per_day = 24 * 60 * 60;
uint64_t FileChannel::getDay(time_t second) {
    return second / s_second_per_day;
}

static string getLogFilePath(const string &dir,uint64_t day){
    time_t second = s_second_per_day * day;
    struct tm *tm = localtime(&second);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d-%02d-%02d.log", 1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday);
    return dir + buf;
}

void FileChannel::write(const Logger &logger, const shared_ptr<LogContext> &stream) {
    //这条日志所在第几天
    auto day = getDay(stream->_tv.tv_sec);
    if(day != _last_day){
        //这条日志是新的一天，记录这一天
        _last_day = day;
        //获取日志当天对应的文件，每天只有一个文件
        auto log_file = getLogFilePath(_dir,day);
        //记录所有的日志文件，以便后续删除老的日志
        _log_file_map.emplace(day,log_file);
        //打开新的日志文件
        setPath(log_file);
        clean();
    }
    //写日志
    FileChannelBase::write(logger,stream);
}

void FileChannel::clean(){
    //获取今天是第几天
    auto today = getDay(time(NULL));
    //遍历所有日志文件，删除老日志
    for(auto it = _log_file_map.begin(); it != _log_file_map.end() ; ){
        if(today < it->first + _log_max_day){
            //这个日志文件距今不超过_log_max_day天
            break;
        }
        //这个文件距今超过了_log_max_day天，则删除文件
        File::delete_file(it->second.data());
        //删除这条记录
        it = _log_file_map.erase(it);
    }
}

void FileChannel::setMaxDay(int max_day){
    _log_max_day = max_day > 1 ? max_day : 1;
}


} /* namespace toolkit */

