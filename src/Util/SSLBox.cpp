/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include "SSLBox.h"
#include "util.h"
#include "onceToken.h"
#include "SSLUtil.h"

#if defined(ENABLE_OPENSSL)
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/bio.h>
#include <openssl/ossl_typ.h>

#if defined(_WIN32)
#if defined(_WIN64)

//64bit
#if !defined(NDEBUG)
#pragma  comment (lib,"libssl64MDd")
#pragma  comment (lib,"libcrypto64MDd")
#else
#pragma  comment (lib,"libssl64MD")
#pragma  comment (lib,"libcrypto64MD")
#endif // !defined(NDEBUG)

#else

//32 bit
#if !defined(NDEBUG)
#pragma  comment (lib,"libssl32MDd")
#pragma  comment (lib,"libcrypto32MDd")
#else
#pragma  comment (lib,"libssl32MD")
#pragma  comment (lib,"libcrypto32MD")
#endif // !defined(NDEBUG)

#endif //defined(_WIN64)
#endif // defined(_WIN32)

#endif //defined(ENABLE_OPENSSL)

#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
//openssl版本是否支持sni
#define SSL_ENABLE_SNI
#endif

namespace toolkit {

static bool s_ingroleSsl = true;

SSL_Initor &SSL_Initor::Instance() {
    static SSL_Initor obj;
    return obj;
}

void SSL_Initor::ignoreInvalidCertificate(bool ignore) {
    s_ingroleSsl = ignore;
}

SSL_Initor::SSL_Initor() {
#if defined(ENABLE_OPENSSL)
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_digests();
    OpenSSL_add_all_ciphers();
    OpenSSL_add_all_algorithms();
    CRYPTO_set_locking_callback([](int mode,int n,
            const char *file,int line) {
        static mutex *s_mutexes = new mutex[CRYPTO_num_locks()];
        static onceToken token(nullptr,[](){
            delete [] s_mutexes;
        });
        if (mode & CRYPTO_LOCK){
            s_mutexes[n].lock();
        } else {
            s_mutexes[n].unlock();
        }
    });

    CRYPTO_set_id_callback([]() ->unsigned long {
#if !defined(_WIN32)
        return (unsigned long)pthread_self();
#else
        return (unsigned long)GetCurrentThreadId();
#endif
    });

    setContext("",SSLUtil::makeSSLContext(vector<shared_ptr<X509> >(), nullptr, false),false);
    setContext("",SSLUtil::makeSSLContext(vector<shared_ptr<X509> >(), nullptr, true),true);
#endif //defined(ENABLE_OPENSSL)
}

SSL_Initor::~SSL_Initor() {
#if defined(ENABLE_OPENSSL)
    EVP_cleanup();
    ERR_free_strings();
    ERR_clear_error();
    ERR_remove_state(0);
    CRYPTO_set_locking_callback(NULL);
    //sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
    CRYPTO_cleanup_all_ex_data();
    CONF_modules_unload(1);
    CONF_modules_free();
#endif //defined(ENABLE_OPENSSL)
}

bool SSL_Initor::loadCertificate(const string &pem_or_p12, bool serverMode, const string &passwd, bool isFile, bool isDefault) {
    auto cers = SSLUtil::loadPublicKey(pem_or_p12, passwd, isFile);
    auto key = SSLUtil::loadPrivateKey(pem_or_p12, passwd, isFile);
    auto ssl_ctx = SSLUtil::makeSSLContext(cers, key, serverMode);
    if(!ssl_ctx){
        return false;
    }
    for (auto &cer : cers) {
        auto server_name = SSLUtil::getServerName(cer.get());
        setContext(server_name, ssl_ctx, serverMode, isDefault);
        break;
    }
    return true;
}

int SSL_Initor::findCertificate(SSL *ssl, int *ad, void *arg) {
#if !defined(ENABLE_OPENSSL) || !defined(SSL_ENABLE_SNI)
    return 0;
#else
    if(!ssl){
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    SSL_CTX* ctx = NULL;
    static auto &ref = SSL_Initor::Instance();
    const char *vhost = SSL_get_servername(ssl,TLSEXT_NAMETYPE_host_name);

    if (vhost && vhost[0] != '\0') {
        //从map中找到vhost对应的SSL_CTX
        ctx =  ref.getSSLCtx(vhost,(bool)(arg)).get();
    } else {
        //选一个默认的SSL_CTX
        ctx =  ref.getSSLCtx("",(bool)(arg)).get();
        if(ctx){
            DebugL << "client does not specify host, select default certificate of host: " << ref.defaultVhost((bool)(arg));
        } else{
            vhost = "default host";
        }
    }

    if(!ctx){
        //未找到对应的证书
        DebugL << "can not find any certificate of host:" << vhost;
        return SSL_TLSEXT_ERR_ALERT_FATAL;
    }

    SSL_set_SSL_CTX(ssl, ctx);
    return SSL_TLSEXT_ERR_OK;
#endif
}

bool SSL_Initor::setContext(const string &vhost,const shared_ptr<SSL_CTX> &ctx, bool serverMode, bool isDefault) {
    if(!ctx){
        return false;
    }
    setupCtx(ctx.get());

#if defined(ENABLE_OPENSSL)
    if(vhost.empty()){
        _ctx_empty[serverMode] = ctx;

#ifdef SSL_ENABLE_SNI
        if(serverMode){
            SSL_CTX_set_tlsext_servername_callback(ctx.get(), findCertificate);
            SSL_CTX_set_tlsext_servername_arg(ctx.get(),(void*)serverMode);
        }
#endif // SSL_ENABLE_SNI

    }else{
        _ctxs[serverMode][vhost] = ctx;
        if(isDefault){
            _default_vhost[serverMode] = vhost;
        }
        DebugL << "add certificate of: " << vhost;
    }
    return true;
#else
    WarnL << "ENABLE_OPENSSL宏未启用,openssl相关功能将无效!";
    return false;
#endif //defined(ENABLE_OPENSSL)
}


void SSL_Initor::setupCtx(SSL_CTX *ctx) {
#if defined(ENABLE_OPENSSL)
    //加载默认信任证书
    SSLUtil::loadDefaultCAs(ctx);
    SSL_CTX_set_cipher_list(ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
    SSL_CTX_set_verify_depth(ctx, 9);
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE,[](int ok, X509_STORE_CTX *pStore) {
                if (!ok) {
                    int depth = X509_STORE_CTX_get_error_depth(pStore);
                    int err = X509_STORE_CTX_get_error(pStore);
                    std::string error(X509_verify_cert_error_string(err));
                    WarnL << depth << " " << error;
                }
                if(s_ingroleSsl){
                    ok = 1;
                }
                return ok;
    });

#endif //defined(ENABLE_OPENSSL)
}

shared_ptr<SSL> SSL_Initor::makeSSL(bool serverMode) {
#if defined(ENABLE_OPENSSL)
#ifdef SSL_ENABLE_SNI
    //openssl 版本支持SNI
    return SSLUtil::makeSSL(_ctx_empty[serverMode].get());
#else
    //openssl 版本不支持SNI，选择默认证书
    return SSLUtil::makeSSL(getSSLCtx("",serverMode).get());
#endif//SSL_CTRL_SET_TLSEXT_HOSTNAME
#else
    return nullptr;
#endif //defined(ENABLE_OPENSSL)
}

bool SSL_Initor::trustCertificate(X509 *cer, bool serverMode) {
    return SSLUtil::trustCertificate(_ctx_empty[serverMode].get(),cer);
}

bool SSL_Initor::trustCertificate(const string &pem_p12_cer, bool serverMode, const string &passwd, bool isFile) {
    auto cers = SSLUtil::loadPublicKey(pem_p12_cer,passwd,isFile);
    for(auto &cer : cers){
        trustCertificate(cer.get(),serverMode);
    }
    return true;
}

std::shared_ptr<SSL_CTX> SSL_Initor::getSSLCtx(const string &vhost,bool serverMode){
    if(!serverMode){
        return _ctx_empty[serverMode];
    }

    if(vhost.empty()){
        return _ctxs[serverMode][_default_vhost[serverMode]];
    }
    return _ctxs[serverMode][vhost];
}

string SSL_Initor::defaultVhost(bool serverMode) {
    return _default_vhost[serverMode];
}

////////////////////////////////////////////////////SSL_Box////////////////////////////////////////////////////////////
SSL_Box::~SSL_Box() {}

SSL_Box::SSL_Box(bool serverMode, bool enable, int buffSize) {
#if defined(ENABLE_OPENSSL)
    _read_bio = BIO_new(BIO_s_mem());
    _serverMode = serverMode;
    if(enable){
        _ssl = SSL_Initor::Instance().makeSSL(serverMode);
    }
    if(_ssl){
        _write_bio = BIO_new(BIO_s_mem());
        SSL_set_bio(_ssl.get(), _read_bio, _write_bio);
        _serverMode ? SSL_set_accept_state(_ssl.get()) : SSL_set_connect_state(_ssl.get());
    } else {
        WarnL << "ssl disabled!";
    }
    _sendHandshake = false;
    _buffSize = buffSize;
#endif //defined(ENABLE_OPENSSL)
}

void SSL_Box::shutdown() {
#if defined(ENABLE_OPENSSL)
    _bufferOut.clear();
    int ret = SSL_shutdown(_ssl.get());
    if (ret != 1) {
        ErrorL << "SSL shutdown failed:" << SSLUtil::getLastError();
    } else {
        flush();
    }
#endif //defined(ENABLE_OPENSSL)
}

void SSL_Box::onRecv(const Buffer::Ptr &buffer) {
    if(!buffer->size()){
        return;
    }
    if (!_ssl) {
        if (_onDec) {
            _onDec(buffer);
        }
        return;
    }
#if defined(ENABLE_OPENSSL)
    uint32_t offset = 0;
    while(offset < buffer->size()){
        auto nwrite = BIO_write(_read_bio, buffer->data() + offset, buffer->size() - offset);
        if (nwrite > 0) {
            //部分或全部写入bio完毕
            offset += nwrite;
            flush();
            continue;
        }
        //nwrite <= 0,出现异常
        ErrorL << "ssl error:" << SSLUtil::getLastError();
        shutdown();
        break;
    }
#endif //defined(ENABLE_OPENSSL)
}

void SSL_Box::onSend(const Buffer::Ptr &buffer) {
    if(!buffer->size()){
        return;
    }
    if (!_ssl) {
        if (_onEnc) {
            _onEnc(buffer);
        }
        return;
    }
#if defined(ENABLE_OPENSSL)
    if (!_serverMode && !_sendHandshake) {
        _sendHandshake = true;
        SSL_do_handshake(_ssl.get());
    }
    _bufferOut.emplace_back(buffer);
    flush();
#endif //defined(ENABLE_OPENSSL)
}

void SSL_Box::flushWriteBio() {
#if defined(ENABLE_OPENSSL)
    int total = 0;
    int nread = 0;
    auto bufferBio = _bufferPool.obtain();
    bufferBio->setCapacity(_buffSize);
    int buf_size = bufferBio->getCapacity() - 1;
    do{
        nread = BIO_read(_write_bio, bufferBio->data() + total, buf_size - total);
        if(nread > 0){
            total += nread;
        }
    }while(nread > 0 && buf_size - total  > 0);

    if(!total){
        //未有数据
        return;
    }

    //触发此次回调
    bufferBio->data()[total] = '\0';
    bufferBio->setSize(total);
    if(_onEnc){
        _onEnc(bufferBio);
    }

    if(nread > 0){
        //还有剩余数据，读取剩余数据
        flushWriteBio();
    }
#endif //defined(ENABLE_OPENSSL)
}

void SSL_Box::flushReadBio() {
#if defined(ENABLE_OPENSSL)
    int total = 0;
    int nread = 0;
    auto bufferBio = _bufferPool.obtain();
    bufferBio->setCapacity(_buffSize);
    int buf_size = bufferBio->getCapacity() - 1;
    do{
        nread = SSL_read(_ssl.get(), bufferBio->data() + total, buf_size - total);
        if(nread > 0){
            total += nread;
        }
    }while(nread > 0 && buf_size - total  > 0);

    if(!total){
        //未有数据
        return;
    }

    //触发此次回调
    bufferBio->data()[total] = '\0';
    bufferBio->setSize(total);
    if(_onDec){
        _onDec(bufferBio);
    }

    if(nread > 0){
        //还有剩余数据，读取剩余数据
        flushReadBio();
    }
#endif //defined(ENABLE_OPENSSL)
}
void SSL_Box::flush() {
#if defined(ENABLE_OPENSSL)
    flushReadBio();
    if (!SSL_is_init_finished(_ssl.get()) || _bufferOut.empty()) {
        //ssl未握手结束或没有需要发送的数据
        flushWriteBio();
        return;
    }

    //加密数据并发送
    while (!_bufferOut.empty()){
        auto &front = _bufferOut.front();
        uint32_t offset = 0;
        while(offset < front->size()){
            auto nwrite = SSL_write(_ssl.get(), front->data() + offset, front->size() - offset);
            if (nwrite > 0) {
                //部分或全部写入完毕
                offset += nwrite;
                flushWriteBio();
                continue;
            }
            //nwrite <= 0,出现异常
            break;
        }

        if(offset != front->size()){
            //这个包未消费完毕，出现了异常,清空数据并断开ssl
            ErrorL << "ssl error:" << SSLUtil::getLastError() ;
            shutdown();
            break;
        }

        //这个包消费完毕，开始消费下一个包
        _bufferOut.pop_front();
    }
#endif //defined(ENABLE_OPENSSL)
}

bool SSL_Box::setHost(const char *host) {
#ifdef SSL_ENABLE_SNI
    return 0 != SSL_set_tlsext_host_name(_ssl.get(), host);
#else
    return false;
#endif//SSL_ENABLE_SNI
}


} /* namespace toolkit */
