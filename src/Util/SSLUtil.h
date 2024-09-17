/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLTOOLKIT_SSLUTIL_H
#define ZLTOOLKIT_SSLUTIL_H

#include <memory>
#include <string>
#include <vector>

typedef struct x509_st X509;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;
typedef struct bio_st BIO;

namespace toolkit {
/**
 * ssl证书后缀一般分为以下几种
 * pem:这个是base64的字符编码串，可能存在公钥、私钥或者两者都存在
 * cer:只且只能是公钥，可以与pem的私钥配合使用
 * p12:必须包括私钥和公钥
 * SSL certificate suffixes are generally divided into the following types
 * pem: This is a base64 character encoded string, which may contain a public key, private key, or both
 * cer: Only and must be a public key, can be used with pem private key
 * p12: Must include both private key and public key
 
 * [AUTO-TRANSLATED:1cae2cfa]
 */
class SSLUtil {
public:
    static std::string getLastError();

    /**
     * 加载公钥证书，支持pem,p12,cer后缀
     * 由于openssl加载p12证书时会校验公钥和私钥是否匹对，所以加载p12的公钥时可能需要传入证书密码
     * @param file_path_or_data 文件路径或文件内容
     * @param isFile 是否为文件
     * @return 公钥证书列表
     * Load public key certificate, support pem, p12, cer suffixes
     * When openssl loads p12 certificate, it will verify whether the public key and private key match,
     * so when loading p12 public key, you may need to pass in the certificate password
     * @param file_path_or_data File path or file content
     * @param isFile Whether it is a file
     * @return Public key certificate list
     
     * [AUTO-TRANSLATED:d9dbac61]
     */
    static std::vector<std::shared_ptr<X509> > loadPublicKey(const std::string &file_path_or_data, const std::string &passwd = "", bool isFile = true);

    /**
     * 加载私钥证书，支持pem,p12后缀
     * @param file_path_or_data 文件路径或文件内容
     * @param passwd 密码
     * @param isFile 是否为文件
     * @return 私钥证书
     * Load private key certificate, support pem, p12 suffixes
     * @param file_path_or_data File path or file content
     * @param passwd Password
     * @param isFile Whether it is a file
     * @return Private key certificate
     
     * [AUTO-TRANSLATED:73c495c8]
     */
    static std::shared_ptr<EVP_PKEY> loadPrivateKey(const std::string &file_path_or_data, const std::string &passwd = "", bool isFile = true);

    /**
     * 创建SSL_CTX对象
     * @param cer 公钥数组
     * @param key 私钥
     * @param serverMode 是否为服务器模式或客户端模式
     * @return SSL_CTX对象
     * Create SSL_CTX object
     * @param cer Public key array
     * @param key Private key
     * @param serverMode Whether it is server mode or client mode
     * @return SSL_CTX object
     
     * [AUTO-TRANSLATED:d0faa6a4]
     */
    static std::shared_ptr<SSL_CTX> makeSSLContext(const std::vector<std::shared_ptr<X509> > &cers, const std::shared_ptr<EVP_PKEY> &key, bool serverMode = true, bool checkKey = false);

    /**
     * 创建ssl对象
     * @param ctx SSL_CTX对象
     * Create ssl object
     * @param ctx SSL_CTX object
     
     * [AUTO-TRANSLATED:2e3eb193]
     */
    static std::shared_ptr<SSL> makeSSL(SSL_CTX *ctx);

    /**
     * specifies that the default locations from which CA certificates are loaded should be used.
     * There is one default directory and one default file.
     * The default CA certificates directory is called "certs" in the default OpenSSL directory.
     * Alternatively the SSL_CERT_DIR environment variable can be defined to override this location.
     * The default CA certificates file is called "cert.pem" in the default OpenSSL directory.
     *  Alternatively the SSL_CERT_FILE environment variable can be defined to override this location.
     * 信任/usr/local/ssl/certs/目录下的所有证书/usr/local/ssl/cert.pem的证书
     * 环境变量SSL_CERT_FILE将替换/usr/local/ssl/cert.pem的路径
     * specifies that the default locations from which CA certificates are loaded should be used.
     * There is one default directory and one default file.
     * The default CA certificates directory is called "certs" in the default OpenSSL directory.
     * Alternatively the SSL_CERT_DIR environment variable can be defined to override this location.
     * The default CA certificates file is called "cert.pem" in the default OpenSSL directory.
     *  Alternatively the SSL_CERT_FILE environment variable can be defined to override this location.
     * Trust all certificates in the /usr/local/ssl/certs/ directory and /usr/local/ssl/cert.pem
     * The environment variable SSL_CERT_FILE will replace the path of /usr/local/ssl/cert.pem
     
     * [AUTO-TRANSLATED:f13fc4c5]
     */
    static bool loadDefaultCAs(SSL_CTX *ctx);

    /**
     * 信任某公钥
     * Trust a public key
     
     * [AUTO-TRANSLATED:08987c7e]
     */
    static bool trustCertificate(SSL_CTX *ctx, X509 *cer);


    /**
     * 验证证书合法性
     * @param cer 待验证的证书
     * @param ... 信任的CA根证书，X509类型，以nullptr结尾
     * @return 是否合法
     * Verify the validity of the certificate
     * @param cer Certificate to be verified
     * @param ... Trusted CA root certificates, X509 type, ending with nullptr
     * @return Whether it is valid
     
     * [AUTO-TRANSLATED:1b026a8f]
     */
    static bool verifyX509(X509 *cer, ...);

    /**
     * 使用公钥加解密数据
     * @param cer 公钥，必须为ras的公钥
     * @param in_str 加密或解密的原始数据，实测加密最大支持245个字节，加密后数据长度固定为256个字节
     * @param enc_or_dec true:加密,false:解密
     * @return 加密或解密后的数据
     * Use public key to encrypt and decrypt data
     * @param cer Public key, must be ras public key
     * @param in_str Original data to be encrypted or decrypted, tested to support up to 245 bytes,
     *                encrypted data length is fixed at 256 bytes
     * @param enc_or_dec true: Encrypt, false: Decrypt
     * @return Encrypted or decrypted data
     
     * [AUTO-TRANSLATED:77bc2939]
     */
    static std::string cryptWithRsaPublicKey(X509 *cer, const std::string &in_str, bool enc_or_dec);

    /**
     * 使用私钥加解密数据
     * @param private_key 私钥，必须为ras的私钥
     * @param in_str 加密或解密的原始数据，实测加密最大支持245个字节，加密后数据长度固定为256个字节
     * @param enc_or_dec true:加密,false:解密
     * @return 加密或解密后的数据
     * Use private key to encrypt and decrypt data
     * @param private_key Private key, must be ras private key
     * @param in_str Original data to be encrypted or decrypted, tested to support up to 245 bytes,
     *                encrypted data length is fixed at 256 bytes
     * @param enc_or_dec true: Encrypt, false: Decrypt
     * @return Encrypted or decrypted data
     
     * [AUTO-TRANSLATED:a6e4aeb0]
     */
    static std::string cryptWithRsaPrivateKey(EVP_PKEY *private_key, const std::string &in_str, bool enc_or_dec);

    /**
     * 获取证书域名
     * @param cer 证书公钥
     * @return 证书域名
     * Get certificate domain name
     * @param cer Certificate public key
     * @return Certificate domain name
     
     * [AUTO-TRANSLATED:b3806b53]
     */
    static std::string getServerName(X509 *cer);
};

}//namespace toolkit
#endif //ZLTOOLKIT_SSLUTIL_H
