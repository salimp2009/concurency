#include <iostream>
#include <atomic>
#include <thread>
#include <functional>
#include <cassert>
#include "Scoped_Lock.hpp"

class RentrantLock32
{
private:
	std::atomic<std::size_t> m_atomic;
	std::int32_t			 m_refCount;
public:
	RentrantLock32():m_atomic{0}, m_refCount{0} { }

	void Acquire()
	{
		// use hash function to get unique id for each thread
		std::hash<std::thread::id> hasher;
		std::size_t tid = hasher(std::this_thread::get_id());
		//if this thread doesnot already hold the lock
		if (m_atomic.load(std::memory_order_relaxed)!=tid)
		{
			std::size_t unlockValue{ 0 };
			while (!m_atomic.compare_exchange_weak(unlockValue, tid,		//fence is below
				std::memory_order_relaxed, std::memory_order_relaxed))
			{
				unlockValue = 0;
				std::this_thread::yield();	// instead of PAUSE()
				//PAUSE();					// not supported every platform
			}
		}
		// we get the lock; so increment reference count so we can verify
		// Acquire() and Release() can be called in pairs
		++m_refCount;

		// use and acquire fence to ensure all subsequent reads by this thread will be valid
		std::atomic_thread_fence(std::memory_order_acquire);
	}

	void Release()
	{
		// use release semantics to ensure all prior writes 
		// have been fully commmitted before onlocking
		std::atomic_thread_fence(std::memory_order_release);

		std::hash<std::thread::id>hasher;
		std::size_t tid = hasher(std::this_thread::get_id());
		std::size_t actual = m_atomic.load(std::memory_order_relaxed);  // get thread id of lock owner heck 
		
		//check if we own the lock
		assert(actual == tid);
		--m_refCount;
		if (m_refCount == 0)
		{
			// release lock; it is safe we own it and no more needed cause m_refCount==0
			m_atomic.store(0, std::memory_order_relaxed);
		}
	}

	bool TryAcquire()
	{
		std::hash<std::thread::id> hasher;
		std::size_t tid = hasher(std::this_thread::get_id());

		bool acquired{ false };
		if (m_atomic.load(std::memory_order_relaxed) == tid)
		{
			acquired = 0;
		}
		else
		{
			std::size_t unlockValue{ 0 };
			acquired=m_atomic.compare_exchange_weak(unlockValue, tid,
						std::memory_order_relaxed, std::memory_order_relaxed);  // fence below!

		}
		if (acquired)
		{
			++m_refCount;
			std::atomic_thread_fence(std::memory_order_acquire);
		}
		return acquired;
	}
};
// Test case to see if there will be a deadlock using Rentrant Lock

RentrantLock32 re_entrant_lock;  // create a lock

void A()
{
	ScopedLock<decltype(re_entrant_lock)> janitor(re_entrant_lock);

	std::cout << "\nFunction A(): Doing some work.."<<std::this_thread::get_id()<<'\n';
	
}

void B()
{
	ScopedLock<decltype(re_entrant_lock)> janitor(re_entrant_lock);
	
	std::cout << "\nFunction B(): Doing some work...."<<std::this_thread::get_id()<<'\n';

	A();
}

int main()
{
    
	std::thread t1(B);
	std::thread t2(B);
	std::thread t3(B);
	std::thread t4(B);

	B();
	t1.join();
	t2.join();
	t3.join();
	t4.join();

	return 0;
}

