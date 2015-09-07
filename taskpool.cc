// g++ -std=c++14 -o taskpool taskpool.cc -lpthread
// ./taskpool
#include <iostream>
#include <functional>
#include <future>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <memory>
#include <atomic>
#include <vector>

#include "task.hpp"

class taskpool {
public:
	std::atomic_bool quit;
	std::queue<std::unique_ptr<abstract_task>> tasks;
	std::mutex task_list_mutex;
	std::vector<std::unique_ptr<std::thread>> runners;

	int threads;

	taskpool(int threads)
	: threads(threads)
	{
	}

	taskpool()
	: threads(4)
	{
	}

	void
	add_task(std::unique_ptr<abstract_task> task) {
		std::lock_guard<std::mutex> lock(task_list_mutex);
		tasks.push(std::move(task));
	}

	template<typename R, typename T>
	void
	add_task(waiting_task<R,T> wtask) {
		auto task = std::make_unique<waiting_task<R,T>>(std::move(wtask));
		add_task(std::move(task));
	}

	template<typename T>
	void
	add_task(std::packaged_task<T> ptask) {
		auto task = std::make_unique<packaged_task<T>>(std::move(ptask));
		add_task(std::move(task));
	}

	template<typename T>
	void
	add_task(std::function<T> func) {
		auto task = std::make_unique<packaged_task<void()>>(std::move(func));
		add_task(std::move(task));
	}

	std::unique_ptr<abstract_task>
	get_task() {
		std::lock_guard<std::mutex> lock(task_list_mutex);
		if (tasks.size() == 0) {
			return nullptr;
		}

		auto task_ptr = std::move(tasks.front());
		tasks.pop();
		return task_ptr;
	}

	void
	run() {
		quit = false;
		using namespace std::literals;
		
		for (int i = 0; i < threads; i++) {
			runners.push_back(std::move(std::make_unique<std::thread>([this](){
				while (true) {
					if (quit) {
						return;
					}

					auto taskptr = get_task();
					if (taskptr) {
						if (taskptr->ready()) {
							(*taskptr)();
						} else {
							add_task(std::move(taskptr));
						}
					} else {
						std::this_thread::sleep_for(100ms);
					}
				}
			})));
		}
	}

	void
	join() {
		for (auto& t : runners) {
			t->join();
		}
	}
};

void
printer() {
	std::cout << "printer()" << std::endl;
}

void
slow_function() {
	using namespace std::literals;
	std::cout << "starting slow function" << std::endl;
	std::this_thread::sleep_for(0.5s);
	std::cout << "finished slow function" << std::endl;
}

void
very_slow_function() {
	using namespace std::literals;
	std::cout << "starting very slow function" << std::endl;
	std::this_thread::sleep_for(5s);
	std::cout << "finished very slow function" << std::endl;
}

int
main() {
	auto pool = std::make_shared<taskpool>();
	pool->run();

	std::unique_ptr<abstract_task> abs_task = std::make_unique<packaged_task<void()>>([pool](){
		printer();
	});

	pool->add_task(std::move(abs_task));
	auto very_slow_task = std::packaged_task<void()>(very_slow_function);
	pool->add_task(waiting_task<void,void>(very_slow_task.get_future(),
		[pool](std::future<void> fut){
		pool->quit = true;
	}));
	pool->add_task(std::move(very_slow_task));
	pool->add_task(std::packaged_task<void()>([](){
		slow_function();
	}));
	pool->add_task(std::packaged_task<void()>([](){
		slow_function();
	}));
	pool->add_task(std::packaged_task<void()>([](){
		slow_function();
	}));

	pool->add_task(std::packaged_task<void()>([pool](){
		pool->add_task(std::packaged_task<void()>([pool](){
			pool->add_task(std::packaged_task<void()>([pool](){
				pool->add_task(std::packaged_task<void()>([pool](){
					auto a = std::packaged_task<int()>([](){ return 2; });
					auto a_future = a.get_future();
					pool->add_task(std::move(a));
					pool->add_task(waiting_task<void,int>(std::move(a_future), [](std::future<int> fut){
						int num = fut.get();
						std::cout << num << " squared is " << num*num << std::endl;
					}));
				}));
			}));
		}));
	}));
	pool->join();
	return 0;
}
