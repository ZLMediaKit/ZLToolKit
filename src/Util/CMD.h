/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xia-chu/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_UTIL_CMD_H_
#define SRC_UTIL_CMD_H_

#include <map>
#include <mutex>
#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include <iostream>
#include <functional>
#include "Util/mini.h"
#include "Util/onceToken.h"

namespace toolkit{

class Option {
public:
    typedef std::function<bool(const std::shared_ptr<std::ostream> &stream, const std::string &arg)> OptionHandler;
    enum ArgType {
        ArgNone = 0,//no_argument,
        ArgRequired = 1,//required_argument,
        ArgOptional = 2,//optional_argument
    };
    Option(){}
    Option(char shortOpt,
           const char *longOpt,
           enum ArgType argType,
           const char *defaultValue,
           bool mustExist,//该参数是否必须存在
           const char *des,
           const OptionHandler &cb) {
        _shortOpt = shortOpt;
        _longOpt = longOpt;
        _argType = argType;
        if(argType != ArgNone){
            if(defaultValue){
                _defaultValue = std::make_shared<std::string>(defaultValue);
            }
            if(!_defaultValue && mustExist){
                _mustExist = true;
            }
        }
        _des = des;
        _cb = cb;
    }
    ~Option() {}
    bool operator()(const std::shared_ptr<std::ostream> &stream, const std::string &arg){
        return _cb ? _cb(stream,arg): true;
    }
private:
    friend class OptionParser;
    char _shortOpt;
    std::string _longOpt;
    std::shared_ptr<std::string> _defaultValue;
    enum ArgType _argType;
    std::string _des;
    OptionHandler _cb;
    bool _mustExist = false;
};

class OptionParser {
public:
    typedef std::function< void(const std::shared_ptr<std::ostream> &,mINI &)> OptionCompleted;
    OptionParser(const OptionCompleted &cb = nullptr,bool enableEmptyArgs = true) {
        _onCompleted = cb;
        _enableEmptyArgs = enableEmptyArgs;
        _helper = Option('h', "help", Option::ArgNone, nullptr, false, "打印此信息",
                         [this](const std::shared_ptr<std::ostream> &stream,const std::string &arg)->bool {
            static const char *argsType[] = {"无参","有参","选参"};
            static const char *mustExist[] = {"选填","必填"};
            static std::string defaultPrefix = "默认:";
            static std::string defaultNull = "null";

            std::stringstream printer;
            size_t maxLen_longOpt = 0;
            auto maxLen_default = defaultNull.size();

            for (auto &pr : _map_options) {
                auto &opt = pr.second;
                if(opt._longOpt.size() > maxLen_longOpt){
                    maxLen_longOpt = opt._longOpt.size();
                }
                if(opt._defaultValue){
                    if(opt._defaultValue->size() > maxLen_default){
                        maxLen_default = opt._defaultValue->size();
                    }
                }
            }
            for (auto &pr : _map_options) {
                auto &opt = pr.second;
                //打印短参和长参名
                if(opt._shortOpt){
                    printer <<"  -" << opt._shortOpt <<"  --" << opt._longOpt;
                }else{
                    printer <<"   " << " " <<"  --" << opt._longOpt;
                }
                for (size_t i = 0; i < maxLen_longOpt - opt._longOpt.size(); ++i) {
                    printer << " ";
                }
                //打印是否有参
                printer << "  " << argsType[opt._argType];
                //打印默认参数
                std::string defaultValue = defaultNull;
                if(opt._defaultValue){
                    defaultValue = *opt._defaultValue;
                }
                printer << "  " << defaultPrefix << defaultValue;
                for (size_t i = 0; i < maxLen_default - defaultValue.size(); ++i) {
                    printer << " ";
                }
                //打印是否必填参数
                printer << "  " << mustExist[opt._mustExist];
                //打印描述
                printer << "  " << opt._des << std::endl;
            }
            throw std::invalid_argument(printer.str());
        });
        (*this) << _helper;
    }
    ~OptionParser() {
    }

