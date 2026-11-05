/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <csignal>
#include <iostream>
#include "Util/logger.h"
#include "Poller/EventPoller.h"
#include "Poller/Pipe.h"
#include "Poller/PipeWrap.h"
#include "Util/util.h"
#if defined(_WIN32)
#include <windows.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>
#endif
using namespace std;
using namespace toolkit;

#if defined(_WIN32)
namespace {

constexpr size_t kProducerCount = 4;
constexpr size_t kTasksPerProducer = 25000;
constexpr size_t kTaskCount = kProducerCount * kTasksPerProducer;

bool noPipeSocketFiles() {
    wchar_t temp[MAX_PATH];
    auto length = GetTempPathW(MAX_PATH, temp);
    if (!length || length >= MAX_PATH) {
        ErrorL << "Could not get temporary directory: " << GetLastError();
        return false;
    }
    wstring pattern(temp, length);
    if (pattern.back() != L'\\' && pattern.back() != L'/') {
        pattern += L'\\';
    }
    pattern += L"zltk_pipe_" + to_wstring(GetCurrentProcessId()) + L"_*.sock";
    WIN32_FIND_DATAW data;
    auto search = FindFirstFileW(pattern.c_str(), &data);
    if (search != INVALID_HANDLE_VALUE) {
        FindClose(search);
        ErrorL << "PipeWrap left a socket file in the temporary directory";
        return false;
    }
    auto error = GetLastError();
    if (error != ERROR_FILE_NOT_FOUND) {
        ErrorL << "Could not check for socket files: " << error;
        return false;
    }
    return true;
}

bool testPipePathCleanup() {
    if (!noPipeSocketFiles()) {
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        {
            PipeWrap pipe;
            pipe.reOpen();
        }
        if (!noPipeSocketFiles()) {
            return false;
        }
    }
    return true;
}

struct WakeupTestState {
    mutex mtx;
    condition_variable cv;
    bool gate_entered = false;
    bool gate_released = false;
    bool barrier_reached = false;
    bool producer_failed = false;
    atomic<size_t> submitted{0};
    atomic<size_t> executed{0};
    atomic<size_t> posting_done{0};
    atomic<size_t> producers_returned{0};
    atomic<long long> max_async_us{0};
    // Only the poller thread updates this vector; the main thread reads it
    // after the barrier callback has published completion under mtx.
    vector<unsigned int> seen = vector<unsigned int>(kTaskCount, 0);
};

int testPollerWakeup() {
    EventPollerPool::setPoolSize(1);
    EventPollerPool::enableCpuAffinity(false);
    auto poller = EventPollerPool::Instance().getFirstPoller();
    auto state = make_shared<WakeupTestState>();

    // Stop the consumer temporarily so producers can fill the wakeup socket.
    poller->async([state]() {
        unique_lock<mutex> lock(state->mtx);
        state->gate_entered = true;
        state->cv.notify_all();
        state->cv.wait_for(lock, chrono::seconds(5), [&]() { return state->gate_released; });
    }, false);
    {
        unique_lock<mutex> lock(state->mtx);
        if (!state->cv.wait_for(lock, chrono::seconds(10), [&]() { return state->gate_entered; })) {
            ErrorL << "Poller did not start the gate task";
            std::_Exit(1);
        }
    }

    vector<thread> producers;
    producers.reserve(kProducerCount);
    try {
        for (size_t producer = 0; producer < kProducerCount; ++producer) {
            producers.emplace_back([poller, state, producer]() {
                try {
                    for (size_t i = 0; i < kTasksPerProducer; ++i) {
                        const size_t id = producer * kTasksPerProducer + i;
                        const auto start = chrono::steady_clock::now();
                        poller->async([state, id]() {
                            ++state->seen[id];
                            ++state->executed;
                        }, false);
                        const auto elapsed = chrono::duration_cast<chrono::microseconds>(chrono::steady_clock::now() - start).count();
                        auto maximum = state->max_async_us.load();
                        while (maximum < elapsed && !state->max_async_us.compare_exchange_weak(maximum, elapsed)) {}
                        ++state->submitted;
                    }
                    // This barrier is enqueued only after every producer has
                    // finished posting, and therefore follows every test task.
                    if (++state->posting_done == kProducerCount) {
                        poller->async([state]() {
                            lock_guard<mutex> lock(state->mtx);
                            state->barrier_reached = true;
                            state->cv.notify_all();
                        }, false);
                    }
                } catch (...) {
                    lock_guard<mutex> lock(state->mtx);
                    state->producer_failed = true;
                    state->cv.notify_all();
                }
                {
                    lock_guard<mutex> lock(state->mtx);
                    ++state->producers_returned;
                    state->cv.notify_all();
                }
            });
        }
    } catch (...) {
        ErrorL << "Could not start producer threads";
        std::_Exit(1);
    }

    this_thread::sleep_for(chrono::milliseconds(500));
    size_t submitted_while_paused;
    {
        lock_guard<mutex> lock(state->mtx);
        submitted_while_paused = state->submitted.load();
        state->gate_released = true;
        state->cv.notify_all();
    }

    {
        unique_lock<mutex> lock(state->mtx);
        const bool finished = state->cv.wait_for(lock, chrono::seconds(60), [&]() {
            return state->producer_failed ||
                (state->barrier_reached && state->producers_returned.load() == kProducerCount);
        });
        if (!finished || state->producer_failed) {
            ErrorL << "Poller wakeup test failed or timed out: submitted=" << state->submitted.load()
                   << ", executed=" << state->executed.load() << ", producers returned=" << state->producers_returned.load();
            // A producer could be stuck in async(); joining it or destroying
            // the poller would otherwise hang the test indefinitely.
            std::_Exit(1);
        }
    }
    for (auto &producer : producers) {
        producer.join();
    }
    size_t missing = 0;
    size_t duplicated = 0;
    for (auto count : state->seen) {
        missing += count == 0;
        duplicated += count > 1;
    }
    InfoL << "Poller wakeup: submitted=" << state->submitted.load() << ", executed=" << state->executed.load()
          << ", submitted while paused=" << submitted_while_paused
          << ", max async latency=" << state->max_async_us.load() << " us"
          << ", missing=" << missing << ", duplicated=" << duplicated;
    return state->submitted.load() == kTaskCount && state->executed.load() == kTaskCount && !missing && !duplicated ? 0 : 1;
}

} // namespace
#endif

