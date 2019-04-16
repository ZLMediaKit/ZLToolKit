//
// Created by xzl on 2019/4/12.
//

#include "SSLUtil.h"
#include "onceToken.h"
#include "logger.h"

#if defined(ENABLE_OPENSSL)
#include <openssl/bio.h>
#include <openssl/ossl_typ.h>
#include <openssl/pkcs12.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#endif //defined(ENABLE_OPENSSL)

using namespace toolkit;

std::string SSLUtil::getLastError(){
#if defined(ENABLE_OPENSSL)
    unsigned long errCode = ERR_get_error();
    if (errCode != 0) {
        char buffer[256];
        ERR_error_string_n(errCode, buffer, sizeof(buffer));
        return buffer;
    } else
#endif //defined(ENABLE_OPENSSL)
    {
        return "No error";
    }
}
shared_ptr<X509> SSLUtil::loadPublicKey(const string &file_path_or_data, const string &passwd,bool isFile) {
#if defined(ENABLE_OPENSSL)
    BIO *bio = isFile ?
              BIO_new_file(file_path_or_data.data(), "r") :
              BIO_new_mem_buf(file_path_or_data.data(),file_path_or_data.size());
    if(!bio){
        WarnL << getLastError();
        return nullptr;
    }

    onceToken token0(nullptr,[&](){
        BIO_free(bio);
    });

    //尝试pem格式
    X509 *x509 = PEM_read_bio_X509(bio, NULL, NULL ,NULL);
    if(!x509){
//        WarnL << "loadPublicKey " << file_path_or_data << " failed:" << getLastError();
        //尝试DER格式
        BIO_reset(bio);
        x509 = d2i_X509_bio(bio, NULL);
        if(!x509){
//            WarnL << "loadPublicKey " << file_path_or_data << " failed:" << getLastError();
            //尝试p12格式
            BIO_reset(bio);
            PKCS12 *p12 = d2i_PKCS12_bio(bio, NULL);
            if(!p12) {
//                WarnL << "loadPublicKey " << file_path_or_data << " failed:" << getLastError();
                return nullptr;
            }
            EVP_PKEY *pkey = NULL;
            PKCS12_parse(p12, passwd.data(), &pkey, &x509, NULL);
            PKCS12_free (p12);
            if(pkey){
                EVP_PKEY_free(pkey);
            }
            if(!x509){
//                WarnL << "loadPublicKey " << file_path_or_data << " failed:" << getLastError();
                return nullptr;
            }
        }
    }

    return shared_ptr<X509>(x509,[](X509 *ptr){
        X509_free(ptr);
    });
#else
    return nullptr;
#endif //defined(ENABLE_OPENSSL)
}

shared_ptr<EVP_PKEY> SSLUtil::loadPrivateKey(const string &file_path_or_data, const string &passwd,bool isFile) {
#if defined(ENABLE_OPENSSL)
    BIO *bio = isFile ?
               BIO_new_file(file_path_or_data.data(), "r") :
               BIO_new_mem_buf(file_path_or_data.data(),file_path_or_data.size());
    if(!bio){
        WarnL << getLastError();
        return nullptr;
    }

    pem_password_cb *cb = [](char *buf, int size, int rwflag, void *userdata) -> int{
        const string *passwd = (const string *)userdata;
        size = size < passwd->size() ? size :  passwd->size();
        memcpy(buf,passwd->data(),size);
        return size;
    };

    onceToken token0(nullptr,[&](){
        BIO_free(bio);
    });

    //尝试pem格式
    EVP_PKEY *evp_key = PEM_read_bio_PrivateKey(bio, NULL, cb ,(void *)&passwd);
    if(!evp_key){
//        WarnL << "loadPrivateKey " << file_path_or_data << " failed:" << getLastError();
        //尝试p12格式
        BIO_reset(bio);
        PKCS12 *p12 = d2i_PKCS12_bio(bio, NULL);
        if(!p12) {
//            WarnL << "loadPrivateKey " << file_path_or_data << " failed:" << getLastError();
            return nullptr;
        }
        X509 *x509 = NULL;
        PKCS12_parse(p12, passwd.data(), &evp_key, &x509, NULL);
        PKCS12_free (p12);
        if(x509){
            X509_free(x509);
        }
        if(!evp_key){
//            WarnL << "loadPrivateKey " << file_path_or_data << " failed:" << getLastError();
            return nullptr;
        }
    }

    return shared_ptr<EVP_PKEY>(evp_key,[](EVP_PKEY *ptr){
        EVP_PKEY_free(ptr);
    });
#else
    return nullptr;
#endif //defined(ENABLE_OPENSSL)
}

shared_ptr<SSL_CTX> SSLUtil::makeSSLContext(X509 *cer, EVP_PKEY *key,bool serverMode) {
#if defined(ENABLE_OPENSSL)
    SSL_CTX *ctx = SSL_CTX_new(serverMode ? TLS_server_method() : TLS_client_method() );
    if(!ctx){
        WarnL << getLastError();
        return nullptr;
    }

    do{
        if (cer && SSL_CTX_use_certificate(ctx,cer) != 1 ) {
            break;
        }

        if (key && SSL_CTX_use_PrivateKey(ctx, key) != 1) {
            break;
        }

        if(cer && key && SSL_CTX_check_private_key(ctx) != 1 ){
            break;
        }

        return shared_ptr<SSL_CTX>(ctx,[](SSL_CTX *ptr){
            SSL_CTX_free(ptr);
        });
    }while(false);

    WarnL << getLastError();
    SSL_CTX_free(ctx);
    return nullptr;
#else
    return nullptr;
#endif //defined(ENABLE_OPENSSL)
}

shared_ptr<SSL> SSLUtil::makeSSL(SSL_CTX *ctx) {
#if defined(ENABLE_OPENSSL)
    auto *ssl = SSL_new(ctx);
    if(!ssl){
        return nullptr;
    }
    return shared_ptr<SSL>(ssl,[](SSL *ptr){
        SSL_free(ptr);
    });
#else
    return nullptr;
#endif //defined(ENABLE_OPENSSL)
}

