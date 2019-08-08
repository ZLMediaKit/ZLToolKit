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

namespace toolkit{

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
              BIO_new_file((char*)file_path_or_data.data(), "r") :
              BIO_new_mem_buf((char *)file_path_or_data.data(),file_path_or_data.size());
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
               BIO_new_file((char*)file_path_or_data.data(), "r") :
               BIO_new_mem_buf((char*)file_path_or_data.data(),file_path_or_data.size());
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
    SSL_CTX *ctx = SSL_CTX_new(serverMode ? SSLv23_server_method() : SSLv23_client_method() );
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

bool SSLUtil::loadDefaultCAs(SSL_CTX *ctx) {
#if defined(ENABLE_OPENSSL)
    if(!ctx){
        return false;
    }

    if(SSL_CTX_set_default_verify_paths(ctx) != 1){
        WarnL << getLastError();
        return false;
    }
    return true;
#else
    return false;
#endif //defined(ENABLE_OPENSSL)
}

bool SSLUtil::trustCertificate(SSL_CTX *ctx, X509 *cer) {
#if defined(ENABLE_OPENSSL)
    X509_STORE * store = SSL_CTX_get_cert_store(ctx);
    if(store && cer){
        if(X509_STORE_add_cert(store,cer) != 1){
            WarnL << getLastError();
            return false;
        }
        return true;
    }
#endif //defined(ENABLE_OPENSSL)
    return false;
}

bool SSLUtil::verifyX509(X509 *cer, ...) {
#if defined(ENABLE_OPENSSL)
    va_list args;
    va_start(args, cer);
    X509_STORE *store = X509_STORE_new();
    do{
        X509 *ca;
        if((ca = va_arg(args, X509*)) == NULL){
            break;
        }
        X509_STORE_add_cert(store,ca);
    }while(true);
    va_end(args);

    X509_STORE_CTX *store_ctx = X509_STORE_CTX_new();
    X509_STORE_CTX_init(store_ctx, store, cer, NULL);
    auto ret = X509_verify_cert(store_ctx);
    if ( ret != 1 ) {
        int depth = X509_STORE_CTX_get_error_depth(store_ctx);
        int err = X509_STORE_CTX_get_error(store_ctx);
        std::string error(X509_verify_cert_error_string(err));
        WarnL << depth << " " << error;
    }

    X509_STORE_CTX_free(store_ctx);
    X509_STORE_free(store);
    return ret == 1;
#else
    WarnL << "ENABLE_OPENSSL宏未启用,openssl相关功能将无效!";
    return false;
#endif //defined(ENABLE_OPENSSL)
}

#ifdef ENABLE_OPENSSL
#ifndef X509_F_X509_PUBKEY_GET0
EVP_PKEY *X509_get0_pubkey(X509 *x){
    EVP_PKEY *ret = X509_get_pubkey(x);
    if(ret){
        EVP_PKEY_free(ret);
    }
    return ret;
}
#endif //X509_F_X509_PUBKEY_GET0

#ifndef EVP_F_EVP_PKEY_GET0_RSA
RSA *EVP_PKEY_get0_RSA(EVP_PKEY *pkey){
    RSA *ret = EVP_PKEY_get1_RSA(pkey);
    if(ret){
        RSA_free(ret);
    }
    return ret;
}
#endif //EVP_F_EVP_PKEY_GET0_RSA
#endif //ENABLE_OPENSSL

string SSLUtil::cryptWithRsaPublicKey(X509 *cer, const string &in_str, bool enc_or_dec) {
#if defined(ENABLE_OPENSSL)
    EVP_PKEY *public_key = X509_get0_pubkey(cer);
    if(!public_key){
        return "";
    }
    RSA *rsa = EVP_PKEY_get0_RSA(public_key);
    if(!rsa){
        return "";
    }
    string out_str(RSA_size(rsa),'\0');
    int ret = 0;
    if(enc_or_dec){
        ret = RSA_public_encrypt(in_str.size(),(uint8_t *)in_str.data(),(uint8_t *)out_str.data(),rsa,RSA_PKCS1_PADDING);
    } else {
        ret = RSA_public_decrypt(in_str.size(),(uint8_t *)in_str.data(),(uint8_t *)out_str.data(),rsa,RSA_PKCS1_PADDING);
    }
    if(ret > 0){
        out_str.resize(ret);
        return out_str;
    }
    WarnL << getLastError();
    return "";
#else
    WarnL << "ENABLE_OPENSSL宏未启用,openssl相关功能将无效!";
    return "";
#endif //defined(ENABLE_OPENSSL)
}



string SSLUtil::cryptWithRsaPrivateKey(EVP_PKEY *private_key, const string &in_str, bool enc_or_dec) {
#if defined(ENABLE_OPENSSL)
    RSA *rsa = EVP_PKEY_get0_RSA(private_key);
    if(!rsa){
        return "";
    }
    string out_str(RSA_size(rsa),'\0');
    int ret = 0;
    if(enc_or_dec){
        ret = RSA_private_encrypt(in_str.size(),(uint8_t *)in_str.data(),(uint8_t *)out_str.data(),rsa,RSA_PKCS1_PADDING);
    } else {
        ret = RSA_private_decrypt(in_str.size(),(uint8_t *)in_str.data(),(uint8_t *)out_str.data(),rsa,RSA_PKCS1_PADDING);
    }
    if(ret > 0){
        out_str.resize(ret);
        return out_str;
    }
    WarnL << getLastError();
    return "";
#else
    WarnL << "ENABLE_OPENSSL宏未启用,openssl相关功能将无效!";
    return "";
#endif //defined(ENABLE_OPENSSL)
}

}//namespace toolkit
