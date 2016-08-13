/*
 * Redox - A modern, asynchronous, and wicked fast C++11 client for Redis
 *
 *    https://github.com/hmartiro/redox
 *
 * Copyright 2015 - Hayk Martirosyan <hayk.mart at gmail dot com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <vector>
#include <set>
#include <unordered_set>

#include "command.hpp"
#include "client.hpp"

using namespace std;

namespace redox {

template<class ReplyT>
Command<ReplyT>::Command(Redox *rdx, long id, const vector<string> &cmd,
		const function<void(Command<ReplyT> &)> &callback, double repeat,
		double after, bool free_memory) :
		rdx_(rdx), id_(id), cmd_(cmd), repeat_(repeat), after_(after), free_memory_(
				free_memory), callback_(callback), last_error_() {
	timer_guard_.lock();
}

template<class ReplyT> void Command<ReplyT>::wait() {
	unique_lock<mutex> lk(waiter_lock_);
	waiter_.wait(lk, [this]() {return waiting_done_.load();});
	waiting_done_ = {false};
}

template<class ReplyT> void Command<ReplyT>::processReply(redisReply *r) {

	last_error_.clear();
	reply_obj_ = r;

	if (reply_obj_ == nullptr) {
		reply_status_ = ERROR_REPLY;
		last_error_ = "Received null redisReply* from hiredis.";
		ErrorL << last_error_;
		Redox::disconnectedCallback(rdx_->ctx_, REDIS_ERR);

	} else {
		lock_guard<mutex> lg(reply_guard_);
		parseReplyObject();
	}

	invoke();

	pending_--;

	{
		unique_lock<mutex> lk(waiter_lock_);
		waiting_done_ = true;
	}
	waiter_.notify_all();

	// Always free the reply object for repeating commands
	if (repeat_ > 0) {
		freeReply();

	} else {

		// User calls .free()
		if (!free_memory_)
			return;

		// Free non-repeating commands automatically
		// once we receive expected replies
		if (pending_ == 0)
			free();
	}
}

// This is the only method in Command that has
// access to private members of Redox
template<class ReplyT> void Command<ReplyT>::free() {

	lock_guard<mutex> lg(rdx_->free_queue_guard_);
	rdx_->commands_to_free_.push(id_);
	ev_async_send(rdx_->evloop_, &rdx_->watcher_free_);
}

template<class ReplyT> void Command<ReplyT>::freeReply() {

	if (reply_obj_ == nullptr)
		return;

	freeReplyObject(reply_obj_);
	reply_obj_ = nullptr;
}

/**
 * Create a copy of the reply and return it. Use a guard
 * to make sure we don't return a reply while it is being
 * modified.
 */
template<class ReplyT> ReplyT &Command<ReplyT>::reply() {
	lock_guard<mutex> lg(reply_guard_);
	if (!ok()) {
		WarnL << cmd() << ": Accessing reply value while status != OK.";
	}
	return reply_val_;
}

template<class ReplyT> string Command<ReplyT>::cmd() const {
	return rdx_->vecToStr(cmd_);
}

template<class ReplyT> bool Command<ReplyT>::isExpectedReply(int type) {

	if (reply_obj_->type == type) {
		reply_status_ = OK_REPLY;
		return true;
	}

	if (checkErrorReply() || checkNilReply())
		return false;

	stringstream errorMessage;
	errorMessage << "Received reply of type " << reply_obj_->type
			<< ", expected type " << type << ".";
	last_error_ = errorMessage.str();
	ErrorL << cmd() << ": " << last_error_;
	reply_status_ = WRONG_TYPE;
	return false;
}

template<class ReplyT> bool Command<ReplyT>::isExpectedReply(int typeA,
		int typeB) {

	if ((reply_obj_->type == typeA) || (reply_obj_->type == typeB)) {
		reply_status_ = OK_REPLY;
		return true;
	}

	if (checkErrorReply() || checkNilReply())
		return false;

	stringstream errorMessage;
	errorMessage << "Received reply of type " << reply_obj_->type
			<< ", expected type " << typeA << " or " << typeB << ".";
	last_error_ = errorMessage.str();
	ErrorL << cmd() << ": " << last_error_;
	reply_status_ = WRONG_TYPE;
	return false;
}

template<class ReplyT> bool Command<ReplyT>::checkErrorReply() {

	if (reply_obj_->type == REDIS_REPLY_ERROR) {
		if (reply_obj_->str != 0) {
			last_error_ = reply_obj_->str;
		}

		ErrorL << cmd() << ": " << last_error_;
		reply_status_ = ERROR_REPLY;
		return true;
	}
	return false;
}

template<class ReplyT> bool Command<ReplyT>::checkNilReply() {

	if (reply_obj_->type == REDIS_REPLY_NIL) {
		WarnL << cmd() << ": Nil reply.";
		reply_status_ = NIL_REPLY;
		return true;
	}
	return false;
}

// ----------------------------------------------------------------------------
// Specializations of parseReplyObject for all expected return types
// ----------------------------------------------------------------------------

template<> void Command<redisReply *>::parseReplyObject() {
	if (!checkErrorReply())
		reply_status_ = OK_REPLY;
	reply_val_ = reply_obj_;
}

template<> void Command<string>::parseReplyObject() {
	if (!isExpectedReply(REDIS_REPLY_STRING, REDIS_REPLY_STATUS))
		return;
	reply_val_ = {reply_obj_->str, static_cast<size_t>(reply_obj_->len)};
}

template<> void Command<char *>::parseReplyObject() {
	if (!isExpectedReply(REDIS_REPLY_STRING, REDIS_REPLY_STATUS))
		return;
	reply_val_ = reply_obj_->str;
}

template<> void Command<int>::parseReplyObject() {

	if (!isExpectedReply(REDIS_REPLY_INTEGER))
		return;
	reply_val_ = (int) reply_obj_->integer;
}

template<> void Command<long long int>::parseReplyObject() {

	if (!isExpectedReply(REDIS_REPLY_INTEGER))
		return;
	reply_val_ = reply_obj_->integer;
}

template<> void Command<nullptr_t>::parseReplyObject() {

	if (!isExpectedReply(REDIS_REPLY_NIL))
		return;
	reply_val_ = nullptr;
}

template<> void Command<vector<string>>::parseReplyObject() {

	if (!isExpectedReply(REDIS_REPLY_ARRAY))
		return;

	for (size_t i = 0; i < reply_obj_->elements; i++) {
		redisReply *r = *(reply_obj_->element + i);
		reply_val_.emplace_back(r->str, r->len);
	}
}

template<> void Command<unordered_set<string>>::parseReplyObject() {

	if (!isExpectedReply(REDIS_REPLY_ARRAY))
		return;

	for (size_t i = 0; i < reply_obj_->elements; i++) {
		redisReply *r = *(reply_obj_->element + i);
		reply_val_.emplace(r->str, r->len);
	}
}

template<> void Command<set<string>>::parseReplyObject() {

	if (!isExpectedReply(REDIS_REPLY_ARRAY))
		return;

	for (size_t i = 0; i < reply_obj_->elements; i++) {
		redisReply *r = *(reply_obj_->element + i);
		reply_val_.emplace(r->str, r->len);
	}
}

// Explicit template instantiation for available types, so that the generated
// library contains them and we can keep the method definitions out of the
// header file.
template class Command<redisReply *> ;
template class Command<string> ;
template class Command<char *> ;
template class Command<int> ;
template class Command<long long int> ;
template class Command<nullptr_t> ;
template class Command<vector<string>> ;
template class Command<set<string>> ;
template class Command<unordered_set<string>> ;

} // End namespace redox
