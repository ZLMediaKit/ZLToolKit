/*
 * @file_name: http_session.hpp
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
 * SOFTWARE.
 */
#ifndef HTTP_SESSION_HPP
#define HTTP_SESSION_HPP
#include "Http/Body/AnyBody.hpp"
#include "HttpParser.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "HttpSplitter.hpp"
#include <Network/Buffer.h>
#include <Network/TcpSession.h>
#include <functional>
#include <map>
namespace http {

class HttpSession;
class HttpRequestHandler{
public:
    using http_callback = std::function<void(std::shared_ptr<HttpSession>)>;
public:
    //注册http回调
    static void ApiRegister(const char* path, http_callback&&);
    static const http_callback* GetInvokes(const char* path);
private:
    static std::map<std::string, http_callback> _invokes;
};

class HttpSession : public toolkit::TcpSession, public HttpParser {
public:
  using Buffer = toolkit::Buffer;
  using BufferLikeString = toolkit::BufferLikeString;
  using SockException = toolkit::SockException;
  using Ptr = std::shared_ptr<HttpSession>;
  using Socket = typename toolkit::Socket;

public:
  HttpSession(const Socket::Ptr &sock);

  const http::http_request<http::any_body>& GetRequest() const {
      return *http_request;
  }

  template<typename body_type>
  void sendResponse(http::http_response<body_type>& response) {
      std::string resp = std::move(response.to_string());
      auto buffer_ptr = std::make_shared<BufferLikeString>(std::move(resp));
      send(std::move(buffer_ptr));
      if(!http_request->keep_alive()){
          this->shutdown();
      }
  }

public:
  virtual void onRecv(const Buffer::Ptr &buf) override;
  virtual void onError(const SockException &err) override;
  virtual void onManager() override {}
private:
  //virtual void OnRecvHeaderBody(std::string& header, std::string& body) override;
private:
  std::shared_ptr<http::http_request<http::any_body>> http_request;
  BufferLikeString input_buffer;
};
}; // namespace http

#endif