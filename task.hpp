/// Пример реализации класса Task для более простого использования при добавлении в очередь
/// Вместо:
//          auto bounded_task = new FunctionWrapper (std::bind(std::move(new_task), std::forward<Args>(args)...));
//          std::unique_ptr<std::remove_pointer_t<decltype(bounded_task)>> task_ptr(bounded_task);
///
/// Будет:
//          Task bounded_task (std::bind(std::move(new_task), std::forward<Args>(args)...));

#pragma once

#include <memory>

class Task {
private:
    struct ITask {
        virtual void execute() const = 0;

        virtual std::unique_ptr<ITask> clone() const = 0;

        virtual ~ITask() = default;
    };

    template<typename T>
    struct TaskContext : ITask {
        T task_instance;

        explicit TaskContext(T &&task) : task_instance(std::move(task)) {}

        explicit TaskContext(const T &task) : task_instance(task) {}

        [[nodiscard]] std::unique_ptr<ITask> clone() const override {
            return std::make_unique<TaskContext>(*this);
        }

        void execute() const override {
            task_instance();
        }
    };

    std::unique_ptr<ITask> task_ptr = nullptr;

public:
    Task() = default;

    template<typename T>
    explicit Task(T &&task) : task_ptr(std::make_unique<TaskContext<T>>(std::forward<T>(task))) {}

    Task(const Task &other) : task_ptr(other.task_ptr->clone()) {}

    Task(Task &&) noexcept = default;

    Task &operator=(const Task &other) &{
        task_ptr = other.task_ptr->clone();
        return *this;
    }

    Task &operator=(Task &&other) & noexcept {
        std::swap(task_ptr, other.task_ptr);
        return *this;
    }

    template<typename T>
    Task &operator=(T &&other) &{
        task_ptr = std::make_unique<TaskContext<T>>(std::forward<T>(other));
        return *this;
    }

    void execute() const {
        task_ptr->execute();
    }
};

