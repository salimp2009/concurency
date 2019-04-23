#include "ParallelAccum.hpp"
#include <iostream>
#include <execution>



//#include <cstdlib>
//#include <array>
//#include <memory_resource>
//#include <shared_mutex>
//#include <Windows.h>
//#include <ppl.h>
//#include <concurrent_vector.h>


//std::shared_mutex vMutex;



int main()
{
	//std::array<std::byte, 2000>buf;
	//std::pmr::monotonic_buffer_resource pool{ buf.data(), buf.size()};

	std::vector<int> vec;
	for (unsigned long i{ 0 }; i <1000000; ++i)
		vec.push_back(i);

	auto sum = parallel_accumulate(vec.begin(), vec.end(), 0l);
	auto sum1 = std::reduce(std::execution::seq, vec.begin(), vec.end(), 0l);
	std::cout << "\nsum: " << sum << '\n';
	std::cout << "\nsum1: " << sum1 << '\n';
	
	std::sort(std::execution::par_unseq, vec.begin(), vec.end());
	std::for_each(std::execution::par, vec.begin(), vec.end(),
		[](int& elem) {elem *=elem;});
	auto num = std::count_if(std::execution::par, 
							vec.begin(), vec.end(),
							[](int elem) {return elem % 2 == 0; } );

	std::cout << "\nEven elements: " << num << '\n';
	std::cout <<"Vec[5]="<<vec[5]<<'\n';

	//concurrency::parallel_for_each(vec.begin(), vec.end(), [](auto &x) {x*=x;});  // windows implemantation of parallel algorithms


		

	return 0;
}