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

#include "client.hpp"

namespace redox {

class Subscriber {

public:
  /**
  * Constructor. Same as Redox.
  */
  Subscriber();

  /**
  * Cleans up.
  */
  ~Subscriber();

  /**
  * Same as .noWait() on a Redox instance.
  */
  void noWait(bool state) { rdx_.noWait(state); }

  /**
  * Same as .connect() on a Redox instance.
  */
  bool connect(const std::string &host = REDIS_DEFAULT_HOST, const int port = REDIS_DEFAULT_PORT,
               std::function<void(int)> connection_callback = nullptr) {
    return rdx_.connect(host, port, connection_callback);
  }

  /**
  * Same as .connectUnix() on a Redox instance.
  */
  bool connectUnix(const std::string &path = REDIS_DEFAULT_PATH,
                   std::function<void(int)> connection_callback = nullptr) {
    return rdx_.connectUnix(path, connection_callback);
  }

  /**
  * Same as .stop() on a Redox instance.
  */
  void stop();

  /**
  * Same as .disconnect() on a Redox instance.
  */
  void disconnect();

  /**
  * Same as .wait() on a Redox instance.
  */
  void wait();

  /**
  * Subscribe to a topic.
  *
  * msg_callback: invoked whenever a message is received.
  * sub_callback: invoked when successfully subscribed
  * err_callback: invoked on some error state
  */
  void subscribe(const std::string topic,
                 std::function<void(const std::string &, const std::string &)> msg_callback,
                 std::function<void(const std::string &)> sub_callback = nullptr,
                 std::function<void(const std::string &)> unsub_callback = nullptr,
                 std::function<void(const std::string &, int)> err_callback = nullptr);

  /**
  * Subscribe to a topic with a pattern.
  *
  * msg_callback: invoked whenever a message is received.
  * sub_callback: invoked when successfully subscribed
  * err_callback: invoked on some error state
  */
  void psubscribe(const std::string topic,
                  std::function<void(const std::string &, const std::string &)> msg_callback,
                  std::function<void(const std::string &)> sub_callback = nullptr,
                  std::function<void(const std::string &)> unsub_callback = nullptr,
                  std::function<void(const std::string &, int)> err_callback = nullptr);

  /**
  * Unsubscribe from a topic.
  *
  * err_callback: invoked on some error state
  */
  void unsubscribe(const std::string topic,
                   std::function<void(const std::string &, int)> err_callback = nullptr);

  /**
  * Unsubscribe from a topic with a pattern.
  *
  * err_callback: invoked on some error state
  */
  void punsubscribe(const std::string topic,
                    std::function<void(const std::string &, int)> err_callback = nullptr);

  /**
  * Return the topics that are subscribed() to.
  */
  std::set<std::string> subscribedTopics() {
    std::lock_guard<std::mutex> lg(subscribed_topics_guard_);
    return subscribed_topics_;
  }

  /**
  * Return the topic patterns that are psubscribed() to.
  */
  std::set<std::string> psubscribedTopics() {
    std::lock_guard<std::mutex> lg(psubscribed_topics_guard_);
    return psubscribed_topics_;
  }

private:
  // Base for subscribe and psubscribe
  void subscribeBase(const std::string cmd_name, const std::string topic,
                     std::function<void(const std::string &, const std::string &)> msg_callback,
                     std::function<void(const std::string &)> sub_callback = nullptr,
                     std::function<void(const std::string &)> unsub_callback = nullptr,
                     std::function<void(const std::string &, int)> err_callback = nullptr);

  // Base for unsubscribe and punsubscribe
  void unsubscribeBase(const std::string cmd_name, const std::string topic,
                       std::function<void(const std::string &, int)> err_callback = nullptr);

  // Underlying Redis client
  Redox rdx_;

  // Keep track of topics because we can only unsubscribe
  // from subscribed topics and punsubscribe from
  // psubscribed topics, or hiredis leads to segfaults
  std::set<std::string> subscribed_topics_;
  std::mutex subscribed_topics_guard_;

  std::set<std::string> psubscribed_topics_;
  std::mutex psubscribed_topics_guard_;

  // Set of persisting commands, so that we can cancel them
  std::set<Command<redisReply *> *> commands_;

  // CVs to wait for unsubscriptions
  std::condition_variable cv_unsub_;
  std::condition_variable cv_punsub_;

  // Pending subscriptions
  std::atomic_int num_pending_subs_ = {0};
};

} // End namespace
