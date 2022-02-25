/*
 * @file_name: http_session.cpp
 * @date: 2021/12/06
 * @author: oaho
 * Copyright @ hz oaho, All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.x
 */
#include <Util/logger.h>
#include "HttpResponse.hpp"
#include "HttpSession.hpp"
#include "HttpServer.hpp"
#include <map>
#include <string.h>
#include <utility>
#include <vector>
namespace http {
    using namespace toolkit;

    using HttpCallBack = typename HttpRequestHandler::http_callback;
    std::map<std::string, HttpCallBack> HttpRequestHandler::_invokes;
    void HttpRequestHandler::ApiRegister(const char* path, HttpCallBack&& callback){
        _invokes[path] = std::move(callback);
    }

    const HttpCallBack* HttpRequestHandler::GetInvokes(const char* path){
        auto it = _invokes.find(path);
        if( it != _invokes.end()){
            return &(it->second);
        }
        return nullptr;
    }


    HttpSession::HttpSession(const Socket::Ptr &sock)
        : toolkit::TcpSession(sock) {

    }

    void HttpSession::onRecv(const Buffer::Ptr &buf) {
        try {
            //输入分包器
            HttpSplitter::Input(buf->data(), buf->size());
            //auto* invoke_func = HttpRequestHandler::GetInvokes(http_request->uri().path().c_str());
            //if(!invoke_func){
                //throw std::logic_error("忽略的http请求");
            //}
            //auto self = std::static_pointer_cast<HttpSession>(toolkit::TcpSession::shared_from_this());
            //(*invoke_func)(self);
        } catch (const std::exception &e) {
            InfoL << e.what();
            this->shutdown();
        }
    }
    void HttpSession::onError(const SockException &err) { ErrorL << err.what(); }
    void HttpSession::OnRecvHeaderBody(std::string& header, std::string& body){
        static std::set<std::string> support_method{
            "GET", "POST"
        };
        http_request.reset(new http::http_request<http::any_body>);
        std::string method;
        std::string version;
        std::string path;
        CreateHeader(header, method, version, path, http_request->headers());
        auto it = support_method.find(method);
        if( it == support_method.end())throw std::logic_error("不支持的http请求方法");
        http_request->_method = std::move(method);
        if( version != "HTTP/1.1")throw std::logic_error("不支持的HTTP协议版本");
        //http_request->uri() = std::move(path);
        http_request->body() = std::move(body);
        const auto& keep = http_request->get_header<http::http_request_header::connection>();
        if(!keep.compare("keep-alive")){
            http_request->keep_alive(true);
        }
    }
}