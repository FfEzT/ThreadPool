#include <iostream>
#include "thread_pool.hpp"


int main() {

    ThreadPool pool;

    pool.addTask([](){std::cout << 3 << std::endl;});
    pool.addTask([](int i){std::cout << i << std::endl;}, 5);
    auto res = pool.addTask([](int i, int j){ return i * j ;}, 5, 17);

    pool.run(2);

    pool.addTask([](int q){std::cout << q*q << std::endl;}, res.get());

    pool.wait();
    return 0;
}
