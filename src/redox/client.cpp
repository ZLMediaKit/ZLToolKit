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

#include <signal.h>
#include <algorithm>
#include "client.hpp"

using namespace std;

namespace {

template<typename tev, typename tcb>
void redox_ev_async_init(tev ev, tcb cb) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
	ev_async_init(ev, cb);
#pragma GCC diagnostic pop
}

template<typename ttimer, typename tcb, typename tafter, typename trepeat>
void redox_ev_timer_init(ttimer timer, tcb cb, tafter after, trepeat repeat) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
	ev_timer_init(timer, cb, after, repeat);
#pragma GCC diagnostic pop
}

} // anonymous

namespace redox {

Redox::Redox() :
		evloop_(nullptr) {
}

bool Redox::connect(const string &host, const int port,
		function<void(int)> connection_callback) {

	host_ = host;
	port_ = port;
	user_connection_callback_ = connection_callback;

	if (!initEv())
		return false;

	// Connect over TCP
	ctx_ = redisAsyncConnect(host.c_str(), port);

	if (!initHiredis())
		return false;

	event_loop_thread_ = thread([this] {runEventLoop();});

	// Block until connected and running the event loop, or until
	// a connection error happens and the event loop exits
	{
		unique_lock<mutex> ul(running_lock_);
		running_waiter_.wait(ul, [this] {
			lock_guard<mutex> lg(connect_lock_);
			return running_ || connect_state_ == CONNECT_ERROR;
		});
	}

	// Return if succeeded
	return getConnectState() == CONNECTED;
}

bool Redox::connectUnix(const string &path,
		function<void(int)> connection_callback) {

	path_ = path;
	user_connection_callback_ = connection_callback;

	if (!initEv())
		return false;

	// Connect over unix sockets
	ctx_ = redisAsyncConnectUnix(path.c_str());

	if (!initHiredis())
		return false;

	event_loop_thread_ = thread([this] {runEventLoop();});

	// Block until connected and running the event loop, or until
	// a connection error happens and the event loop exits
	{
		unique_lock<mutex> ul(running_lock_);
		running_waiter_.wait(ul, [this] {
			lock_guard<mutex> lg(connect_lock_);
			return running_ || connect_state_ == CONNECT_ERROR;
		});
	}

	// Return if succeeded
	return getConnectState() == CONNECTED;
}

void Redox::disconnect() {
	stop();
	wait();
}

void Redox::stop() {
	to_exit_ = true;
	DebugL << "stop() called, breaking event loop";
	ev_async_send(evloop_, &watcher_stop_);
}

void Redox::wait() {
	unique_lock<mutex> ul(exit_lock_);
	exit_waiter_.wait(ul, [this] {return exited_;});
}

Redox::~Redox() {

	// Bring down the event loop
	if (getRunning()) {
		stop();
	}

	if (event_loop_thread_.joinable())
		event_loop_thread_.join();

	if (evloop_ != nullptr)
		ev_loop_destroy(evloop_);
}

void Redox::connectedCallback(const redisAsyncContext *ctx, int status) {
	Redox *rdx = (Redox *) ctx->data;

	if (status != REDIS_OK) {
		FatalL << "Could not connect to Redis: " << ctx->errstr;
		FatalL << "Status: " << status;
		rdx->setConnectState(CONNECT_ERROR);

	} else {
		InfoL << "Connected to Redis.";
		// Disable hiredis automatically freeing reply objects
		ctx->c.reader->fn->freeObject = [](void *reply) {};
		rdx->setConnectState(CONNECTED);
	}

	if (rdx->user_connection_callback_) {
		rdx->user_connection_callback_(rdx->getConnectState());
	}
}

void Redox::disconnectedCallback(const redisAsyncContext *ctx, int status) {

	Redox *rdx = (Redox *) ctx->data;

	if (status != REDIS_OK) {
		ErrorL << "Disconnected from Redis on error: " << ctx->errstr;
		rdx->setConnectState(DISCONNECT_ERROR);
	} else {
		InfoL << "Disconnected from Redis as planned.";
		rdx->setConnectState(DISCONNECTED);
	}

	rdx->stop();
	if (rdx->user_connection_callback_) {
		rdx->user_connection_callback_(rdx->getConnectState());
	}
}

bool Redox::initEv() {
	signal(SIGPIPE, SIG_IGN);
	evloop_ = ev_loop_new(EVFLAG_AUTO);
	if (evloop_ == nullptr) {
		FatalL << "Could not create a libev event loop.";
		setConnectState(INIT_ERROR);
		return false;
	}
	ev_set_userdata(evloop_, (void *) this); // Back-reference
	return true;
}

bool Redox::initHiredis() {

	ctx_->data = (void *) this; // Back-reference

	if (ctx_->err) {
		FatalL << "Could not create a hiredis context: " << ctx_->errstr;
		setConnectState(INIT_ERROR);
		return false;
	}

	// Attach event loop to hiredis
	if (redisLibevAttach(evloop_, ctx_) != REDIS_OK) {
		FatalL << "Could not attach libev event loop to hiredis.";
		setConnectState(INIT_ERROR);
		return false;
	}

	// Set the callbacks to be invoked on server connection/disconnection
	if (redisAsyncSetConnectCallback(ctx_, Redox::connectedCallback) != REDIS_OK) {
		FatalL << "Could not attach connect callback to hiredis.";
		setConnectState(INIT_ERROR);
		return false;
	}

	if (redisAsyncSetDisconnectCallback(ctx_,
			Redox::disconnectedCallback) != REDIS_OK) {
		FatalL << "Could not attach disconnect callback to hiredis.";
		setConnectState(INIT_ERROR);
		return false;
	}

	return true;
}

void Redox::noWait(bool state) {
	if (state)
		InfoL << "No-wait mode enabled.";
	else
		InfoL << "No-wait mode disabled.";
	nowait_ = state;
}

void breakEventLoop(struct ev_loop *loop, ev_async *async, int revents) {
	ev_break(loop, EVBREAK_ALL);
}

int Redox::getConnectState() {
	lock_guard<mutex> lk(connect_lock_);
	return connect_state_;
}

void Redox::setConnectState(int connect_state) {
	{
		lock_guard<mutex> lk(connect_lock_);
		connect_state_ = connect_state;
	}
	connect_waiter_.notify_all();
}

int Redox::getRunning() {
	lock_guard<mutex> lg(running_lock_);
	return running_;
}
void Redox::setRunning(bool running) {
	{
		lock_guard<mutex> lg(running_lock_);
		running_ = running;
	}
	running_waiter_.notify_one();
}

int Redox::getExited() {
	lock_guard<mutex> lg(exit_lock_);
	return exited_;
}
void Redox::setExited(bool exited) {
	{
		lock_guard<mutex> lg(exit_lock_);
		exited_ = exited;
	}
	exit_waiter_.notify_one();
}

void Redox::runEventLoop() {

	// Events to connect to Redox
	ev_run(evloop_, EVRUN_ONCE);
	ev_run(evloop_, EVRUN_NOWAIT);

	// Block until connected to Redis, or error
	{
		unique_lock<mutex> ul(connect_lock_);
		connect_waiter_.wait(ul,
				[this] {return connect_state_ != NOT_YET_CONNECTED;});

		// Handle connection error
		if (connect_state_ != CONNECTED) {
			WarnL << "Did not connect, event loop exiting.";
			setExited(true);
			setRunning(false);
			return;
		}
	}

	// Set up asynchronous watcher which we signal every
	// time we add a command
	redox_ev_async_init(&watcher_command_, processQueuedCommands);
	ev_async_start(evloop_, &watcher_command_);

	// Set up an async watcher to break the loop
	redox_ev_async_init(&watcher_stop_, breakEventLoop);
	ev_async_start(evloop_, &watcher_stop_);

	// Set up an async watcher which we signal every time
	// we want a command freed
	redox_ev_async_init(&watcher_free_, freeQueuedCommands);
	ev_async_start(evloop_, &watcher_free_);

	setRunning(true);

	// Run the event loop, using NOWAIT if enabled for maximum
	// throughput by avoiding any sleeping
	while (!to_exit_) {
		if (nowait_) {
			ev_run(evloop_, EVRUN_NOWAIT);
		} else {
			ev_run(evloop_);
		}
	}

	InfoL << "Stop signal detected. Closing down event loop.";

	// Signal event loop to free all commands
	freeAllCommands();

	// Wait to receive server replies for clean hiredis disconnect
	this_thread::sleep_for(chrono::milliseconds(10));
	ev_run(evloop_, EVRUN_NOWAIT);

	if (getConnectState() == CONNECTED) {
		redisAsyncDisconnect(ctx_);
	}

	// Run once more to disconnect
	ev_run(evloop_, EVRUN_NOWAIT);

	long created = commands_created_;
	long deleted = commands_deleted_;
	if (created != deleted) {
		ErrorL << "All commands were not freed! " << deleted << "/" << created;
	}

	// Let go for block_until_stopped method
	setExited(true);
	setRunning(false);

	InfoL << "Event thread exited.";
}

template<class ReplyT> Command<ReplyT> *Redox::findCommand(long id) {

	lock_guard<mutex> lg(command_map_guard_);

	auto &command_map = getCommandMap<ReplyT>();
	auto it = command_map.find(id);
	if (it == command_map.end())
		return nullptr;
	return it->second;
}

template<class ReplyT>
void Redox::commandCallback(redisAsyncContext *ctx, void *r, void *privdata) {

	Redox *rdx = (Redox *) ctx->data;
	long id = (long) privdata;
	redisReply *reply_obj = (redisReply *) r;

	Command<ReplyT> *c = rdx->findCommand<ReplyT>(id);
	if (c == nullptr) {
		freeReplyObject(reply_obj);
		return;
	}

	c->processReply(reply_obj);
}

template<class ReplyT> bool Redox::submitToServer(Command<ReplyT> *c) {

	Redox *rdx = c->rdx_;
	c->pending_++;

	// Construct a char** from the vector
	vector<const char *> argv;
	transform(c->cmd_.begin(), c->cmd_.end(), back_inserter(argv),
			[](const string &s) {return s.c_str();});

	// Construct a size_t* of string lengths from the vector
	vector<size_t> argvlen;
	transform(c->cmd_.begin(), c->cmd_.end(), back_inserter(argvlen),
			[](const string &s) {return s.size();});

	if (redisAsyncCommandArgv(rdx->ctx_, commandCallback<ReplyT>,
			(void *) c->id_, argv.size(), &argv[0], &argvlen[0]) != REDIS_OK) {
		ErrorL << "Could not send \"" << c->cmd() << "\": "
				<< rdx->ctx_->errstr;
		c->reply_status_ = Command<ReplyT>::SEND_ERROR;
		c->invoke();
		return false;
	}

	return true;
}

template<class ReplyT>
void Redox::submitCommandCallback(struct ev_loop *loop, ev_timer *timer,
		int revents) {

	Redox *rdx = (Redox *) ev_userdata(loop);
	long id = (long) timer->data;

	Command<ReplyT> *c = rdx->findCommand<ReplyT>(id);
	if (c == nullptr) {
		ErrorL << "Couldn't find Command " << id
				<< " in command_map (submitCommandCallback).";
		return;
	}

	submitToServer<ReplyT>(c);
}

template<class ReplyT> bool Redox::processQueuedCommand(long id) {

	Command<ReplyT> *c = findCommand<ReplyT>(id);
	if (c == nullptr)
		return false;

	if ((c->repeat_ == 0) && (c->after_ == 0)) {
		submitToServer<ReplyT>(c);

	} else {

		c->timer_.data = (void *) c->id_;
		redox_ev_timer_init(&c->timer_, submitCommandCallback<ReplyT>,
				c->after_, c->repeat_);
		ev_timer_start(evloop_, &c->timer_);

		c->timer_guard_.unlock();
	}

	return true;
}

void Redox::processQueuedCommands(struct ev_loop *loop, ev_async *async,
		int revents) {

	Redox *rdx = (Redox *) ev_userdata(loop);

	lock_guard<mutex> lg(rdx->queue_guard_);

	while (!rdx->command_queue_.empty()) {

		long id = rdx->command_queue_.front();
		rdx->command_queue_.pop();

		if (rdx->processQueuedCommand<redisReply *>(id)) {
		} else if (rdx->processQueuedCommand<string>(id)) {
		} else if (rdx->processQueuedCommand<char *>(id)) {
		} else if (rdx->processQueuedCommand<int>(id)) {
		} else if (rdx->processQueuedCommand<long long int>(id)) {
		} else if (rdx->processQueuedCommand<nullptr_t>(id)) {
		} else if (rdx->processQueuedCommand<vector<string>>(id)) {
		} else if (rdx->processQueuedCommand<std::set<string>>(id)) {
		} else if (rdx->processQueuedCommand<unordered_set<string>>(id)) {
		} else
			throw runtime_error("Command pointer not found in any queue!");
	}
}

void Redox::freeQueuedCommands(struct ev_loop *loop, ev_async *async,
		int revents) {

	Redox *rdx = (Redox *) ev_userdata(loop);

	lock_guard<mutex> lg(rdx->free_queue_guard_);

	while (!rdx->commands_to_free_.empty()) {
		long id = rdx->commands_to_free_.front();
		rdx->commands_to_free_.pop();

		if (rdx->freeQueuedCommand<redisReply *>(id)) {
		} else if (rdx->freeQueuedCommand<string>(id)) {
		} else if (rdx->freeQueuedCommand<char *>(id)) {
		} else if (rdx->freeQueuedCommand<int>(id)) {
		} else if (rdx->freeQueuedCommand<long long int>(id)) {
		} else if (rdx->freeQueuedCommand<nullptr_t>(id)) {
		} else if (rdx->freeQueuedCommand<vector<string>>(id)) {
		} else if (rdx->freeQueuedCommand<std::set<string>>(id)) {
		} else if (rdx->freeQueuedCommand<unordered_set<string>>(id)) {
		} else {
		}
	}
}

template<class ReplyT> bool Redox::freeQueuedCommand(long id) {
	Command<ReplyT> *c = findCommand<ReplyT>(id);
	if (c == nullptr)
		return false;

	c->freeReply();

	// Stop the libev timer if this is a repeating command
	if ((c->repeat_ != 0) || (c->after_ != 0)) {
		lock_guard<mutex> lg(c->timer_guard_);
		ev_timer_stop(c->rdx_->evloop_, &c->timer_);
	}

	deregisterCommand<ReplyT>(c->id_);

	delete c;

	return true;
}

long Redox::freeAllCommands() {
	return freeAllCommandsOfType<redisReply *>()
			+ freeAllCommandsOfType<string>() + freeAllCommandsOfType<char *>()
			+ freeAllCommandsOfType<int>()
			+ freeAllCommandsOfType<long long int>()
			+ freeAllCommandsOfType<nullptr_t>()
			+ freeAllCommandsOfType<vector<string>>()
			+ freeAllCommandsOfType<std::set<string>>()
			+ freeAllCommandsOfType<unordered_set<string>>();
}

template<class ReplyT> long Redox::freeAllCommandsOfType() {

	lock_guard<mutex> lg(free_queue_guard_);
	lock_guard<mutex> lg2(queue_guard_);
	lock_guard<mutex> lg3(command_map_guard_);

	auto &command_map = getCommandMap<ReplyT>();
	long len = command_map.size();

	for (auto &pair : command_map) {
		Command<ReplyT> *c = pair.second;

		c->freeReply();

		// Stop the libev timer if this is a repeating command
		if ((c->repeat_ != 0) || (c->after_ != 0)) {
			lock_guard<mutex> lg3(c->timer_guard_);
			ev_timer_stop(c->rdx_->evloop_, &c->timer_);
		}

		delete c;
	}

	command_map.clear();
	commands_deleted_ += len;

	return len;
}

// ---------------------------------
// get_command_map specializations
// ---------------------------------

template<> unordered_map<long, Command<redisReply *> *> &Redox::getCommandMap<
		redisReply *>() {
	return commands_redis_reply_;
}

template<> unordered_map<long, Command<string> *> &Redox::getCommandMap<string>() {
	return commands_string_;
}

template<> unordered_map<long, Command<char *> *> &Redox::getCommandMap<char *>() {
	return commands_char_p_;
}

template<> unordered_map<long, Command<int> *> &Redox::getCommandMap<int>() {
	return commands_int_;
}

template<> unordered_map<long, Command<long long int> *> &Redox::getCommandMap<
		long long int>() {
	return commands_long_long_int_;
}

template<> unordered_map<long, Command<nullptr_t> *> &Redox::getCommandMap<
		nullptr_t>() {
	return commands_null_;
}

template<> unordered_map<long, Command<vector<string>> *> &Redox::getCommandMap<
		vector<string>>() {
	return commands_vector_string_;
}

template<> unordered_map<long, Command<set<string>> *> &Redox::getCommandMap<
		set<string>>() {
	return commands_set_string_;
}

template<>
unordered_map<long, Command<unordered_set<string>> *> &
Redox::getCommandMap<unordered_set<string>>() {
	return commands_unordered_set_string_;
}

// ----------------------------
// Helpers
// ----------------------------

string Redox::vecToStr(const vector<string> &vec, const char delimiter) {
	string str;
	for (size_t i = 0; i < vec.size() - 1; i++)
		str += vec[i] + delimiter;
	str += vec[vec.size() - 1];
	return str;
}

vector<string> Redox::strToVec(const string &s, const char delimiter) {
	vector<string> vec;
	size_t last = 0;
	size_t next = 0;
	while ((next = s.find(delimiter, last)) != string::npos) {
		vec.push_back(s.substr(last, next - last));
		last = next + 1;
	}
	vec.push_back(s.substr(last));
	return vec;
}

void Redox::command(const vector<string> &cmd) {
	command<redisReply *>(cmd, nullptr);
}

bool Redox::commandSync(const vector<string> &cmd) {
	auto &c = commandSync<redisReply *>(cmd);
	bool succeeded = c.ok();
	c.free();
	return succeeded;
}

string Redox::get(const string &key) {

	Command<char *> &c = commandSync<char *>( { "GET", key });
	if (!c.ok()) {
		throw runtime_error(
				"[FATAL] Error getting key " + key + ": Status code "
						+ to_string(c.status()));
	}
	string reply = c.reply();
	c.free();
	return reply;
}

bool Redox::set(const string &key, const string &value) {
	return commandSync( { "SET", key, value });
}

bool Redox::del(const string &key) {
	return commandSync( { "DEL", key });
}

void Redox::publish(const string &topic, const string &msg) {
	command<redisReply *>( { "PUBLISH", topic, msg });
}

} // End namespace redis
