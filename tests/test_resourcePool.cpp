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
#include <signal.h>
#include <iostream>
#include <random>
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/ResourcePool.h"
#include "Thread/threadgroup.h"
#include <list>

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

//程序退出标志
bool g_bExitFlag = false;

//大小为50的循环池
ResourcePool<string,50> g_pool;

//后台线程任务
void onRun(int threadNum){
	std::random_device rd;
	while(!g_bExitFlag){
        //从循环池获取一个可用的对象
		auto obj_ptr = g_pool.obtain();
		if(obj_ptr->empty()){
            //这个对象是全新未使用的
			InfoL << "thread " << threadNum << ":" << "obtain a emptry object!";
		}else{
            //这个对象是循环使用的
			InfoL << "thread " << threadNum << ":" << *obj_ptr;
		}
        //标记该对象被本线程使用
		obj_ptr->assign(StrPrinter << "keeped by thread:" << threadNum );

        //随机休眠，打乱循环使用顺序
        usleep( 1000 * (rd()% 10));
		obj_ptr.reset();//手动释放，也可以注释这句代码。根据RAII的原理，该对象会被自动释放并重新进入循环列队
		usleep( 1000 * (rd()% 1000));
	}
}

int main() {
    //初始化日志
	Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

	//获取一个对象,该对象将被主线程持有，并且不会被后台线程获取并赋值
	auto reservedObj = g_pool.obtain();
    //在主线程赋值该对象
	reservedObj->assign("This is a reserved object , and will never be used!");

	thread_group group;
    //创建4个后台线程，该4个线程模拟循环池的使用场景，
    //理论上4个线程在同一时间最多同时总共占用4个对象
	for(int i = 0 ;i < 4 ; ++i){
		group.create_thread([i](){
			onRun(i);
		});
	}

    //等待3秒钟，此时循环池里面可用的对象基本上最少都被使用过一遍了
	sleep(3);

	//但是由于reservedObj早已被主线程持有，后台线程是获取不到该对象的
    //所以其值应该尚未被覆盖
	WarnL << *reservedObj << endl;

    //获取该对象的引用
	auto &objref = *reservedObj;

    //显式释放对象,让对象重新进入循环列队，这时该对象应该会被后台线程持有并赋值
	reservedObj.reset();

    //再休眠3秒，让reservedObj被后台线程循环使用
	sleep(3);

	//这时，reservedObj还在循环池内，引用应该还是有效的，但是值应该被覆盖了
	WarnL << objref << endl;

    //通知后台线程退出
	g_bExitFlag = true;
    //等待后台线程退出
	group.join_all();
	Logger::Destory();
	return 0;
}











