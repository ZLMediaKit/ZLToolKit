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

    setContext(SSLUtil::makeSSLContext(nullptr, nullptr, false),false);
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

bool SSL_Initor::loadServerPem(const char *pem_or_p12, const char *passwd){
	return loadCertificate(pem_or_p12, true,passwd, true);

}
bool SSL_Initor::loadClientPem(const char *pem_or_p12, const char *passwd){
	return loadCertificate(pem_or_p12,false,passwd, true);
}

bool SSL_Initor::loadCertificate(const string &pem_or_p12,  bool serverMode, const string &passwd , bool isFile){
	return loadCertificate(SSLUtil::loadPublicKey(pem_or_p12, passwd, isFile).get(),
						   SSLUtil::loadPrivateKey(pem_or_p12, passwd, isFile).get(),
						   serverMode);
}
bool SSL_Initor::loadCertificate(X509 *public_key, EVP_PKEY *private_key, bool serverMode) {
	return setContext(SSLUtil::makeSSLContext(public_key, private_key, serverMode),serverMode);
}

bool SSL_Initor::setContext(const shared_ptr<SSL_CTX> &ctx, bool serverMode) {
#if defined(ENABLE_OPENSSL)
	auto &ref = serverMode ? _ctx_server : _ctx_client;
	ref = ctx;
	if(!ref){
		WarnL << "证书无效!";
		return false;
	}
	setupCtx(ref.get());
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
	SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);

	SSL_CTX_set_default_verify_paths(ctx);
	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER,[](int ok, X509_STORE_CTX *pStore) {
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
    if(serverMode){
		return _ctx_server ? SSLUtil::makeSSL(_ctx_server.get()) : nullptr;
	}
	return _ctx_client ? SSLUtil::makeSSL(_ctx_client.get()) : nullptr;
#else
    return nullptr;
#endif //defined(ENABLE_OPENSSL)
}

bool SSL_Initor::trustCertificate(X509 *cer, bool serverMode) {
	return SSLUtil::trustCertificate(serverMode ? _ctx_server.get() : _ctx_client.get(),cer);
}

bool SSL_Initor::trustCertificate(const string &pem_p12_cer, bool serverMode, const string &passwd, bool isFile) {
	return trustCertificate(SSLUtil::loadPublicKey(pem_p12_cer,passwd,isFile).get(),serverMode);
}
////////////////////////////////////////////////////SSL_Box////////////////////////////////////////////////////////////

SSL_Box::SSL_Box(bool serverMode,
				 bool enable,
				 int buffSize) {
#if defined(ENABLE_OPENSSL)
    _read_bio = BIO_new(BIO_s_mem());
    _serverMode = serverMode;
    if(enable){
        _ssl =  SSL_Initor::Instance().makeSSL(serverMode) ;
    }
    if(_ssl){
		_write_bio = BIO_new(BIO_s_mem());
		SSL_set_bio(_ssl.get(), _read_bio, _write_bio);
		_serverMode ? SSL_set_accept_state(_ssl.get()) : SSL_set_connect_state(_ssl.get());

	} else {
		WarnL << "ssl disabled!";
	}
	_sendHandshake = false;
	_bufferBio = std::make_shared<BufferRaw>(buffSize);
#endif //defined(ENABLE_OPENSSL)
}

SSL_Box::~SSL_Box() {}

void SSL_Box::shutdown() {
#if defined(ENABLE_OPENSSL)
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
    BIO_write(_read_bio, buffer->data(), buffer->size());
	flush();
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
	int buf_size = _bufferBio->getCapacity() - 1;
	do{
		nread = BIO_read(_write_bio, _bufferBio->data() + total, buf_size - total);
		if(nread > 0){
			total += nread;
		}
	}while(nread > 0 && buf_size - total  > 0);

	if(!total){
		//未有数据
		return;
	}

	//触发此次回调
	_bufferBio->data()[total] = '\0';
	_bufferBio->setSize(total);
	if(_onEnc){
		_onEnc(_bufferBio);
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
	int buf_size = _bufferBio->getCapacity() - 1;
	do{
		nread = SSL_read(_ssl.get(), _bufferBio->data() + total, buf_size - total);
		if(nread > 0){
			total += nread;
		}
	}while(nread > 0 && buf_size - total  > 0);

	if(!total){
		//未有数据
		return;
	}

	//触发此次回调
	_bufferBio->data()[total] = '\0';
	_bufferBio->setSize(total);
	if(_onDec){
		_onDec(_bufferBio);
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
	flushWriteBio();
	if (SSL_is_init_finished(_ssl.get()) && !_bufferOut.empty()) {
		while (!_bufferOut.empty()){
			auto nwrite = SSL_write(_ssl.get(), _bufferOut.front()->data(), _bufferOut.front()->size());
			if (nwrite > 0) {
				_bufferOut.pop_front();
				continue;
			}
			ErrorL << "ssl error:" << SSLUtil::getLastError() ;
		}
		flushWriteBio();
	}
#endif //defined(ENABLE_OPENSSL)
}


} /* namespace toolkit */
