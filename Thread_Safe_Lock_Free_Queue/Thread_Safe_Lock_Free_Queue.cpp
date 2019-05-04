#include <iostream>
#include <memory>
#include <atomic>
#include <cassert>
#include <thread>

// Example to implement a Thread Safe Lock Free Queue 
// throws an exception error at line 156; due to target was nullptr in atomic compare_exchange 
// Exception error at line 156 in push() 
// T* old_data = nullptr;
// if (old_tail.ptr->data.compare_exchange_strong(old_data, new_data.get()));
// Unhandled exception thrown: write access violation.
//** _Tgt** was nullptr.occurred


template<typename T>
class lock_free_queue
{
private:
	struct node;
	struct counted_node_ptr
	{
		int external_count;
		node* ptr;
	};

	std::atomic<counted_node_ptr>head;							// refers to front element
	std::atomic<counted_node_ptr>tail;							// last element

	struct node_counter
	{
		unsigned internal_count : 30;			// make the variable 30 bits only
		unsigned external_counters : 2;			// to make total size 32 bit to fit into a machine word
	};											// so that these counts are update together to avoid race conditions						
												// and atomic operations will also most likely to be lock free on every platform 
	struct node
	{
		std::atomic<T*> data;
		std::atomic<node_counter> count;
		std::atomic<counted_node_ptr> next;
		node()
		{
			node_counter new_count;
			new_count.internal_count = 0;
			new_count.external_counters = 2;	// every new starts out referenced from tail and 
			count.store(new_count);				// from next pointer of previous node once it is added to queue

			counted_node_ptr new_next;
			new_next.ptr = nullptr;
			new_next.external_count = 0;
			next.store(new_next);
			//delete new_next.ptr;
		}

		void release_ref()
		{
			node_counter old_counter = count.load(std::memory_order_relaxed);
			node_counter new_counter;
			do
			{
				new_counter = old_counter;
				--new_counter.internal_count;
			} while (!count.compare_exchange_strong(old_counter, new_counter, 
									std::memory_order_acquire,  std::memory_order_relaxed));

			if (!new_counter.internal_count && !new_counter.external_counters)  // if both internal and external counts are zero, this is last reference
			{
				delete this;
			}
		}
	};

	static void increase_external_count(std::atomic<counted_node_ptr>& counter, counted_node_ptr& old_counter)												
	{
		counted_node_ptr new_counter;
		do 
		{
			new_counter = old_counter;
			++new_counter.external_count;
		} while (!counter.compare_exchange_strong(old_counter, new_counter,
									std::memory_order_acquire, std::memory_order_relaxed));
		old_counter.external_count = new_counter.external_count;
	}

	static void free_external_counter(counted_node_ptr& old_node_ptr)
	{
		node* const ptr = old_node_ptr.ptr;
		int const count_increase = old_node_ptr.external_count - 2;

		node_counter old_counter = ptr->count.load(std::memory_order_relaxed);
		node_counter new_counter;
		do
		{
			new_counter = old_counter;
			--new_counter.external_counters;
			new_counter.internal_count += count_increase;
		} while (!ptr->count.compare_exchange_strong(old_counter, new_counter,
										std::memory_order_acquire, std::memory_order_relaxed));
		if (!new_counter.internal_count && !new_counter.external_counters)
		{
			delete ptr;
		}
	}

	void set_new_tail(counted_node_ptr& old_tail, counted_node_ptr const &new_tail)
	{
		node* const current_tail_ptr = old_tail.ptr;
		while (!tail.compare_exchange_weak(old_tail, new_tail) && old_tail.ptr == current_tail_ptr);
		if (old_tail.ptr == current_tail_ptr)
			free_external_counter(old_tail);
		else
			current_tail_ptr->release_ref();
	}

public:
	
	lock_free_queue() : head{}, tail{ } { }
	lock_free_queue(const lock_free_queue&) = delete;
	lock_free_queue& operator=(const lock_free_queue&) = delete;
	~lock_free_queue() = default;
	
	std::unique_ptr<T> pop()
	{
		counted_node_ptr old_head = head.load(std::memory_order_relaxed);
		for (;;)
		{
			increase_external_count(head, old_head);
			node* const ptr = old_head.ptr;
			if (ptr == tail.load().ptr)
			{
				//ptr->release_ref();
				return std::unique_ptr<T>();
			}
			counted_node_ptr next = ptr->next.load();				// using memory_order_cst; might be changed if required
			if (head.compare_exchange_strong(old_head, next))
			{
				T* const res = ptr->data.exchange(nullptr);
				free_external_counter(old_head);
				return std::unique_ptr<T>(res);
			}
			ptr->release_ref();
		}	
	}


	void push(T new_value)
	{
		std::unique_ptr<T>new_data{ new T(new_value) };
		counted_node_ptr new_next;
		new_next.ptr = new node;
		new_next.external_count = 1;
		counted_node_ptr old_tail = tail.load();
		for(;;)
		{
			increase_external_count(tail, old_tail);
			T* old_data = nullptr;
			if (old_tail.ptr->data.compare_exchange_strong(old_data, new_data.get()))
			{
				counted_node_ptr old_next{ 0 };
				if (!old_tail.ptr->next.compare_exchange_strong(old_next, new_next))
				{
					delete new_next.ptr;
					new_next = old_next;
				}
				set_new_tail(old_tail, new_next);
				new_data.release();
				break;
			}
			else
			{
				counted_node_ptr old_next{ 0 };
				if (old_tail.ptr->next.compare_exchange_strong(old_next, new_next))
				{
					old_next = new_next;
					new_next.ptr = new node;
				}
				set_new_tail(old_tail, old_next);
			}
		}
	}
};

void push_queue(lock_free_queue<int>* q)
{
	for (int i{ 0 }; i < 10; ++i) 
	{
		q->push(i);
		std::cout << "\nPushing: " << i << '\n';
	}
}

void pop_queue(lock_free_queue<int>* q)
{
	for (int i{ 0 }; i < 10; ++i)
	{
		std::shared_ptr<int>p=q->pop();
		std::cout << "\nPop(): " << *p << '\n';
	}
}


int main()
{
	lock_free_queue<int>my_queue;

	std::thread t1(&lock_free_queue<int>::push, &my_queue, 30);
	std::thread t2(push_queue,&my_queue);
	std::thread t3(pop_queue, &my_queue);

	my_queue.push(10);

	t1.join();
	t2.join();
	t3.join();

	//std::cout << "\npop front: " << (*my_queue.pop()) << '\n';
	//std::cout << "\npop front: " << (*my_queue.pop()) << '\n';
	 //std::cout << "\nPop front: " << (*my_queue.pop()) << '\n';
   
	return 0;
}
