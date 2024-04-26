#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <variant>
#include <ucontext.h>

#define STACK_SIZE 18192

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
template <typename Return, typename Message> class Coroutine {
public:
	/** Return or nothing if the corutine has finished. */
	using MaybeReturn = std::optional<Return>;

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


	~Coroutine(){
		delete weak_executor;
		delete executor;
	};

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

	class Executor{
		ucontext_t main_context, generator_context;
		Message message;
		MaybeReturn value;

		char stack_main[STACK_SIZE];
		char stack_generator[STACK_SIZE];

		template <class Function, class... Args>
		static void run(Function&& f, std::weak_ptr<Executor>* executor, Args&&... args) {
			std::weak_ptr<Executor> sharedPtr = *executor;
			f(sharedPtr, args...);
			sharedPtr.lock()->value.reset();
			setcontext(&(executor->lock()->main_context));
		}

	public:
		using StopToken = Coroutine::StopToken;
		Executor():
			main_context(),
			generator_context() {
		}

		~Executor(){
			std::flush(std::cout);
		}

		template <typename Function, typename... Args>
		void start(std::weak_ptr<Executor>* executor, Function&& func_, Args&&... args_){
			volatile bool b = true;

			getcontext(&main_context);
			main_context.uc_stack.ss_sp = stack_main;
			main_context.uc_stack.ss_size = sizeof(stack_main);

			if (b) {
				b = false;

				getcontext(&generator_context);
				generator_context.uc_stack.ss_sp = stack_generator;
				generator_context.uc_stack.ss_size = sizeof(stack_generator);

				makecontext(
					&generator_context,
					(void (*)(void))Executor::run<Function, Args...>,
					sizeof...(Args) + 2,
					&func_,
					executor,
					&args_...);
			}
		}

		MaybeReturn wait(){
			swapcontext(&main_context, &generator_context);
			return std::move(value);
		}

		void resume(Message instruction){
			message=std::move(instruction);
		}

		MessageOrStop yield(Return value){
			this->value = std::move(value);
			swapcontext(&generator_context, &main_context);
			return std::move(message);
		}
	};

private:
	std::shared_ptr<Executor>* executor;
	std::weak_ptr<Executor>* weak_executor;
	//Executor* executor;
};
}  // namespace ecole::utility

namespace ecole::utility {


/*********************************
 *  Implementation of Coroutine  *
 *********************************/

template <typename Return, typename Message>
template <typename Function, typename... Args>
Coroutine<Return, Message>::Coroutine(Function&& func_, Args&&... args_):
	executor(new std::shared_ptr<Executor>()),
	weak_executor(new std::weak_ptr<Executor>()){
	(*executor) = std::make_shared<Executor>();
	(*weak_executor) = *executor;
	(*executor)->template start<>(weak_executor, func_, args_...);
}


template <typename Return, typename Message> auto Coroutine<Return, Message>::wait() -> MaybeReturn {
	return (*executor)->wait();
}

template <typename Return, typename Message> auto Coroutine<Return, Message>::resume(Message instruction) -> void {
	(*executor)->resume(instruction);
}

template <typename Return, typename Message>
auto Coroutine<Return, Message>::is_stop(MessageOrStop const& message) -> bool {
	return std::holds_alternative<StopToken>(message);
}

}  // namespace ecole::utility
