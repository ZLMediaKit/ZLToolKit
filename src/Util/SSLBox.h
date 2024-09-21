/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
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
#include "util.h"
#include "Network/Buffer.h"
#include "ResourcePool.h"

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
     * @param server_mode 是否为服务器模式
     * @param password 私钥加密密码
     * @param is_file 参数pem_or_p12是否为文件路径
     * @param is_default 是否为默认证书
     * Load public and private keys from a file or string
     * The certificate file must contain both public and private keys (cer format certificates only include public keys, use the following method to load)
     * The client can load the certificate by default (unless the server requires the client to provide a certificate)
     * @param pem_or_p12 pem or p12 file path or file content string
     * @param server_mode Whether it is in server mode
     * @param password Private key encryption password
     * @param is_file Whether the parameter pem_or_p12 is a file path
     * @param is_default Whether it is the default certificate
     
     * [AUTO-TRANSLATED:18cec755]
     */
    bool loadCertificate(const std::string &pem_or_p12, bool server_mode = true, const std::string &password = "",
                         bool is_file = true, bool is_default = true);

    /**
     * 是否忽略无效的证书
     * 默认忽略，强烈建议不要忽略！
     * @param ignore 标记
     * Whether to ignore invalid certificates
     * Ignore by default, strongly not recommended!
     * @param ignore Flag
     
     * [AUTO-TRANSLATED:fd45125a]
     */
    void ignoreInvalidCertificate(bool ignore = true);

    /**
     * 信任某证书,一般用于客户端信任自签名的证书或自签名CA签署的证书使用
     * 比如说我的客户端要信任我自己签发的证书，那么我们可以只信任这个证书
     * @param pem_p12_cer pem文件或p12文件或cer文件路径或内容
     * @param server_mode 是否为服务器模式
     * @param password pem或p12证书的密码
     * @param is_file 是否为文件路径
     * @return 是否加载成功
     * Trust a certain certificate, generally used for clients to trust self-signed certificates or certificates signed by self-signed CAs
     * For example, if my client wants to trust a certificate I issued myself, we can only trust this certificate
     * @param pem_p12_cer pem file or p12 file or cer file path or content
     * @param server_mode Whether it is in server mode
     * @param password pem or p12 certificate password
     * @param is_file Whether it is a file path
     * @return Whether the loading is successful
     
     * [AUTO-TRANSLATED:9ace5400]
     */
    bool trustCertificate(const std::string &pem_p12_cer, bool server_mode = false, const std::string &password = "",
                          bool is_file = true);

    /**
     * 信任某证书
     * @param cer 证书公钥
     * @param server_mode 是否为服务模式
     * @return 是否加载成功
     * Trust a certain certificate
     * @param cer Certificate public key
     * @param server_mode Whether it is in server mode
     * @return Whether the loading is successful
     
     * [AUTO-TRANSLATED:557120dd]
     */
    bool trustCertificate(X509 *cer, bool server_mode = false);

    /**
     * 根据虚拟主机获取SSL_CTX对象
     * @param vhost 虚拟主机名
     * @param server_mode 是否为服务器模式
     * @return SSL_CTX对象
     * Get the SSL_CTX object based on the virtual host
     * @param vhost Virtual host name
     * @param server_mode Whether it is in server mode
     * @return SSL_CTX object
     
     * [AUTO-TRANSLATED:4d771109]
     */
    std::shared_ptr<SSL_CTX> getSSLCtx(const std::string &vhost, bool server_mode);

