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

#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include <hiredis/adapters/libev.h>
#include <hiredis/async.h>

#include <Util/logger.h>
using namespace ZL::Util;

namespace redox {

class Redox;

/**
 * The Command class represents a single command string to be sent to
 * a Redis server, for both synchronous and asynchronous usage. It manages
 * all of the state relevant to a single command string. A Command can also
 * represent a deferred or looping command, in which case the success or
 * error callbacks are invoked more than once.
 */
template<class ReplyT> class Command {

public:
	// Reply codes
	static const int NO_REPLY = -1;   // No reply yet
	static const int OK_REPLY = 0;    // Successful reply of the expected type
	static const int NIL_REPLY = 1;   // Got a nil reply
	static const int ERROR_REPLY = 2; // Got an error reply
	static const int SEND_ERROR = 3;  // Could not send to server
	static const int WRONG_TYPE = 4; // Got reply, but it was not the expected type
	static const int TIMEOUT = 5;     // No reply, timed out

	/**
	 * Returns the reply status of this command.
	 */
	int status() const {
		return reply_status_;
	}

	std::string lastError() const {
		return last_error_;
	}

	/**
	 * Returns true if this command got a successful reply.
	 */
	bool ok() const {
		return reply_status_ == OK_REPLY;
	}

	/**
	 * Returns the reply value, if the reply was successful (ok() == true).
	 */
	ReplyT &reply();

	/**
	 * Tells the event loop to free memory for this command. The user is
	 * responsible for calling this on synchronous or looping commands,
	 * AKA when free_memory_ = false.
	 */
	void free();

	/**
	 * This method returns once this command's callback has been invoked
	 * (or would have been invoked if there is none) since the last call
	 * to wait(). If it is the first call, then returns once the callback
	 * is invoked for the first time.
	 */
	void wait();

	/**
	 * Returns the command string represented by this object.
	 */
	std::string cmd() const;

	// Allow public access to constructed data
	Redox * const rdx_;
	const long id_;
	const std::vector<std::string> cmd_;
	const double repeat_;
	const double after_;
	const bool free_memory_;

private:
	Command(Redox *rdx, long id, const std::vector<std::string> &cmd,
			const std::function<void(Command<ReplyT> &)> &callback,
			double repeat, double after,
			bool free_memory);

	// Handles a new reply from the server
	void processReply(redisReply *r);

	// Invoke a user callback from the reply object. This method is specialized
	// for each ReplyT of Command.
	void parseReplyObject();

	// Directly invoke the user callback if it exists
	void invoke() {
		if (callback_)
			callback_(*this);
	}

	bool checkErrorReply();bool checkNilReply();bool isExpectedReply(int type);bool isExpectedReply(
			int typeA, int typeB);

	// If needed, free the redisReply
	void freeReply();

	// The last server reply
	redisReply *reply_obj_ = nullptr;

	// User callback
	const std::function<void(Command<ReplyT> &)> callback_;

	// Place to store the reply value and status.
	ReplyT reply_val_;
	int reply_status_;
	std::string last_error_;

	// How many messages sent to server but not received reply
	std::atomic_int pending_ = { 0 };

	// Whether a repeating or delayed command is canceled
	std::atomic_bool canceled_ = { false };

	// libev timer watcher
	ev_timer timer_;
	std::mutex timer_guard_;

	// Access the reply value only when not being changed
	std::mutex reply_guard_;

	// For synchronous use
	std::condition_variable waiter_;
	std::mutex waiter_lock_;
	std::atomic_bool waiting_done_ = { false };

	// Explicitly delete copy constructor and assignment operator,
	// Command objects should never be copied because they hold
	// state with a network resource.
	Command(const Command &) = delete;
	Command &operator=(const Command &) = delete;

	friend class Redox;
};

} // End namespace redis
