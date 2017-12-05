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
#ifndef ZLTOOLKIT_LIST_H
#define ZLTOOLKIT_LIST_H

#include <type_traits>

using namespace std;

namespace ZL {
namespace Thread {


template<typename T>
class List;

template<typename T>
class ListNode
{
public:
    friend class List<T>;
    virtual ~ListNode(){}

    ListNode(T &&data):_data(std::forward<T>(data)){}
    ListNode(const T &data):_data(data){}

    template <class... _Args>
    ListNode(_Args&&... __args):_data(std::forward<_Args>(__args)...){}
private:
    T _data;
    ListNode *next = nullptr;
};


template<typename T>
class List {
public:
    typedef ListNode<T> NodeType;
    List(){}
    virtual ~List(){
        auto ptr = _front;
        auto last = ptr;
        while(ptr){
            last = ptr;
            ptr = ptr->next;
            delete last;
        }
    }

    template <typename  FUN>
    void for_each(FUN &&fun){
        auto ptr = _front;
        while(ptr){
            fun(ptr->_data);
            ptr = ptr->next;
        }
    }

    uint64_t size() const{
        return _size;
    }

    template <class... _Args>
    void emplace_front(_Args&&... __args){
        NodeType *node = new NodeType(std::forward<_Args>(__args)...);
        if(!_front){
            _front = node;
            _back = node;
            _size = 1;
        }else{
            node->next = _front;
            _front = node;
            ++_size;
        }
    }

    template <class... _Args>
    void emplace_back(_Args&&... __args){
        NodeType *node = new NodeType(std::forward<_Args>(__args)...);
        if(!_back){
            _back = node;
            _front = node;
            _size = 1;
        }else{
            _back->next = node;
            _back = node;
            ++_size;
        }
    }

    T &front() const{
        return _front->_data;
    }

    void pop_front(){
        if(!_front){
            return;
        }
        auto ptr = _front;
        _front = _front->next;
        delete ptr;
        if(!_front){
            _back = nullptr;
        }
        --_size;
    }

private:
    NodeType *_front = nullptr;
    NodeType *_back = nullptr;
    uint64_t _size = 0;
};




} /* namespace Thread */
} /* namespace ZL */

#endif //ZLTOOLKIT_LIST_H
