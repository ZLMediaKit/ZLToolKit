源代码放置在`src`文件夹下，里面有若干模块：

```
src
|
|-- NetWork				# 网络模块
|	|-- Socket.cpp			# 套接字抽象封装，包含了TCP服务器/客户端，UDP套接字
|	|-- Socket.h
|	|-- sockutil.cpp		# 系统网络相关API的统一封装
|	|-- sockutil.h
|	|-- TcpClient.cpp		# TCP客户端封装，派生该类可以很容易实现客户端程序
|	|-- TcpClient.h
|	|-- TcpLimitedSession.h 	# 派生于TcpSession，该模板类可以全局限制会话数量
|	|-- TcpServer.h			# TCP服务器模板类，可以很容易就实现一个高性能私有协议服务器
|	|-- TcpSession.h 		# TCP服务私有协议实现会话基类，用于处理TCP长连接数据及响应
|
|-- Poller				# 主线程事件轮询模块
|	|-- EventPoller.cpp		# 主线程，所有网络事件由此线程轮询并触发
|	|-- EventPoller.h
|	|-- Pipe.cpp			# 管道的对象封装
|	|-- Pipe.h
|	|-- PipeWrap.cpp		# 管道的包装，windows下由socket模拟
|	|-- SelectWrap.cpp		# select 模型的简单包装 
|	|-- SelectWrap.h
|	|-- Timer.cpp			# 在主线程触发的定时器
|	|-- Timer.h
|
|-- Thread				# 线程模块
|	|-- AsyncTaskThread.cpp		# 后台异步任务线程，可以提交一个可定时重复的任务后台执行
|	|-- AsyncTaskThread.h
|	|-- rwmutex.h			# 读写锁，实验性质的
|	|-- semaphore.h			# 信号量，由条件变量实现
|	|-- spin_mutex.h		# 自旋锁，在低延时临界区适用，单核/低性能设备慎用
|	|-- TaskQueue.h			# functional的任务列队
|	|-- threadgroup.h		# 线程组，移植自boost
|	|-- ThreadPool.h		# 线程池，可以输入functional任务至后台线程执行
|	|-- WorkThreadPool.cpp		# 获取一个可用的线程池(可以加入线程负载均衡分配算法)
|	|-- WorkThreadPool.h
|
|-- Util				# 工具模块
	|-- File.cpp			# 文件/目录操作模块
	|-- File.h
	|-- function_traits.h		# 函数、lambda转functional
	|-- logger.h			# 日志模块
	|-- MD5.cpp			# md5加密模块
	|-- MD5.h
	|-- mini.h			# ini配置文件读写模块，支持unix/windows格式的回车符
	|-- NoticeCenter.h		# 消息广播器，可以广播传递任意个数任意类型参数
	|-- onceToken.h			# 使用RAII模式实现，可以在对象构造和析构时执行一段代码
	|-- ResourcePool.h		# 基于智能指针实现的一个循环池，不需要手动回收对象
	|-- RingBuffer.h		# 环形缓冲，可以自适应大小，适用于GOP缓存等
	|-- SqlConnection.cpp		# mysql客户端
	|-- SqlConnection.h
	|-- SqlPool.h			# mysql连接池，以及简单易用的sql语句生成工具
	|-- SSLBox.cpp			# openssl的黑盒封装，屏蔽了ssl握手细节，支持多线程
	|-- SSLBox.h
	|-- TimeTicker.h		# 计时器，可以用于统计函数执行时间
	|-- util.cpp			# 其他一些工具代码，适配了多种系统
	|-- util.h
	|-- uv_errno.cpp		# 提取自libuv的错误代码系统，主要是为了兼容windows
	|-- uv_errno.h
	
``` 