private:
    SSL_Initor();
    ~SSL_Initor();

    /**
     * 创建SSL对象
     * Create an SSL object
     
     * [AUTO-TRANSLATED:047a0b4c]
     */
    std::shared_ptr<SSL> makeSSL(bool server_mode);

    /**
     * 设置ssl context
     * @param vhost 虚拟主机名
     * @param ctx ssl context
     * @param server_mode ssl context
     * @param is_default 是否为默认证书
     * Set the ssl context
     * @param vhost Virtual host name
     * @param ctx ssl context
     * @param server_mode ssl context
     * @param is_default Whether it is the default certificate
     
     * [AUTO-TRANSLATED:265f3049]
     */
    bool setContext(const std::string &vhost, const std::shared_ptr<SSL_CTX> &ctx, bool server_mode, bool is_default = true);

    /**
     * 设置SSL_CTX的默认配置
     * @param ctx 对象指针
     * Set the default configuration for SSL_CTX
     * @param ctx Object pointer
     
     * [AUTO-TRANSLATED:1b3438d0]
     */
    static void setupCtx(SSL_CTX *ctx);

    std::shared_ptr<SSL_CTX> getSSLCtx_l(const std::string &vhost, bool server_mode);

    std::shared_ptr<SSL_CTX> getSSLCtxWildcards(const std::string &vhost, bool server_mode);

    /**
     * 获取默认的虚拟主机
     * Get the default virtual host
     
     * [AUTO-TRANSLATED:e2430399]
     */
    std::string defaultVhost(bool server_mode);

    /**
     * 完成vhost name 匹配的回调函数
     * Callback function for completing vhost name matching
     
     * [AUTO-TRANSLATED:f9973cfa]
     */
    static int findCertificate(SSL *ssl, int *ad, void *arg);

private:
    struct less_nocase {
        bool operator()(const std::string &x, const std::string &y) const {
            return strcasecmp(x.data(), y.data()) < 0;
        }
    };

private:
    std::recursive_mutex _mtx;
    std::string _default_vhost[2];
    std::shared_ptr<SSL_CTX> _ctx_empty[2];
    std::map<std::string, std::shared_ptr<SSL_CTX>, less_nocase> _ctxs[2];
    std::map<std::string, std::shared_ptr<SSL_CTX>, less_nocase> _ctxs_wildcards[2];
};

////////////////////////////////////////////////////////////////////////////////////

class SSL_Box {
public:
    SSL_Box(bool server_mode = true, bool enable = true, int buff_size = 32 * 1024);

    ~SSL_Box();

    /**
     * 收到密文后，调用此函数解密
     * @param buffer 收到的密文数据
     * Decrypts the received ciphertext after calling this function
     * @param buffer Received ciphertext data
     
     * [AUTO-TRANSLATED:7e8b1fc6]
     */
    void onRecv(const Buffer::Ptr &buffer);

    /**
     * 需要加密明文调用此函数
     * @param buffer 需要加密的明文数据
     * Calls this function to encrypt the plaintext that needs to be encrypted
     * @param buffer Plaintext data that needs to be encrypted
     
     * [AUTO-TRANSLATED:9d386695]
     */
    void onSend(Buffer::Ptr buffer);

    /**
     * 设置解密后获取明文的回调
     * @param cb 回调对象
     * Sets the callback to get the plaintext after decryption
     * @param cb Callback object
     
     * [AUTO-TRANSLATED:897359bc]
     */
    void setOnDecData(const std::function<void(const Buffer::Ptr &)> &cb);

    /**
     * 设置加密后获取密文的回调
     * @param cb 回调对象
     * Sets the callback to get the ciphertext after encryption
     * @param cb Callback object
     
     * [AUTO-TRANSLATED:bb31b34b]
     */
    void setOnEncData(const std::function<void(const Buffer::Ptr &)> &cb);

    /**
     * 终结ssl
     * Terminates SSL
     
     * [AUTO-TRANSLATED:2ab06469]
     */
    void shutdown();

    /**
     * 清空数据
     * Clears data
     
     * [AUTO-TRANSLATED:62d4f400]
     */
    void flush();

    /**
     * 设置虚拟主机名
     * @param host 虚拟主机名
     * @return 是否成功
     * Sets the virtual host name
     * @param host Virtual host name
     * @return Whether the operation was successful
     
     * [AUTO-TRANSLATED:eebc1e2f]
     */
    bool setHost(const char *host);

private:
    void flushWriteBio();

    void flushReadBio();

private:
    bool _server_mode;
    bool _send_handshake;
    bool _is_flush = false;
    int _buff_size;
    BIO *_read_bio;
    BIO *_write_bio;
    std::shared_ptr<SSL> _ssl;
    List <Buffer::Ptr> _buffer_send;
    ResourcePool <BufferRaw> _buffer_pool;
    std::function<void(const Buffer::Ptr &)> _on_dec;
    std::function<void(const Buffer::Ptr &)> _on_enc;
};

} /* namespace toolkit */
#endif /* CRYPTO_SSLBOX_H_ */
