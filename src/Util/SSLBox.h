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

#ifndef CRYPTO_SSLBOX_H_
#define CRYPTO_SSLBOX_H_

#include <mutex>
#include <string>
#include <functional>
#include "logger.h"
#include "List.h"
#include "Network/Buffer.h"
using namespace std;

typedef struct x509_st X509;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;
typedef struct bio_st BIO;

namespace toolkit {

class SSL_Initor {
public:
	static SSL_Initor &Instance();

	/**
	 * 加载服务器证书,证书必须包含公钥和私钥，格式可以为pem和p12
	 * 该接口已经过期，建议使用loadCertificate接口
	 * @param pem_or_p12 证书文件路径
	 * @param passwd 证书加密密码
	 */
	bool loadServerPem(const char *pem_or_p12, const char *passwd = "");

	/**
	 * 加载客户端证书,证书必须包含公钥和私钥，格式可以为pem和p12
	 * 该接口已经过期，建议使用loadCertificate接口
	 * 客户端默认可以不加载证书(除非服务器要求客户端提供证书)
	 * @param pem_or_p12 证书文件路径
	 * @param passwd 证书加密密码
	 */
	bool loadClientPem(const char *pem_or_p12, const char *passwd = "");

	/**
	 * 从文件或字符串中加载公钥和私钥
	 * 该证书文件必须同时包含公钥和私钥(cer格式的证书只包括公钥，请使用后面的方法加载)
	 * 客户端默认可以不加载证书(除非服务器要求客户端提供证书)
	 * @param pem_or_p12 pem或p12文件路径或者文件内容字符串
	 * @param serverMode 是否为服务器模式
	 * @param passwd 私钥加密密码
	 * @param isFile 参数pem_or_p12是否为文件路径
	 */
	bool loadCertificate(const string &pem_or_p12,  bool serverMode = true, const string &passwd = "" , bool isFile = true);

	/**
	 * 加载公钥和私钥
	 * 公钥可以使用SSLUtil加载
	 * 客户端默认可以不加载证书(除非服务器要求客户端提供证书)
	 * @see SSLUtil
	 * @param public_key 公钥
	 * @param private_key 私钥
	 * @param serverMode 是否为服务器模式
	 */
	bool loadCertificate(X509 *public_key, EVP_PKEY *private_key, bool serverMode);

	/**
	 * 设置ssl context
	 * @param ctx ssl context
	 * @param serverMode ssl context
	 */
	bool setContext(const std::shared_ptr<SSL_CTX> &ctx,bool serverMode);

	/**
	 * 创建SSL对象
	 * @param ctx
	 * @return
	 */
	shared_ptr<SSL> makeSSL(bool serverMode);

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
	bool trustCertificate(const string &pem_p12_cer,  bool serverMode = false, const string &passwd = "" , bool isFile = true) ;

	/**
	 * 信任某证书
	 * @param cer 证书公钥
	 * @param serverMode 是否为服务模式
	 * @return 是否加载成功
	 */
	bool trustCertificate(X509 *cer,bool serverMode = false) ;
private:
	SSL_Initor();
	~SSL_Initor();
	void setupCtx(SSL_CTX *ctx);
private:
	std::shared_ptr<SSL_CTX> _ctx_server;
	std::shared_ptr<SSL_CTX> _ctx_client;
};

////////////////////////////////////////////////////////////////////////////////////

class SSL_Box {
public:
	SSL_Box(bool serverMode = true,
			bool enable = true ,
			int buffSize = 4 * 1024);
	~SSL_Box();

	//收到密文后，调用此函数解密
	void onRecv(const Buffer::Ptr &buffer);
	//需要加密明文调用此函数
	void onSend(const Buffer::Ptr &buffer);

	//设置解密后获取明文的回调
	template<typename F>
	void setOnDecData(F &&fun) {
		_onDec = std::forward<F>(fun);
	}

	//设置加密后获取密文的回调
	template<typename F>
	void setOnEncData(F &&fun) {
		_onEnc = std::forward<F>(fun);;
	}
	void shutdown();

	void flush();
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
	BufferRaw::Ptr _bufferBio;
};


} /* namespace toolkit */

#endif /* CRYPTO_SSLBOX_H_ */
