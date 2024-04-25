
#include <memory>
#include <variant>
#include <iostream>


#include "ecole/utility/coroutine2.cpp"

using namespace ecole;
using Coroutine = utility::Coroutine<int, int>;
using Executor = Coroutine;




int main(){
	auto f = [](std::weak_ptr<Executor> const& executor, int start) {
		int i =start;
		while(true) {
			auto message = executor.lock()->yield(i);
			if (Coroutine::is_stop(message)) {
				break;
			}
			i += std::get<int>(message);
		}
	};

	auto co = Coroutine::test(f, 5);
	//auto co = Coroutine{f, 5};

	for(int i=0; i<5; ++i){
		auto x = co.wait();
		std::cout << x.value() << std::endl;
		co.resume(i);
	}

}
