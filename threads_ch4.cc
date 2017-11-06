/* Synchrosizing Operations */
#include "stdafx.h"

#include <deque>
#include <functional>
#include <iostream>
#include <fstream>
#include <thread>
#include <string>
#include <mutex>

std::deque<int> q;
std::mutex mu;

void function_1() {
	int count = 10;
	while (count > 0) {
		std::unique_lock<mutex> locker(mu);
		q.push_front(count);
		locker.unlock();
		std::this_thread::sleep_for(chrono::seconds(1));
		count--;
	}
}

void function_2() {
	int data = 0;
	while ( data != 1) {
		std::unique_lock<mutex> locker(mu);
		if (!q.empty()) {
			data = q.back();
			q.pop_back();
			locker.unlock();
			cout << "t2 got a value from t1: " << data << endl;
		} else {
			locker.unlock();
		}
		std::this_thread::sleep_for(chrono::milliseconds(10));
	}
}
// It is hard to set the sleep time.
int main() {
	std::thread t1(function_1);
	std::thread t2(function_2);
	t1.join();
	t2.join();
	return 0;
}



// Using conditional variable and mutex
void function_1() {
	int count = 10;
	while (count > 0) {
		std::unique_lock<mutex> locker(mu);
		q.push_front(count);
		locker.unlock();
		cond.notify_one();  // Notify one waiting thread, if there is one.
		std::this_thread::sleep_for(chrono::seconds(1));
		count--;
	}
}

void function_2() {
	int data = 0;
	while ( data != 1) {
		std::unique_lock<mutex> locker(mu);
		cond.wait(locker, [](){ return !q.empty();} );  // Unlock mu and wait to be notified
			// relock mu
		data = q.back();
		q.pop_back();
		locker.unlock();
		cout << "t2 got a value from t1: " << data << endl;
	}
}







/* For threads to return values: future */
int factorial(int N) {
	int res = 1;
	for (int i=N; i>1; i--)
		res *= i;

	return res;
}

int main() {
	//future<int> fu = std::async(factorial, 4); 
	future<int> fu = std::async(std::launch::deferred | std::launch::async, factorial, 4);
	cout << "Got from child thread #: " << fu.get() << endl;
	// fu.get();  // crash
	return 0;
}



/* Asynchronously provide data with promise */
int factorial(future<int>& f) {
	// do something else

	int N = f.get();     // If promise is distroyed, exception: std::future_errc::broken_promise
	cout << "Got from parent: " << N << endl; 
	int res = 1;
	for (int i=N; i>1; i--)
		res *= i;

	return res;
}

int main() {
	promise<int> p;
	future<int> f = p.get_future();

	future<int> fu = std::async(std::launch::async, factorial, std::ref(f));

	// Do something else
	std::this_thread::sleep_for(chrono::milliseconds(20));
	//p.set_value(5);   
	//p.set_value(28);  // It can only be set once
	p.set_exception(std::make_exception_ptr(std::runtime_error("Flat tire")));

	cout << "Got from child thread #: " << fu.get() << endl;
	return 0;
}




/* shared_future */
int factorial(shared_future<int> f) {
	// do something else

	int N = f.get();     // If promise is distroyed, exception: std::future_errc::broken_promise
	f.get();
	cout << "Got from parent: " << N << endl; 
	int res = 1;
	for (int i=N; i>1; i--)
		res *= i;

	return res;
}

int main() {
	// Both promise and future cannot be copied, they can only be moved.
	promise<int> p;
	future<int> f = p.get_future();
	shared_future<int> sf = f.share();

	future<int> fu = std::async(std::launch::async, factorial, sf);
	future<int> fu2 = std::async(std::launch::async, factorial, sf);

	// Do something else
	std::this_thread::sleep_for(chrono::milliseconds(20));
	p.set_value(5);

	cout << "Got from child thread #: " << fu.get() << endl;
	cout << "Got from child thread #: " << fu2.get() << endl;
	return 0;
}










/* async() are used in the same ways as thread(), bind() */
class A {
public:
	string note;
	void f(int x, char c) { }
	long g(double x) { note = "changed"; return 0;}
	int operator()(int N) { return 0;}
};
A a;

