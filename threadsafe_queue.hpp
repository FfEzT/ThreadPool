#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <iostream>


template <typename T>
class ThreadsafeQueue {
public:
    ThreadsafeQueue() = default;

    ThreadsafeQueue(const ThreadsafeQueue&) = delete;

    ThreadsafeQueue(ThreadsafeQueue&&) = delete;

    size_t size() const {
        std::scoped_lock lock (locker_);
        return queue_.size();
    }

    bool empty() const {
        std::scoped_lock lock (locker_);
        return queue_.empty();
    }

    void push(const T& obj) {
        emplace(obj);
    }

    void push(T&& obj) {
        emplace(std::move(obj));
    }

    template<typename... Args>
    void emplace(Args&&... args) {
        std::scoped_lock lock (locker_);
        queue_.template emplace(std::forward<Args>(args)...);
        not_empty_waiter_.notify_one();
    }

    bool wait_pop(T& result) {
        std::unique_lock lock(locker_);
        bool pred = not_empty_waiter_.wait(lock, canceler_.get_token(), [this](){ return !queue_.empty();});
        if(pred){
            result = std::move(queue_.front());
            queue_.pop();
        }
        return pred;
    }

    bool pop(T& result) {
        std::scoped_lock lock (locker_);
        bool empty = queue_.empty();
        if(!empty){
            result = std::move(queue_.front());
            queue_.pop();
        }
        return empty;
    }

    void clear() {
        std::scoped_lock lock (locker_);
        queue_ = {};
    }

    void cancel() {
        canceler_.request_stop();
    }

    void reset() {
        canceler_.request_stop();
        std::scoped_lock lock (locker_);
        std::stop_source tmp;
        canceler_.swap(tmp);
    }

    void swap(ThreadsafeQueue& other) {
        if(this == &other) return;
        std::scoped_lock (locker_, other.locker_);
        std::swap(queue_, other.queue_);
    }

    ~ThreadsafeQueue() = default;

private:
    mutable std::mutex locker_;

    std::stop_source canceler_;

    std::condition_variable_any not_empty_waiter_;

    std::queue<T> queue_;
};
