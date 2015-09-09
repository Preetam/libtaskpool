// g++ -std=c++14 -o taskpool taskpool.cc -lpthread
// ./taskpool

#include <iostream>

#include "task.hpp"

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
		auto a = std::packaged_task<int()>([](){ return 2; });
		auto a_future = a.get_future();
		pool->add_task(std::move(a));
		pool->add_task(waiting_task<void,int>(std::move(a_future), [](std::future<int> fut){
			int num = fut.get();
			std::cout << num << " squared is " << num*num << std::endl;
		}));
	}));

	pool->join();
	return 0;
}
