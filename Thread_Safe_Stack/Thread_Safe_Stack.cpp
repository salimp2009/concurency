#include <iostream>
#include <thread>
#include <atomic>
#include <cassert>
#include <memory>

// Under Progress!!!!!!

// Example for Lock Free Stack from book Concurency in Action
// UNDER CONSTRUCTION; the example is not finished;
	// - needs to delete pointer on the heap
	// = needs memory_order semantics applied for better performance
// Lock free but it is not wait free since atomic.compare_exchange_weak() can fail constantly    

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
	std::atomic<unsigned>threads_in_pop;		// counter for the threads trying to pop()
	std::atomic<node*>to_be_deleted;
	static void delete_nodes(node* nodes) {
		while (nodes) {
			node* next = nodes->next;
			delete nodes;
			nodes = next;
		}
	}
	
	void try_reclaim(node* old_head) 
	{
		if (threads_in_pop == 1) 
		{																// first check if there is only one thread
			node* nodes_to_delete = to_be_deleted.exchange(nullptr);	// if only thread, deletion is safe so change the value of atomic to nullptr
			if (!--threads_in_pop) 
			{
				delete_nodes(nodes_to_delete);						   // check if there are any threads after the first check; if none then safe to delete
			}
			else if (nodes_to_delete) 
			{
				chain_pending_nodes(nodes_to_delete);
			}
			delete old_head;
		}
		else
		{
			chain_pending_node(old_head);
			--threads_in_pop;
		}
	}

	void chain_pending_nodes(node* nodes) 
	{
		node* last = nodes;
		while (node* const next = last->next)
		{
			last = next;
		}
		chain_pending_nodes(nodes, last);
	}
	
	void chain_pending_nodes(node* first, node* last)
	{
		last->next = to_be_deleted;
		while (!to_be_deleted.compare_exchange_weak(last->next, first));
	}

	void chain_pending_node(node* n) 
	{
		chain_pending_nodes(n, n);
	}
	   

public:
	void push(T const& data) {
		node* const new_node = new node(data);
		new_node->next = head.load();
		while (!head.compare_exchange_weak(new_node->next, new_node));
		//std::cout << "\nHead data: " << *(head.load()->data) << "Thread ID: " << std::this_thread::get_id();
	}

	std::shared_ptr<T> pop() 
	{
		++threads_in_pop;				
		node * old_head = head.load();
		while (old_head && !head.compare_exchange_weak(old_head, old_head->next));
		std::shared_ptr<T>res;
		//std::cout << "\nHead data: " << *(head.load()->data) << "Thread ID: " << std::this_thread::get_id();
		if (old_head) {
			res.swap(old_head->data);		// move old_head->data to res; data will be deleted automatically when not needed
		}
		try_reclaim(old_head);			   // deal with raw pointers; if there is no thread it should be deleted
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