int main() {
	a.note = "Original"; 
	std::future<int> fu3 = std::async(A(), 4);    // A tmpA;  tmpA is moved to async(); create a task/thread with tmpA(4);
	std::future<int> fu4 = std::async(a, 7);    
	std::future<int> fu4 = std::async(std::ref(a), 7); // a(7);  Must use reference wrapper
	std::future<int> fu5 = std::async(&a, 7); // Won't compile

	std::future<void> fu1 = std::async(&A::f, a, 56, 'z'); // A copy of a invokes f(56, 'z')
	std::future<long> fu2 = std::async(&A::g, &a, 5.6);    // a.g(5.6);  a is passed by reference
		// note: the parameter of the invocable are always passed by value, but the invokeable itself can be passed by ref.
	cout << a.note << endl;
	return 0;
}
/*
	std::thread t1(a, 6);   
	std::async(a, 6);   
    std::bind(a, 6);
    std::call_once(once_flag, a, 6);

	std::thread t2(a, 6); 
	std::thread t3(std::ref(a), 6); 
	std::thread t4(std::move(a), 6);
	std::thread t4([](int x){return x*x;}, 6);

	std::thread t5(&A::f, a, 56, 'z');  // copy_of_a.f(56, 'z')
	std::thread t6(&A::f, &a, 56, 'z');  // a.f(56, 'z') 
*/



/* packaged_task */

std::mutex mu;
std::deque<std::packaged_task<int()> > task_q;

int factorial(int N) {
	int res = 1;
	for (int i=N; i>1; i--)
		res *= i;

	return res;
}

void thread_1() {
	for (int i=0; i<10000; i++) {
		std::packaged_task<int()> t;
		{
			std::lock_guard<std::mutex> locker(mu);
			if (task_q.empty()) 
				continue;
			t = std::move(task_q.front());
			task_q.pop_front();
		}
		t();
	}
}

int main() {
	std::thread th(thread_1);

	std::packaged_task<int()> t(bind(factorial, 6));  
	std::future<int> ret = t.get_future();
	std::packaged_task<int()> t2(bind(factorial, 9));
	std::future<int> ret2 = t2.get_future();
	{
		std::lock_guard<std::mutex> locker(mu);
		task_q.push_back(std::move(t));
		task_q.push_back(std::move(t2));
	}
	cout << "I see: " << ret.get() << endl;
	cout << "I see: " << ret2.get() << endl;

	th.join();
	return 0;
}



/* Summary
 * 3 ways to get a future:
 * - promise::get_future()
 * - packaged_task::get_future()
 * - async() returns a future
 */












/* threads with time constrains */

int main() {
    /* thread */
    std::thread t1(factorial, 6);
    std::this_thread::sleep_for(chrono::milliseconds(3));
    chrono::steady_clock::time_point tp = chrono::steady_clock::now() + chrono::microseconds(4);
    std::this_thread::sleep_until(tp);

    /* Mutex */
    std::mutex mu;
    std::lock_guard<mutex> locker(mu);
    std::unique_lock<mutex> ulocker(mu);
    ulocker.try_lock();
    ulocker.try_lock_for(chrono::nanoseconds(500));
    ulocker.try_lock_until(tp);

    /* Condition Variable */
    std:condition_variable cond;
    cond.wait_for(ulocker, chrono::microseconds(2));
    cond.wait_until(ulocker, tp);

    /* Future and Promise */
    std::promise<int> p; 
    std::future<int> f = p.get_future();
    f.get();
    f.wait();
    f.wait_for(chrono::milliseconds(2));
    f.wait_until(tp);

    /* async() */
    std::future<int> fu = async(factorial, 6);

    /* Packaged Task */
    std::packaged_task<int(int)> t(factorial);
    std::future<int> fu2 = t.get_future();
    t(6);
 	
	 return 0;
}





   // Together with thread library 
	std::this_thread::sleep_until(steady_clock::now() + seconds(3));

	std::future<int> fu;
	fu.wait_for(seconds(3));
	fu.wait_until(steady_clock::now() + seconds(3));

	std::condition_variable c;
	std::mutex mu;
	std::unique_lock<std::mutex> locker(mu);
	c.wait_for(locker, seconds(3));
	c.wait_until(locker, steady_clock::now() + seconds(3));
 