    OptionParser &operator <<(Option &&option) {
        int index = 0xFF + (int)_map_options.size();
        if(option._shortOpt){
            _map_charIndex.emplace(option._shortOpt,index);
        }
        _map_options.emplace(index, std::forward<Option>(option));
        return *this;
    }
    OptionParser &operator <<(const Option &option) {
        int index = 0xFF + (int)_map_options.size();
        if(option._shortOpt){
            _map_charIndex.emplace(option._shortOpt,index);
        }
        _map_options.emplace(index, option);
        return *this;
    }
    void delOption(const char *key){
        for (auto &pr : _map_options) {
            if(pr.second._longOpt == key){
                if(pr.second._shortOpt){
                    _map_charIndex.erase(pr.second._shortOpt);
                }
                _map_options.erase(pr.first);
                break;
            }
        }
    }
    void operator ()(mINI &allArg, int argc, char *argv[],const std::shared_ptr<std::ostream> &stream);
private:
    std::map<int,Option> _map_options;
    std::map<char,int> _map_charIndex;
    OptionCompleted _onCompleted;
    Option _helper;
    static std::mutex s_mtx_opt;
    bool _enableEmptyArgs;
};

class CMD :public mINI{
public:
    CMD(){};
    virtual ~CMD(){};
    virtual const char *description() const {
        return "description";
    }
    void operator ()(int argc, char *argv[],const std::shared_ptr<std::ostream> &stream = nullptr) {
        this->clear();
        std::shared_ptr<std::ostream> coutPtr(&std::cout,[](std::ostream *){});
        (*_parser)(*this,argc, argv,stream? stream : coutPtr );
    }

    bool hasKey(const char *key){
        return this->find(key) != this->end();
    }

    std::vector<variant> splitedVal(const char *key,const char *delim= ":"){
        std::vector<variant> ret;
        auto &val = (*this)[key];
        split(val,delim,ret);
        return ret;
    }
    void delOption(const char *key){
        if(_parser){
            _parser->delOption(key);
        }
    }
protected:
    std::shared_ptr<OptionParser> _parser;
private:
    void split(const std::string& s, const char *delim, std::vector<variant> &ret){
        size_t last = 0;
        auto index = s.find(delim, last);
        while (index != std::string::npos) {
            if(index - last > 0){
                ret.push_back(s.substr(last, index - last));
            }
            last = index + strlen(delim);
            index = s.find(delim, last);
        }
        if (s.size() - last > 0) {
            ret.push_back(s.substr(last));
        }
    }
};


class CMDRegister
{
public:
    CMDRegister() {};
    ~CMDRegister(){};
    static CMDRegister &Instance();
    void clear(){
        std::lock_guard<std::recursive_mutex> lck(_mtxCMD);
        _mapCMD.clear();
    }
    void registCMD(const char *name,const std::shared_ptr<CMD> &cmd){
        std::lock_guard<std::recursive_mutex> lck(_mtxCMD);
        _mapCMD.emplace(name,cmd);
    }
    void unregistCMD(const char *name){
        std::lock_guard<std::recursive_mutex> lck(_mtxCMD);
        _mapCMD.erase(name);
    }
    std::shared_ptr<CMD> operator[](const char *name){
        std::lock_guard<std::recursive_mutex> lck(_mtxCMD);
        auto it = _mapCMD.find(name);
        if(it == _mapCMD.end()){
            throw std::invalid_argument(std::string("命令不存在:") + name);
        }
        return it->second;
    }

