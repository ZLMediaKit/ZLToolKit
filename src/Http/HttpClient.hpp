/*
 * @file_name: http_client.hpp
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
#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP
#include <Network/TcpClient.h>
#include <functional>
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <memory>
namespace http{
  template<typename T>
  class http_client : public http_request<T>, public toolkit::TcpClient{
  public:
    using client_type = typename toolkit::TcpClient;
    using Ptr = typename client_type::Ptr;
    using body_type = T;
    using SockException = toolkit::SockException;
    using Buffer = toolkit::Buffer;
    using EventPoller = toolkit::EventPoller;
    template<typename body_type> using response_type = std::shared_ptr<http::http_response<body_type>>;
    template<typename body_type> using response_func = std::function<void(const SockException &ex, response_type<body_type>)>;

  public:
    http_client(const EventPoller::Ptr &poller = nullptr):client_type(poller){}

    template<typename _body_type>
    void get(const http::uri& _uri, response_func<_body_type>&& f){
      http_request<body_type>::uri(_uri);

    }

    template<typename body_type>
    void post(const http::uri& _uri, response_func<body_type>&& f){
      http_request<body_type>::uri(_uri);
    }

  private:

    virtual void onConnect(const SockException &ex) override {
       if(!ex.getErrCode()){}
    }

    virtual void onRecv(const Buffer::Ptr &buf) override {

    }

    virtual void onErr(const SockException &ex) override {

    }
  };
};

#endif // VIDEO_ON_DEMAND_HTTP_CLIENT_HPP
