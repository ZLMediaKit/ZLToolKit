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
#ifndef ZLTOOLKIT_LIST_H
#define ZLTOOLKIT_LIST_H

#include <type_traits>

using namespace std;

namespace toolkit {


template<typename T>
class List;

template<typename T>
class ListNode
{
public:
    friend class List<T>;
    ~ListNode(){}

    ListNode(T &&data):_data(std::forward<T>(data)){}
    ListNode(const T &data):_data(data){}

    template <class... Args>
    ListNode(Args&&... args):_data(std::forward<Args>(args)...){}
private:
    T _data;
    ListNode *next = nullptr;
};


template<typename T>
class List {
public:
    typedef ListNode<T> NodeType;
    List(){}
    List(List &&that){
        swap(that);
    }
    ~List(){
        clear();
    }
    void clear(){
        auto ptr = _front;
        auto last = ptr;
        while(ptr){
            last = ptr;
            ptr = ptr->next;
            delete last;
        }
        _size = 0;
        _front = nullptr;
        _back = nullptr;
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

    bool empty() const{
        return _size == 0;
    }
    template <class... Args>
    void emplace_front(Args&&... args){
        NodeType *node = new NodeType(std::forward<Args>(args)...);
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

    template <class...Args>
    void emplace_back(Args&&... args){
        NodeType *node = new NodeType(std::forward<Args>(args)...);
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

    T &back() const{
        return _back->_data;
    }

    T &operator[](uint64_t pos){
        NodeType *front = _front ;
        while(pos--){
            front = front->next;
        }
        return front->_data;
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

    void swap(List &other){
        NodeType *tmp_node;

        tmp_node = _front;
        _front = other._front;
        other._front = tmp_node;

        tmp_node = _back;
        _back = other._back;
        other._back = tmp_node;

        uint64_t tmp_size = _size;
        _size = other._size;
        other._size = tmp_size;
    }

    void append(List<T> &other){
        if(other.empty()){
            return;
        }
        if(_back){
            _back->next = other._front;
            _back = other._back;
        }else{
            _front = other._front;
            _back = other._back;
        }
        _size += other._size;

        other._front = other._back = nullptr;
        other._size = 0;
    }
private:
    NodeType *_front = nullptr;
    NodeType *_back = nullptr;
    uint64_t _size = 0;
};



} /* namespace toolkit */

#endif //ZLTOOLKIT_LIST_H
