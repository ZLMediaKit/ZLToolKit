//
// Created by xzl on 2019/4/12.
//

#ifndef ZLTOOLKIT_SSLUTIL_H
#define ZLTOOLKIT_SSLUTIL_H

#include <memory>
#include <string>
using namespace std;

typedef struct x509_st X509;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;
typedef struct bio_st BIO;

/**
 * ssl证书后缀一般分为以下几种
 * pem:这个是base64的字符编码串，可能存在公钥、私钥或者两者都存在
 * cer:只且只能是公钥，可以与pem的私钥配合使用
 * p12:必须包括私钥和公钥
 */
class SSLUtil {
public:
    static std::string getLastError();

    /**
     * 加载公钥证书，支持pem,p12,cer后缀
     * 由于openssl加载p12证书时会校验公钥和私钥是否匹对，所以加载p12的公钥时可能需要传入证书密码
     * @param file_path_or_data 文件路径或文件内容
     * @param isFile 是否为文件
     * @return 公钥证书
     */
    static shared_ptr<X509> loadPublicKey(const string &file_path_or_data,const string &passwd = "",bool isFile = true);

    /**
     * 加载私钥证书，支持pem,p12后缀
     * @param file_path_or_data 文件路径或文件内容
     * @param passwd 密码
     * @param isFile 是否为文件
     * @return 私钥证书
     */
    static shared_ptr<EVP_PKEY> loadPrivateKey(const string &file_path_or_data,const string &passwd = "",bool isFile = true);

    /**
     * 创建SSL_CTX对象
     * @param cer 公钥
     * @param key 私钥
     * @param serverMode 是否为服务器模式或客户端模式
     * @return SSL_CTX对象
     */
    static shared_ptr<SSL_CTX> makeSSLContext(X509 *cer, EVP_PKEY *key, bool serverMode = true);

    /**
     * 创建ssl对象
     * @param ctx SSL_CTX对象
     * @return
     */
    static shared_ptr<SSL> makeSSL(SSL_CTX *ctx);
};



#endif //ZLTOOLKIT_SSLUTIL_H
