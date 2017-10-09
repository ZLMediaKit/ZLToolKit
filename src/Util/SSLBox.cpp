/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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
 
#if defined(ENABLE_OPENSSL)

#include <string.h>
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include "SSLBox.h"
#include "Util/util.h"

#define SSL_BUF_SIZE 1024*4

namespace ZL {
namespace Util {
mutex *SSL_Initor::_mutexes;

SSL_Initor::SSL_Initor() {
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_digests();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_algorithms();
	_mutexes = new mutex[CRYPTO_num_locks()];
	CRYPTO_set_locking_callback([](int mode,int n,
			const char *file,int line) {
		if (mode & CRYPTO_LOCK)
		_mutexes[n].lock();
		else
		_mutexes[n].unlock();
	});
	CRYPTO_set_id_callback([]() ->unsigned long {
#if !defined(_WIN32)
		return (unsigned long)pthread_self();
#else
		return (unsigned long)GetCurrentThreadId();
#endif
	});
	ssl_client = SSL_CTX_new(TLSv1_client_method());
	ssl_server = SSL_CTX_new(TLSv1_server_method());
	setCtx(ssl_client);
	setCtx(ssl_server);
}

void SSL_Initor::loadServerPem(const char *keyAndCA_pem, const char *import_pwd){
	loadPem(ssl_server,keyAndCA_pem,import_pwd);
}
void SSL_Initor::loadClientPem(const char *keyAndCA_pem, const char *import_pwd){
	loadPem(ssl_client,keyAndCA_pem,import_pwd);
}
void SSL_Initor::loadPem(SSL_CTX *ctx, const char *keyAndCA_pem,const char *import_pwd) {
	int errCode = SSL_CTX_use_PrivateKey_file(ctx, keyAndCA_pem,SSL_FILETYPE_PEM);
	if (errCode != 1) {
		throw std::runtime_error(std::string("SSL_CTX_use_PrivateKey_file: ") + getLastError());
	}
	errCode = SSL_CTX_use_certificate_file(ctx, keyAndCA_pem,SSL_FILETYPE_PEM);
	if (errCode != 1) {
		throw std::runtime_error(std::string("SSL_CTX_use_certificate_chain_file: ")+ getLastError());
	}
	SSL_CTX_set_default_passwd_cb_userdata(ctx, (void *) import_pwd);
	SSL_CTX_set_default_passwd_cb(ctx,[](char *buf, int size, int rwflag, void *userdata)->int {
				const char *privateKeyPwd=(const char *)userdata;
				size_t privateKeyPwd_len=strlen(privateKeyPwd);
				strncpy(buf, privateKeyPwd, size);
				buf[size - 1] = '\0';
				if (size > (int)privateKeyPwd_len)
				size = privateKeyPwd_len;
				return size;
	});
	errCode = SSL_CTX_check_private_key(ctx);
	if (errCode != 1) {
		throw std::runtime_error(std::string("SSL_CTX_check_private_key: ") + getLastError());
	}
}

SSL_Initor::~SSL_Initor() {
	SSL_CTX_free(ssl_client);
	SSL_CTX_free(ssl_server);
	EVP_cleanup();
	ERR_free_strings();
	ERR_clear_error();
	ERR_remove_state(0);
	CRYPTO_set_locking_callback(NULL);
	sk_SSL_COMP_free(SSL_COMP_get_compression_methods());
	CRYPTO_cleanup_all_ex_data();
	CONF_modules_unload(1);
	CONF_modules_free();
	delete[] _mutexes;
}
void SSL_Initor::setCtx(SSL_CTX *ctx) {
	SSL_CTX_set_cipher_list(ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
	SSL_CTX_set_verify_depth(ctx, 9);
	SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
	SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
	SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE,[](int ok, X509_STORE_CTX *pStore) {
				if (!ok) {
					int depth = X509_STORE_CTX_get_error_depth(pStore);
					int err = X509_STORE_CTX_get_error(pStore);
					std::string error(X509_verify_cert_error_string(err));
					ErrorL<<depth<<" "<<error<<endl;
					ok = 1;
				}
				return ok;
	});
}
inline std::string SSL_Initor::getLastError(){
	unsigned long errCode = ERR_get_error();
	if (errCode != 0) {
		char buffer[256];
		ERR_error_string_n(errCode, buffer, sizeof(buffer));
		return std::string(buffer);
	} else
		return "No error";
}
SSL_Box::SSL_Box(bool _isServer, bool _enable) :
		ssl(nullptr), read_bio(nullptr), write_bio(nullptr) {
	isServer = _isServer;
	enable = _enable;
	ssl = SSL_new(isServer ?SSL_Initor::Instance().ssl_server :SSL_Initor::Instance().ssl_client);
	read_bio = BIO_new(BIO_s_mem());
	write_bio = BIO_new(BIO_s_mem());
	SSL_set_bio(ssl, read_bio, write_bio);
	isServer ? SSL_set_accept_state(ssl) : SSL_set_connect_state(ssl);
	sendHandshake = false;
}

SSL_Box::~SSL_Box() {
	if (ssl) {
		SSL_free(ssl);
		ssl = nullptr;
	}
	ERR_clear_error();
	ERR_remove_state(0);
}

void SSL_Box::shutdown() {
	int ret = SSL_shutdown(ssl);
	if (ret != 1) {
		ErrorL << "SSL shutdown failed:"<< ERR_reason_error_string(ERR_get_error()) << endl;
	} else {
		flush();
	}
}
void SSL_Box::onRecv(const char* data, uint32_t data_len) {
	if (!enable) {
		if (onDec) {
			onDec(data, data_len);
		}
		return;
	}
	BIO_write(read_bio, data, data_len);
	flush();
}

void SSL_Box::onSend(const char* data, uint32_t data_len) {
	if (!enable) {
		if (onEnc) {
			onEnc(data, data_len);
		}
		return;
	}
	if (!isServer && !sendHandshake) {
		sendHandshake = true;
		SSL_do_handshake(ssl);
	}
	_bufferOut.append(data, data_len);
	flush();
}
void SSL_Box::flushWriteBio(char *buf, int bufsize) {
	int nread = 0;
	//write to socket
	while ((nread = BIO_read(write_bio, buf, bufsize)) > 0) {
		if (onEnc) {
			//send
			onEnc(buf, nread);
		}
	}
}

void SSL_Box::flushReadBio(char *buf, int bufsize) {
	int nread = 0;
	//recv from bio
	while ((nread = SSL_read(ssl, buf, bufsize)) > 0) {
		if (onDec) {
			//recv
			onDec(buf, nread);
		}
	}
}
void SSL_Box::flush() {
	int nread = 0;
	char buffer[SSL_BUF_SIZE]; // optimize!
	flushReadBio(buffer, sizeof(buffer));
	flushWriteBio(buffer, sizeof(buffer));
	//write to bio
	if (SSL_is_init_finished(ssl) && _bufferOut.size()) {
		nread = SSL_write(ssl, _bufferOut.data(), _bufferOut.size());
		if (nread >= 0) {
			//success
			_bufferOut.clear();
			flushWriteBio(buffer, sizeof(buffer));
		} else {
			int error = SSL_get_error(ssl, nread);
			ErrorL << "ssl error:" << error << endl;
		}
	}
}

} /* namespace Util */
} /* namespace ZL */
#endif //ENABLE_OPENSSL
