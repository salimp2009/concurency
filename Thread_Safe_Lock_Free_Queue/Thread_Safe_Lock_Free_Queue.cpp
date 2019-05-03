#include <iostream>
#include <memory>
#include <atomic>
#include <cassert>

// Example to implement a Thread Safe Lock Free Queue 
//- first implement a regular linked list queue with atomic headand tail pointers to node; DONE
//- revise for multiple thread; NOT DONE 

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
		std::atomic<T*>data;
		std::atomic<node_counter>count;
		counted_node_ptr next;
		node()
		{
			node_counter new_count;
			new_count.internal_count = 0;
			new_count.external_counters = 2;	// every new starts out referenced from tail and 
			count.store(new_count);				// from next pointer of previous node once it is added to queue

			next.ptr = nullptr;
			next.external_count = 0;
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

public:
	
	//lock_free_queue() : head{}, tail{ head.load() } { }
	//lock_free_queue(const lock_free_queue&) = delete;
	//lock_free_queue& operator=(const lock_free_queue&) = delete;
	//~lock_free_queue() = default;
	
	std::unique_ptr<T> pop()
	{
		counted_node_ptr old_head = head.load(std::memory_order_relaxed);
		for (;;)
		{
			increase_external_count(head, old_head);
			node* const ptr = old_head.ptr;
			if (ptr == tail.load().ptr)
			{
				ptr->release_ref();
				return std::unique_ptr<T>();
			}
			if (head.compare_exchange_strong(old_head, ptr->next))
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
		std::unique_ptr<T>new_data(new T{ new_value });
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
				old_tail.ptr->next = new_next;
				old_tail = tail.exchange(new_next);
				free_external_counter(old_tail);
				new_data.release();
				break;
			}
			old_tail.ptr->release_ref();
		}
	}

};



int main()
{
	lock_free_queue<int>my_queue;

	my_queue.push(10);
	my_queue.push(20);

	std::cout << "\npop front: " << (*my_queue.pop()) << '\n';
	std::cout << "\npop front: " << (*my_queue.pop()) << '\n';
	 //std::cout << "\nPop front: " << (*my_queue.pop()) << '\n';
   
	return 0;
}
