/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xiongziliang/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
