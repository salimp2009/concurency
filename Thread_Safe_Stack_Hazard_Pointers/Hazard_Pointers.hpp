#ifndef HAZARD_POINTERS_H
#define HAZARD_POINTERS_H

//#pragma once											// windows implemantation oheader guard; not portable
#include <thread>
#include <atomic>
#include <memory>
#include <exception>

unsigned constexpr max_hazard_pointers{ 100 };			// size of hazard pointer array
struct hazard_pointer {								    // define a hazard pointer struct with thread::id and atomic pointer
	std::atomic<std::thread::id>id;
	std::atomic<void*>pointer;
};

hazard_pointer hazard_pointers[max_hazard_pointers];	

class hp_owner										// hp owner class assigns hazard pointer for each thread with their id
{
private:
	hazard_pointer* hp;
public:
	hp_owner(hp_owner const&) = delete;
	hp_owner& operator=(hp_owner const&) = delete;

	hp_owner():hp{nullptr} 
	{ 
		for (std::size_t i{ 0 }; i < max_hazard_pointers; ++i) {
			std::thread::id old_id;
			if (hazard_pointers[i].id.compare_exchange_strong(old_id, std::this_thread::get_id())) {
				hp = &hazard_pointers[i];
				break;
			}
		}
		if(!hp) 
		{
			throw std::runtime_error("\nNo hazard pointer available..!!!\n");

		}
	}

	std::atomic<void*>& get_pointer() 
	{
		return hp->pointer;
	}

	~hp_owner() 
	{
		hp->pointer.store(nullptr);
		hp->id.store(std::thread::id());
	}
};

#endif // HAZARD_POINTERS_H

