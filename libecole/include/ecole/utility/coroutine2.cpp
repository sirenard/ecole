#pragma once

#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>
#include <variant>
#include <ucontext.h>
#include <functional>

#define STACK_SIZE 8192

namespace ecole::utility {

/**
 * Asynchronous cooperative interruptable code execution.
 *
 * Asynchronously execute a piece of code in an interative fashion while producing intermediary results.
 * User-defined messages can be send to communicate with the executor.
 * The execution flow is as follow:
 * 1. Upon creation, the instruction provided in the constructor start being executed by the executor.
 * 2. The ``yield`` function is called by the executor with the first return value.
 * 3. The coroutine calls ``wait`` to recieve that first return value.
 * 4. If no value is returned, then the executor has finished.
 * 5. Else, the coroutine calls ``resume`` with a message to pass to the executor.
 * 6. The executor recieve the message and continue its execution unitl the next ``yield``, the process repeats from 2.
 *
 * @tparam Return The type of return values created by the executor.
 * @tparam Message The type of the messages that can be sent to the executor.
 */
template <typename Return, typename Message> class Coroutine: public std::enable_shared_from_this<Coroutine<Return, Message>> {
	ucontext_t main_context, generator_context;
	volatile Message message;
	volatile Return value;

	char stack_[STACK_SIZE];

	template <class Function, class... Args>
	static void run(Function&& f, std::weak_ptr<Coroutine> const& coroutine, Args&&... args);

public:
	/** Return or nothing if the corutine has finished. */
	using MaybeReturn = std::optional<Return>;
	using Executor = Coroutine;

	/** Type indicating that the executor must terminate. */
	struct StopToken {};

	/** Message recieved by the executor. */
	using MessageOrStop = std::variant<Message, Coroutine::StopToken>;

	/**
	 * Start the execution.
	 *
	 * @param func Function used to define the code that needs to be executed by the executor.
	 *        The first parameter to that function is a ``std::weak_ptr<Executor>`` used to yield values and recieve
	 *        messages.
	 *        If the weak pointer is expired, the executor must terminate.
	 * @param args Additional parameters to be passed as additinal arguments to ``func``.
	 */
	template <class Function, class... Args> Coroutine(Function&& func, Args&&... args);

	template <class Function, class... Args>
	static std::shared_ptr<Coroutine> test(Function&& func, Args&&... args) {
		return std::make_shared<Coroutine<Return, Message>>(&func, &args...);
	}

	~Coroutine()=default;

	/**
	 * Wait for the executor to yield a value.
	 *
	 * If no return value is given, then the coroutine has finished.
	 * This function should not be called successively without calling ``resume``.
	 */
	auto wait() -> MaybeReturn;

	/**
	 * Send a message and resume the executor.
	 *
	 * This function should not be called without first calling ``wait``, or if ``wait`` has not returned a value.
	 */
	auto resume(Message instruction) -> void;

	/** Return whether the message is a ``StopToken``/ */
	static auto is_stop(MessageOrStop const& message) -> bool;

	auto yield(Return value) -> MessageOrStop;
};
}  // namespace ecole::utility

namespace ecole::utility {


/*********************************
 *  Implementation of Coroutine  *
 *********************************/

template <typename Return, typename Message>
template <typename Function, typename... Args>
Coroutine<Return, Message>::Coroutine(Function&& func_, Args&&... args_):
	main_context(),
	generator_context(){
	volatile bool b=true;

	getcontext(&main_context);
	main_context.uc_stack.ss_sp = stack_;
	main_context.uc_stack.ss_size = sizeof(stack_);

	if(b){
		b=false;
		getcontext(&generator_context);
		generator_context.uc_stack.ss_sp = stack_;
		generator_context.uc_stack.ss_size = sizeof(stack_);

		std::weak_ptr<Coroutine> const weak = this->shared_from_this();


		makecontext(&generator_context, (void (*)(void))Coroutine::run<Function, Args...>, sizeof...(Args)+2, &func_, weak, &args_...);
	}
}

template <typename Return, typename Message>
auto Coroutine<Return, Message>::yield(Return value) -> MessageOrStop {
	this->value = value;
	swapcontext(&generator_context, &main_context);
	return message;
}

template <typename Return, typename Message> auto Coroutine<Return, Message>::wait() -> MaybeReturn {
	swapcontext(&main_context, &generator_context);
	return value;
}

template <typename Return, typename Message> auto Coroutine<Return, Message>::resume(Message instruction) -> void {
	message=instruction;
}

template <typename Return, typename Message>
auto Coroutine<Return, Message>::is_stop(MessageOrStop const& message) -> bool {
	return std::holds_alternative<StopToken>(message);
}


template <typename Return, typename Message>
template <class Function, class... Args>
void Coroutine<Return, Message>::run(Function&& f, std::weak_ptr<Executor> const& coroutine, Args&&... args) {
	f(coroutine, args...);
}


}  // namespace ecole::utility
