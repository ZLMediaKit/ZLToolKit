#include <Thread/non_lock_queue.hpp>
#include <Util/List.h>
#include <Util/TimeTicker.h>
#include <Util/logger.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>
using namespace toolkit;
std::mutex mtx;
std::atomic<size_t> counter;
class object{
public:
  object(){
  }
  object(const object&) = default;
  object(object&&) = default;
  ~object(){
  }
};
List<object> que;
non_lock_queue<object> non_que;
class thread_group{
public:
  thread_group(){}
  ~thread_group(){
    join();
  }

  template<typename Func, typename...Args>
  void start(size_t num, Func f, Args&&... args){
    for(size_t i = 0; i < num;i++){
      group.template emplace_back(new std::thread(f, std::forward<Args>(args)...));
    }
  }

  void join(){
    std::for_each(group.begin(), group.end(), [](std::thread* t){
      if(t && t->joinable()){
        t->join();
        delete t;
      }
    });
    group.clear();
  }
private:
  std::unique_ptr<int> non_copy;
  std::vector<std::thread*> group;
};
int main()
{
  Logger::Instance().add(std::make_shared<ConsoleChannel>());
  Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
  Ticker ticker;
  constexpr const int size = 1000 * 10000;
  //防止cpu指令重排
  counter.store(0, std::memory_order_seq_cst);
  //由于CPU Cache的影响，特意把无锁队列放在前面测试
  auto threads_num = std::thread::hardware_concurrency() - 5;
  DebugL << "共开启" << threads_num << "个线程测试";
  auto non_lock_func = [](){
    for(int i = 0;i < size;i++){
      non_que.emplace_back(object{});
      //non_que.pop_front();
    }
  };
  ticker.resetTime();
  DebugL << "无锁队列开始存放任务";
  {
    thread_group group;
    group.start(threads_num, non_lock_func);
  }
  auto duration = ticker.elapsedTime();
  DebugL << "无锁队列一共耗费" << duration * 1.0 / 1000 << "s";
  auto func = [](){
    for(int i = 0;i < size;i++){
      std::lock_guard<std::mutex> lmtx(mtx);
      que.emplace_back(object{});
      //que.pop_front();
    }
  };

  ticker.resetTime();
  DebugL << "阻塞队列开始存放任务";
  {
    thread_group group;
    group.start(threads_num, func);
  }
  duration = ticker.elapsedTime();
  DebugL << "阻塞队列一共耗费" << duration * 1.0 / 1000 << "s";
  DebugL << "程序退出";
  return 0;
}