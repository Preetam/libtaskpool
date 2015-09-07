#include <future>
#include <functional>
#include <memory>
#include <chrono>

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
