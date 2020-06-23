/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CRYPTO_SSLBOX_H_
#define CRYPTO_SSLBOX_H_

#include <mutex>
#include <string>
#include <functional>
#include "logger.h"
#include "List.h"
#include "Network/Buffer.h"
#include "Util/ResourcePool.h"
using namespace std;

typedef struct x509_st X509;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;
typedef struct bio_st BIO;

namespace toolkit {

class SSL_Initor {
public:
    friend class SSL_Box;
    static SSL_Initor &Instance();

    /**
     * 从文件或字符串中加载公钥和私钥
     * 该证书文件必须同时包含公钥和私钥(cer格式的证书只包括公钥，请使用后面的方法加载)
     * 客户端默认可以不加载证书(除非服务器要求客户端提供证书)
     * @param pem_or_p12 pem或p12文件路径或者文件内容字符串
     * @param serverMode 是否为服务器模式
     * @param passwd 私钥加密密码
     * @param isFile 参数pem_or_p12是否为文件路径
     * @param isDefault 是否为默认证书
     */
    bool loadCertificate(const string &pem_or_p12, bool serverMode = true, const string &passwd = "", bool isFile = true, bool isDefault = true);

    /**
     * 是否忽略无效的证书
     * 默认忽略，强烈建议不要忽略！
     * @param ignore 标记
     */
    void ignoreInvalidCertificate(bool ignore = true);

    /**
     * 信任某证书,一般用于客户端信任自签名的证书或自签名CA签署的证书使用
     * 比如说我的客户端要信任我自己签发的证书，那么我们可以只信任这个证书
     * @param pem_p12_cer pem文件或p12文件或cer文件路径或内容
     * @param serverMode 是否为服务器模式
     * @param passwd pem或p12证书的密码
     * @param isFile 是否为文件路径
     * @return 是否加载成功
     */
    bool trustCertificate(const string &pem_p12_cer, bool serverMode = false, const string &passwd = "", bool isFile = true);

    /**
     * 信任某证书
     * @param cer 证书公钥
     * @param serverMode 是否为服务模式
     * @return 是否加载成功
     */
    bool trustCertificate(X509 *cer,bool serverMode = false);

private:
    SSL_Initor();
    ~SSL_Initor();

    /**
     * 创建SSL对象
     * @param ctx
     * @return
     */
    shared_ptr<SSL> makeSSL(bool serverMode);

    /**
     * 设置ssl context
     * @param vhost 虚拟主机名
     * @param ctx ssl context
     * @param serverMode ssl context
     * @param isDefault 是否为默认证书
     */
    bool setContext(const string &vhost, const std::shared_ptr<SSL_CTX> &ctx, bool serverMode, bool isDefault = true);

    /**
     * 设置SSL_CTX的默认配置
     * @param ctx 对象指针
     */
    void setupCtx(SSL_CTX *ctx);

    /**
     * 根据虚拟主机获取SSL_CTX对象
     * @param vhost 虚拟主机名
     * @param serverMode 是否为服务器模式
     * @return SSL_CTX对象
     */
    std::shared_ptr<SSL_CTX> getSSLCtx(const string &vhost, bool serverMode);

    /**
     * 获取默认的虚拟主机
     * @param serverMode
     * @return
     */
    string defaultVhost(bool serverMode);

    /**
     * 完成vhost name 匹配的回调函数
     * @param ssl
     * @param ad
     * @param arg
     * @return
     */
    static int findCertificate(SSL *ssl, int *ad, void *arg);

private:
    std::shared_ptr<SSL_CTX> _ctx_empty[2];
    map<string,std::shared_ptr<SSL_CTX> > _ctxs[2];
    string _default_vhost[2];
};

////////////////////////////////////////////////////////////////////////////////////

class SSL_Box {
public:
    SSL_Box(bool serverMode = true, bool enable = true, int buffSize = 32 * 1024);
    ~SSL_Box();

    /**
     * 收到密文后，调用此函数解密
     * @param buffer 收到的密文数据
     */
    void onRecv(const Buffer::Ptr &buffer);

    /**
     * 需要加密明文调用此函数
     * @param buffer 需要加密的明文数据
     */
    void onSend(const Buffer::Ptr &buffer);

    /**
     * 设置解密后获取明文的回调
     * @param fun 回调对象
     */
    template<typename F>
    void setOnDecData(F &&fun) {
        _onDec = std::forward<F>(fun);
    }

    /**
     * 设置加密后获取密文的回调
     * @param fun 回调对象
     */
    template<typename F>
    void setOnEncData(F &&fun) {
        _onEnc = std::forward<F>(fun);
    }

    /**
     * 终结ssl
     */
    void shutdown();

    /**
     * 清空数据
     */
    void flush();

    /**
     * 设置虚拟主机名
     * @param host 虚拟主机名
     * @return 是否成功
     */
    bool setHost(const char *host);
private:
    void flushWriteBio();
    void flushReadBio();
private:
    bool _serverMode;
    bool _sendHandshake;
    shared_ptr<SSL> _ssl;
    BIO *_read_bio, *_write_bio;
    function<void(const Buffer::Ptr &)> _onDec;
    function<void(const Buffer::Ptr &)> _onEnc;
    List<Buffer::Ptr> _bufferOut;
    ResourcePool<BufferRaw> _bufferPool;
    int _buffSize;
};

} /* namespace toolkit */
#endif /* CRYPTO_SSLBOX_H_ */
