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

#ifndef UTIL_ONCETOKEN_H_
#define UTIL_ONCETOKEN_H_

#include <functional>
#include <type_traits>

using namespace std;

namespace toolkit {

class onceToken {
public:
	typedef function<void(void)> task;
	onceToken(const task &onConstructed, const task &onDestructed = nullptr) {
		if (onConstructed) {
			onConstructed();
		}
		_onDestructed = onDestructed;
	}
	onceToken(const task &onConstructed, task &&onDestructed) {
		if (onConstructed) {
			onConstructed();
		}
		_onDestructed = std::move(onDestructed);
	}
	~onceToken() {
		if (_onDestructed) {
			_onDestructed();
		}
	}
private:
	onceToken();
	onceToken(const onceToken &);
	onceToken(onceToken &&);
	onceToken &operator =(const onceToken &);
	onceToken &operator =(onceToken &&);
	task _onDestructed;
};

} /* namespace toolkit */

#endif /* UTIL_ONCETOKEN_H_ */
