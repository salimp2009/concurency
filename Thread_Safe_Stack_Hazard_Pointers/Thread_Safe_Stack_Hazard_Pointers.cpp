#include <iostream>
//#include <thread>
//#include <atomic>
//#include <memory>
#include "Hazard_Pointers.hpp"

// Under Progress!!!!!!
 
// Example for Lock Free Stack from book Concurency in Action
// UNDER CONSTRUCTION; the example is not finished;
	// - needs to delete pointer on the heap
	// = needs memory_order semantics applied for better performance
// this is Lock free but it is not wait free since atomic.compare_exchange_weak() can fail constantly    


std::atomic<void*>& get_hazard_pointer_for_current_thread() 
{
	thread_local static hp_owner hazard;						// each thread will have its own hazard pointer
	return hazard.get_pointer();
}

bool outstanding_hazard_pointers_for(void* p)					// scan through the hazard_pointer array for the passed 
{												
	for (std::size_t i{ 0 }; i < max_hazard_pointers; ++i) 
	{
		if (hazard_pointers[i].pointer.load() == p)
		{
			return true;
		}
	}
	return false;											// no need to  check every entry since unowned hazard pointer entries  will have nullptr value
}



template<typename T>
class lock_free_stack {
private:
	struct node
	{
		std::shared_ptr<T> data;
		node* next;
		node(T const& data) : data{ std::make_shared<T>(data) }, next{ nullptr } { }
	};
	std::atomic<node*>head;


public:
	void push(T const& data) 
	{
		node* const new_node = new node(data);
		new_node->next = head.load();
		while (!head.compare_exchange_weak(new_node->next, new_node));
	}

	std::shared_ptr<T> pop()
	{
		std::atomic<void*>& hp = get_hazard_pointer_for_current_thread();  // returns a reference to atomic pointer; local to each thread
		node* old_head = head.load();									   //old_head will be stored in hazard pointers; if there are no pointers in hp then delete 
		do {
			node* temp{ nullptr };
			do
			{
				temp = old_head;
				hp.store(old_head);												// store old_head in hp so otherthreads cannot delete 
				old_head = head.load();
			} while (old_head != temp);											// run the loop to make sure that node is not deleted between reading old_head & setting hp 

		} while (old_head && !head.compare_exchange_strong(old_head, old_head->next));
		hp.store(nullptr);													// clear hazard pointer once you get the node
		std::shared_ptr<T>res;
		if (old_head) 
		{
			res.swap(old_head->data);										// move old_head->data to res; data will be deleted automatically when not needed
			if (outstanding_hazard_pointers_for(old_head))					// check for hazard pointers referencing a node before deleting
			{
				reclaim_later(old_head);									// dont delete yet; other threads reference to that node and have hazard pointers
			} else 
			{
				delete old_head;											// delete old_head if no one references to it and no hazard pointers by other threads
			}
			delete_nodes_with_no_hazards();									
		}
		return res;
	}

};



int main()
{
	lock_free_stack<int>lf_stack;
	std::thread t2(&lock_free_stack<int>::push, &lf_stack, 20);
	std::thread t1(&lock_free_stack<int>::push, &lf_stack, 10);
	std::thread t3(&lock_free_stack<int>::pop, &lf_stack);


	t1.join();
	t2.join();
	t3.join();

	std::cout << *(lf_stack.pop()) << '\n';

	return 0;
}