int main() {
    //设置日志  [AUTO-TRANSLATED:50372045]
    // Set up logging
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
#if defined(_WIN32)
    if (!testPipePathCleanup()) {
        return 1;
    }
    return testPollerWakeup();
#else
    //获取父进程的PID  [AUTO-TRANSLATED:0a35f39a]
    // Get the parent process's PID
    auto parentPid = getpid();
    InfoL << "parent pid:" << parentPid << endl;

    //定义一个管道，lambada类型的参数是管道收到数据的回调  [AUTO-TRANSLATED:d5eeb28a]
    // Define a pipe, with a lambda type parameter as the callback for when data is received
    Pipe pipe([](int size,const char *buf) {
        //该管道有数据可读了  [AUTO-TRANSLATED:e542256f]
        // The pipe has data available for reading
        InfoL << getpid() << " recv:" << buf;
    });

    //创建子进程  [AUTO-TRANSLATED:8d5746fc]
    // Create a child process
    auto pid = fork();

    if (pid == 0) {
        //子进程  [AUTO-TRANSLATED:3f793797]
        // Child process
        int i = 10;
        while (i--) {
            //在子进程每隔一秒把数据写入管道，共计发送10次  [AUTO-TRANSLATED:5a340f4b]
            // In the child process, write data to the pipe every second, for a total of 10 times
            sleep(1);
            string msg = StrPrinter << "message " << i << " form subprocess:" << getpid();
            DebugL << "子进程发送:" << msg << endl;
            pipe.send(msg.data(), msg.size());
        }
        DebugL << "子进程退出" << endl;
    } else {
        //父进程设置退出信号处理函数  [AUTO-TRANSLATED:b2a0b432]
        // Parent process sets up exit signal handling function
        static semaphore sem;
        signal(SIGINT, [](int) { sem.post(); });// 设置退出信号
        sem.wait();

        InfoL << "父进程退出" << endl;
    }
#endif // defined(_WIN32)

    return 0;
}
