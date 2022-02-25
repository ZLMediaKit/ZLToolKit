/*
* @file_name: non_lock_queue.h
* @date: 2021/10/23
* @author: oaho
* Copyright @ hz oaho, All rights reserved.
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
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO basic_event SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* 线程安全，无锁队列(CAS)，线程数多的时候，谨慎使用。吞吐量会变低。
* 释放时线程不安全，通常用做全局变量或者变量的作用域要比操作它的线程生命周期更长，但释放的时候要保证只有一个线程在释放
 */
#ifndef _OAHO_NON_LOCK_QUEUE_H
#define _OAHO_NON_LOCK_QUEUE_H
#include <atomic>
#include <memory>
#include <type_traits>
template<typename T>
struct node {
  T val;
  std::atomic<node<T>*> _next;
  template<typename...Args> node(Args&&...args) :val(std::forward<Args>(args)...), _next(nullptr){
  }
};


template<typename T, typename allocator = std::allocator<T>>
class non_lock_queue {
public:
  using value_type = T;
  using this_type = non_lock_queue<T, allocator>;
  using size_type = size_t;
  using pointer = value_type*;
  using const_poiner = const pointer;
  using rebind_alloc_t = typename std::allocator_traits<allocator>::template rebind_alloc<node<T>>;
private:
  using Node_t = node<T>;
public:
  non_lock_queue() :_head_(nullptr), _tail_(nullptr),_size(0) {
      _tail_ = alloc_new();
      _head_ = _tail_.load(std::memory_order_relaxed);
  }
  ~non_lock_queue() {
    clear();
  }


  inline std::unique_ptr<T> pop_front() {
    Node_t* old_head = nullptr, *front = nullptr;
    do{
      //尝试拿到链表头
      old_head = _head_.load();
      front = old_head->_next.load();
      if(!front)return nullptr;
    }while(!old_head->_next.compare_exchange_weak(front, front->_next.load()));
    std::unique_ptr<T> front_ptr(new T(std::move(front->val)));
    release(front);
    return std::move(front_ptr);
  }


  inline const std::atomic<ssize_t>& size() const { return _size; }

  template<typename...Args>
  void emplace_back(Args&&...args) {
    //申请一个新的元素
    Node_t* new_one = alloc_new(std::forward<Args>(args)...);
    Node_t* old_back = nullptr;
    Node_t* back = nullptr;
    do{
      //尝试拿到最后一个元素
      old_back = _tail_.load();
      back = old_back->_next.load();
      //失败重来，直到当前节点的下一个元素是空指针
    }while(!old_back->_next.compare_exchange_weak(back, new_one));
    _tail_.compare_exchange_weak(old_back, new_one, std::memory_order_relaxed, std::memory_order_relaxed);
    ++_size;
  }
private:
  template<typename...Args>
  inline Node_t* alloc_new(Args&&...args) {
    Node_t* new_node = std::allocator_traits<rebind_alloc_t>::allocate(rebind_alloc, 1);
    std::allocator_traits<rebind_alloc_t>::construct(rebind_alloc, new_node, std::forward<Args>(args)...);
    return new_node;
  }

  inline void release(Node_t* n){
    std::allocator_traits<rebind_alloc_t>::destroy(rebind_alloc, n);
    std::allocator_traits<rebind_alloc_t>::deallocate(rebind_alloc, n, 1);
  }
  /* 释放时，线程不安全 */
  inline void clear() {

  }
private:
  std::atomic<Node_t*> _head_;
  std::atomic<Node_t*> _tail_;
  std::atomic<ssize_t> _size;
  rebind_alloc_t rebind_alloc;
};
#endif