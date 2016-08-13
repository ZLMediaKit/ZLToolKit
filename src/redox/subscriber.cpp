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

#include <string.h>
#include "subscriber.hpp"

using namespace std;

namespace redox {

Subscriber::Subscriber() {
}

Subscriber::~Subscriber() {
}

void Subscriber::disconnect() {
	stop();
	wait();
}

void Subscriber::wait() {
	rdx_.wait();
}

// This is a fairly awkward way of shutting down, where
// we pause to wait for subscriptions to happen, and then
// unsubscribe from everything and wait for that to finish.
// The reason is because hiredis goes into
// a segfault in freeReplyObject() under redisAsyncDisconnect()
// if we don't do this first.
// TODO(hayk): look at hiredis, ask them what causes the error
void Subscriber::stop() {

	this_thread::sleep_for(chrono::milliseconds(1000));

	for (const string &topic : subscribedTopics())
		unsubscribe(topic);

	for (const string &topic : psubscribedTopics())
		punsubscribe(topic);

	{
		unique_lock<mutex> ul(subscribed_topics_guard_);
		cv_unsub_.wait(ul, [this] {
			return (subscribed_topics_.size() == 0);
		});
	}

	{
		unique_lock<mutex> ul(psubscribed_topics_guard_);
		cv_punsub_.wait(ul, [this] {
			return (psubscribed_topics_.size() == 0);
		});
	}

	for (Command<redisReply *> *c : commands_)
		c->free();

	rdx_.stop();
}

// For debugging only
void debugReply(Command<redisReply *> c) {

	redisReply *reply = c.reply();

	cout << "------" << endl;
	cout << c.cmd() << " " << (reply->type == REDIS_REPLY_ARRAY) << " "
			<< (reply->elements) << endl;
	for (size_t i = 0; i < reply->elements; i++) {
		redisReply *r = reply->element[i];
		cout << "element " << i << ", reply type = " << r->type << " ";
		if (r->type == REDIS_REPLY_STRING)
			cout << r->str << endl;
		else if (r->type == REDIS_REPLY_INTEGER)
			cout << r->integer << endl;
		else
			cout << "some other type" << endl;
	}
	cout << "------" << endl;
}

void Subscriber::subscribeBase(const string cmd_name, const string topic,
		function<void(const string &, const string &)> msg_callback,
		function<void(const string &)> sub_callback,
		function<void(const string &)> unsub_callback,
		function<void(const string &, int)> err_callback) {

	Command<redisReply *> &sub_cmd =
			rdx_.commandLoop<redisReply *>( { cmd_name, topic },
					[this, topic, msg_callback, err_callback, sub_callback, unsub_callback](
							Command<redisReply *> &c) {

						if (!c.ok()) {
							num_pending_subs_--;
							if (err_callback)
							err_callback(topic, c.status());
							return;
						}

						redisReply *reply = c.reply();

						// If the last entry is an integer, then it is a [p]sub/[p]unsub command
						if ((reply->type == REDIS_REPLY_ARRAY) &&
								(reply->element[reply->elements - 1]->type == REDIS_REPLY_INTEGER)) {

							if (!strncmp(reply->element[0]->str, "sub", 3)) {
								lock_guard<mutex> lg(subscribed_topics_guard_);
								subscribed_topics_.insert(topic);
								num_pending_subs_--;
								if (sub_callback)
								sub_callback(topic);
							} else if (!strncmp(reply->element[0]->str, "psub", 4)) {
								lock_guard<mutex> lg(psubscribed_topics_guard_);
								psubscribed_topics_.insert(topic);
								num_pending_subs_--;
								if (sub_callback)
								sub_callback(topic);
							} else if (!strncmp(reply->element[0]->str, "uns", 3)) {
								lock_guard<mutex> lg(subscribed_topics_guard_);
								subscribed_topics_.erase(topic);
								if (unsub_callback)
								unsub_callback(topic);
								cv_unsub_.notify_all();
							} else if (!strncmp(reply->element[0]->str, "puns", 4)) {
								lock_guard<mutex> lg(psubscribed_topics_guard_);
								psubscribed_topics_.erase(topic);
								if (unsub_callback)
								unsub_callback(topic);
								cv_punsub_.notify_all();
							}

							else
							ErrorL<< "Unknown pubsub message: " << reply->element[0]->str;
						}

						// Message for subscribe
						else if ((reply->type == REDIS_REPLY_ARRAY) && (reply->elements == 3)) {
							char *msg = reply->element[2]->str;
							int len = reply->element[2]->len;
							if (msg && msg_callback)
							msg_callback(topic, string(msg, len));
						}

						// Message for psubscribe
						else if ((reply->type == REDIS_REPLY_ARRAY) && (reply->elements == 4)) {
							char *msg = reply->element[3]->str;
							int len = reply->element[3]->len;
							if (msg && msg_callback)
							msg_callback(reply->element[2]->str, string(msg, len));
						}

						else
						ErrorL << "Unknown pubsub message of type " << reply->type;
					}, 1e10 // To keep the command around for a few hundred years
					);

	// Add it to the command list
	commands_.insert(&sub_cmd);
	num_pending_subs_++;
}

void Subscriber::subscribe(const string topic,
		function<void(const string &, const string &)> msg_callback,
		function<void(const string &)> sub_callback,
		function<void(const string &)> unsub_callback,
		function<void(const string &, int)> err_callback) {
	lock_guard<mutex> lg(subscribed_topics_guard_);
	if (subscribed_topics_.find(topic) != subscribed_topics_.end()) {
		WarnL << "Already subscribed to " << topic << "!";
		return;
	}
	subscribeBase("SUBSCRIBE", topic, msg_callback, sub_callback,
			unsub_callback, err_callback);
}

void Subscriber::psubscribe(const string topic,
		function<void(const string &, const string &)> msg_callback,
		function<void(const string &)> sub_callback,
		function<void(const string &)> unsub_callback,
		function<void(const string &, int)> err_callback) {
	lock_guard<mutex> lg(psubscribed_topics_guard_);
	if (psubscribed_topics_.find(topic) != psubscribed_topics_.end()) {
		WarnL << "Already psubscribed to " << topic << "!";
		return;
	}
	subscribeBase("PSUBSCRIBE", topic, msg_callback, sub_callback,
			unsub_callback, err_callback);
}

void Subscriber::unsubscribeBase(const string cmd_name, const string topic,
		function<void(const string &, int)> err_callback) {
	rdx_.command<redisReply *>( { cmd_name, topic },
			[topic, err_callback](Command<redisReply *> &c) {
				if (!c.ok()) {
					if (err_callback)
					err_callback(topic, c.status());
					return;
				}
			});
}

void Subscriber::unsubscribe(const string topic,
		function<void(const string &, int)> err_callback) {
	lock_guard<mutex> lg(subscribed_topics_guard_);
	if (subscribed_topics_.find(topic) == subscribed_topics_.end()) {
		WarnL << "Cannot unsubscribe from " << topic << ", not subscribed!";
		return;
	}
	unsubscribeBase("UNSUBSCRIBE", topic, err_callback);
}

void Subscriber::punsubscribe(const string topic,
		function<void(const string &, int)> err_callback) {
	lock_guard<mutex> lg(psubscribed_topics_guard_);
	if (psubscribed_topics_.find(topic) == psubscribed_topics_.end()) {
		WarnL << "Cannot punsubscribe from " << topic << ", not psubscribed!";
		return;
	}
	unsubscribeBase("PUNSUBSCRIBE", topic, err_callback);
}

} // End namespace
