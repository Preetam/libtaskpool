#include <future>
#include <functional>
#include <memory>
#include <chrono>
#include <queue>
#include <vector>
#include <thread>

class abstract_task {
public:
	virtual bool ready() = 0;
	virtual void operator()() = 0;
	virtual ~abstract_task() {};
};

template<class R, class T>
class waiting_task : public abstract_task
{
public:
	using future_t = std::future<T>;
	using task_t = std::packaged_task<R(future_t)>;

	future_t fut;
	task_t task;

	waiting_task(future_t fut, task_t task)
	: fut(fut), task(task)
	{
	}

	waiting_task(future_t fut, std::function<R(future_t)> func)
	: fut(std::move(fut)), task(std::move(func))
	{
	}

	bool
	ready() {
		auto status = fut.wait_for(std::chrono::seconds(0));
		if (status == std::future_status::ready) {
			return true;
		}
		return false;
	}

	void
	operator()() {
		task(std::move(fut));
	}
};

template<class T>
class packaged_task : public abstract_task
{
private:
	std::packaged_task<T> _task;

public:
	packaged_task(std::packaged_task<T> task)
	: _task(std::move(task))
	{
	}

	packaged_task(std::function<T> func)
	: _task(func)
	{
	}

	bool
	ready() {
		return true;
	}

	void
	operator()() {
		_task();
	}
};

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
