#include "ThreadHandler.hpp"


ThreadHandler::~ThreadHandler() {
	wait_for_threads_end();
	if (reached_work != nullptr) delete[] reached_work;
}

std::thread* ThreadHandler::give_new_thread(std::thread* new_th) {
	threads.push_back(new_th);
	return new_th;
}

void ThreadHandler::wait_for_threads_end() {
	while(threads.size()) {
		threads.back()->join();
		delete(threads.back());
		threads.pop_back();
	}
}

void ThreadHandler::set_nb_fun(uint16_t n) {
	if (n != rw_size) {
		if (reached_work != nullptr) delete[] reached_work;
		reached_work = new std::atomic_uint32_t[n]();
		rw_size = n;

		for (uint16_t i=0; i<n; ++i) reached_work[i].store(0);
	}
}

void ThreadHandler::prep_new_work_loop() {
	for (uint16_t i=0; i<rw_size; ++i) reached_work[i].store(0);
}

void ThreadHandler::load_repartition(std::function<void(uint32_t, uint32_t)> array_worker, uint16_t fun_id, uint32_t arr_size, uint32_t work_subset) {
	uint32_t start, end;
	// work_subset = std::max(work_subset, (uint32_t)1); // work_subset=0 causes this function to block obviously 
	//                                                  // I should check this here, but I prefer to do it where I know how many is the minimum work_subset.
	                                                     
	start = reached_work[fun_id].fetch_add(work_subset, std::memory_order_relaxed);
	end = std::min(start + work_subset, arr_size);
	while (start < arr_size) {
		array_worker(start, end);
		start = reached_work[fun_id].fetch_add(work_subset, std::memory_order_relaxed);
		end = std::min(start + work_subset, arr_size);
	};
}