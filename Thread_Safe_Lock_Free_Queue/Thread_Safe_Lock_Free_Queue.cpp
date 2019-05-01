#include <iostream>
#include <memory>
#include <atomic>

// Example to implement a Thread Safe Lock Free Queue 
//- first implement a regular linked list queue with atomic headand tail pointers to node; DONE
//- revise for multiple thread; NOT DONE 

template<typename T>
class lock_free_queue
{
private:
	struct node
	{
		std::shared_ptr<T>data;
		node* next;
		node() :data{ std::make_shared<T>() }, next { nullptr } { }
	};
	std::atomic<node*>head;							// refers to front element
	std::atomic<node*>tail;							// last element

	node* pop_head()
	{
		node* const old_head = head.load();
		if(old_head==tail.load())				// tail.load has to sequence with the push() operation
		{
			return nullptr;			   // if the queue is empty return nullptr; 0
		}
		head.store(old_head->next);   // store the node after the old head as new head 
		return old_head;			  // return the old_head to delete
	}
public:
	lock_free_queue(): head{new node}, tail{head.load()} { }
	lock_free_queue(const lock_free_queue&) = delete;
	lock_free_queue& operator=(const lock_free_queue&) = delete;
	~lock_free_queue()
	{
		while (node* const old_head = head.load())
		{
			head.store(old_head->next);
			delete old_head;
		}
	}

	std::shared_ptr<T> pop()						// returns the value of the old head, store new head and delete old head
	{
		node* const old_head =pop_head();			//pop_head() returns head and stores the node after the head as the new head
		if(!old_head)
		{
			return std::shared_ptr<T>();			// if the queue is empty return nullptr
		}
		std::shared_ptr<T>const res{ old_head->data };
		delete old_head;
		return res;
	}


	void push(T new_value)
	{	// tail is the last element and is alway a null node
		std::shared_ptr<T>new_data(std::make_shared<T>(new_value));  // create a ptr for new value
		node* p = new node;											 // create a new null node; this will be new tail
		node* const old_tail = tail.load();							 // create a ptr to old tail
		old_tail->data.swap(new_data);								 // assign new data to old tail
		old_tail->next = p;											 // assign the new null node as the next of old tail (which has the new value)
		tail.store(p);												 // store tail as the new empty node 
	}
};



int main()
{
	lock_free_queue<int>my_queue;

	my_queue.push(10);
	my_queue.push(20);

	std::cout << "\nPop front: " << (*my_queue.pop()) << '\n';
	std::cout << "\nPop front: " << (*my_queue.pop()) << '\n';
	std::cout << "\nPop front: " << (*my_queue.pop()) << '\n';
   
	return 0;
}
