#pragma once

#include <future>
#include <memory>
#include <thread>
#include <algorithm>
#include <functional>

#include "threadsafe_queue.hpp"

class ThreadPool {
private:
    struct Task{
        virtual void operator()() = 0;

        virtual ~Task() = default;
    };

    template<typename Func>
    class FunctionWrapper : public Task {
    private:
        Func function_;

    public:
        explicit FunctionWrapper(Func func) : function_(std::move(func)) {}

        void operator()() final {
            function_();
        }
    };

    using TaskQueue = ThreadsafeQueue<std::unique_ptr<Task>>;

    class ThreadWorker {
    private:
        std::jthread worker_;

        static void execute_tasks(TaskQueue& tasks_) {
            std::unique_ptr<Task> task;
            while(tasks_.wait_pop(task)){
                (*task)();
            }
        }

    public:
        explicit ThreadWorker(TaskQueue& queue)  {
            // Здесь было ужасно неприятное стреляющее в ногу UB если разрешено перемещение
            // worker_ = std::jthread(&ThreadWorker::execue_tasks, this);
            worker_ = std::jthread(ThreadWorker::execute_tasks, std::ref(queue));
        }

        ThreadWorker(ThreadWorker&&) noexcept = default;

        ~ThreadWorker() = default;
    };

private:
    TaskQueue tasks_;

    std::vector<ThreadWorker> threads_;

    std::mutex launch_locker_;

public:
    ThreadPool() = default;

    ThreadPool(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;

    ThreadPool& operator= (const ThreadPool&) = delete;

    ThreadPool& operator= (ThreadPool&&) & noexcept = delete;

    ~ThreadPool() {
        wait();
    };

    void run(size_t num_threads) {
        std::scoped_lock lock (launch_locker_);
        if(!threads_.empty())
            throw std::logic_error("Thread pool has already started.");
        std::generate_n(std::back_inserter(threads_), num_threads,
                        [this]() { return ThreadWorker(tasks_);});
    }

    void wait() {
        while(!tasks_.empty())
            std::this_thread::yield();
        tasks_.cancel();
        threads_.clear();
    }

    template<typename Func, typename... Args>
    std::future<std::invoke_result_t<Func, Args...>> addTask(Func&& func, Args&&... args) {
        std::packaged_task new_task (std::forward<Func>(func));
        auto result_future = new_task.get_future();

        // В файле task.hpp пример того, как можно избежать "магии" ниже
        auto bounded_task = new FunctionWrapper (std::bind(std::move(new_task), std::forward<Args>(args)...)); // До С++20 нужен deduction guide
        std::unique_ptr<std::remove_pointer_t<decltype(bounded_task)>> task_ptr(bounded_task);

        tasks_.push(std::move(task_ptr));
        return result_future;
    }
};