    void operator()(const char *name,int argc,char *argv[],const std::shared_ptr<std::ostream> &stream = nullptr){
        auto cmd = (*this)[name];
        if(!cmd){
            throw std::invalid_argument(std::string("命令不存在:") + name);
        }
        (*cmd)(argc,argv,stream);
    };
    void printHelp(const std::shared_ptr<std::ostream> &streamTmp = nullptr){
        auto stream = streamTmp;
        if(!stream){
            stream.reset(&std::cout,[](std::ostream *){});
        }

        std::lock_guard<std::recursive_mutex> lck(_mtxCMD);
        size_t maxLen = 0;
        for (auto &pr : _mapCMD) {
            if(pr.first.size() > maxLen){
                maxLen = pr.first.size();
            }
        }
        for (auto &pr : _mapCMD) {
            (*stream) << "  " << pr.first;
            for (size_t i = 0; i < maxLen - pr.first.size(); ++i) {
                (*stream) << " ";
            }
            (*stream) << "  " << pr.second->description() << std::endl;
        }
    };
    void operator()(const std::string &line,const std::shared_ptr<std::ostream> &stream = nullptr){
        if(line.empty()){
            return;
        }
        std::vector<char *> argv;
        size_t argc = getArgs((char *)line.data(), argv);
        if (argc == 0) {
            return;
        }
        std::string cmd = argv[0];
        std::lock_guard<std::recursive_mutex> lck(_mtxCMD);
        auto it = _mapCMD.find(cmd);
        if (it == _mapCMD.end()) {
            std::stringstream ss;
            ss << "  未识别的命令\"" << cmd << "\",输入 \"help\" 获取帮助.";
            throw std::invalid_argument(ss.str());
        }
        (*it->second)((int)argc,&argv[0],stream);
    };

private:
    size_t getArgs(char *buf, std::vector<char *> &argv) {
        size_t argc = 0;
        bool start = false;
        auto len = strlen(buf);
        for (size_t i = 0; i < len; ++i) {
            if (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\r' && buf[i] != '\n') {
                if (!start) {
                    start = true;
                    if(argv.size() < argc + 1){
                        argv.resize(argc + 1);
                    }
                    argv[argc++] = buf + i;
                }
            } else {
                buf[i] = '\0';
                start = false;
            }
        }
        return argc;
    }
private:
    std::map<std::string,std::shared_ptr<CMD> > _mapCMD;
    std::recursive_mutex _mtxCMD;

};

//帮助命令(help)，该命令默认已注册
class CMD_help: public CMD {
public:
    CMD_help(){
		_parser = std::make_shared<OptionParser>([](const std::shared_ptr<std::ostream> &stream,mINI &){
			CMDRegister::Instance().printHelp(stream);
        });
    }
    ~CMD_help() {}

    const char *description() const override {
        return "打印帮助信息";
    }
};

class ExitException : public std::exception
{
public:
    ExitException(){}
    ~ExitException(){}

};

//退出程序命令(exit)，该命令默认已注册
class CMD_exit: public CMD {
public:
    CMD_exit(){
        _parser = std::make_shared<OptionParser>([](const std::shared_ptr<std::ostream> &,mINI &){
            throw ExitException();
        });
    }
    ~CMD_exit() {}

    const char *description() const override {
        return "退出shell";
    }
};

//退出程序命令(quit),该命令默认已注册
#define CMD_quit CMD_exit

//清空屏幕信息命令(clear)，该命令默认已注册
class CMD_clear : public CMD
{
public:
    CMD_clear(){
        _parser = std::make_shared<OptionParser>([this](const std::shared_ptr<std::ostream> &stream,mINI &args){
            clear(stream);
        });
    }
    ~CMD_clear(){}
    const char *description() const {
        return "清空屏幕输出";
    }
private:
    void clear(const std::shared_ptr<std::ostream> &stream){
        (*stream) << "\x1b[2J\x1b[H";
        stream->flush();
    }
};

#define GET_CMD(name) (*(CMDRegister::Instance()[name]))
#define CMD_DO(name,...) (*(CMDRegister::Instance()[name]))(__VA_ARGS__)
#define REGIST_CMD(name) CMDRegister::Instance().registCMD(#name,std::make_shared<CMD_##name>());

}//namespace toolkit
#endif /* SRC_UTIL_CMD_H_ */
