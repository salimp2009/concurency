

#include <iostream>
#include <atomic>
#include <thread>

// Spinlock Example with memory_order


class Spinlock {
	std::atomic_flag m_atomic=ATOMIC_FLAG_INIT ;
public:
	Spinlock() = default;
	~Spinlock() = default;

	//bool TryAcquire() 
	//{
	//	// use an acquire fence to ensure all subsequent reads 
	//	// by this thread will be valid
	//	bool alreadyLocked = m_atomic.test_and_set(std::memory_order_acquire);

	//	return !alreadyLocked;

	//}

	void Acquire() {
		// spin until acquire
		while (m_atomic.test_and_set(std::memory_order_acquire)) {
			// reduce power consumption on Intel CPUs by PAUSE() or
			std::this_thread::yield();
			//PAUSE();  // not supported on every platform
		}
	}

	void Release() {
		// release semantics used to make sure all prior
		// writes have been fully commited before releasing lock
		m_atomic.clear(std::memory_order_release);
	}

};

template<typename LOCK>
class ScopedLock
{ // ScopedLock can be used with any Spinlock or mutex that has Acquire() & Release() interface
private:
	typedef LOCK lock_t;
	lock_t* m_plock;
public:
	explicit ScopedLock(lock_t& lock) : m_plock{ &lock } 
	{
		m_plock->Acquire();
	}

	~ScopedLock() 
	{
		m_plock->Release();
	}

};

Spinlock g_lock{};

int ThreadSafeFunction() {
	// scoped lock acts like a janitor
	ScopedLock<decltype(g_lock)>janitor(g_lock);
	std::cout << "\nDo Some work..." << std::this_thread::get_id()<<'\n';

	//if (SomethingWentWrong()) {
	//	//lock will be released
	//	return -1;
	//}

	//otherwise do some work
	std::cout << "\nDoing some more work...." << std::this_thread::get_id() << '\n';

	//lock will be released here
	return 0;
}

int main()
{
	std::thread t1(ThreadSafeFunction);
	std::thread t2(ThreadSafeFunction);

	std::cout << "\nMain thread is also working\n";

	t1.join();
	t2.join();
	
	std::cout << "\nFinishing program\n";
	std::cout << '\n';
}


