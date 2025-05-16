#pragma once

#include <atomic>
#include <functional>
#include <thread>


class ThreadHandler {
public :
	std::vector<std::thread*> threads;

private :
	std::atomic_uint8_t synched_threads{0}; //< Number of threads that arrived to a synchronization point
	uint16_t rw_size = 0; //< reached_work size so the number of load_repartition that can be done at the same time.
	std::atomic_uint32_t* reached_work = nullptr; //< Used by load_repartition so each thread know where to start its work.
	pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t sync_condition = PTHREAD_COND_INITIALIZER;

public :
	~ThreadHandler();

	std::thread* operator[](uint8_t i) {return threads[i];};
	/**
	* @brief Stores a given thread to store.
	* @details It doesn't do much but I simply prefer it the threads are held by the threadHandler rather.
	* @return new_th 
	*/
	std::thread* give_new_thread(std::thread* new_th); //< returns new_th;

	/**
	* @brief Wait for every thread to end, then deletes them.
	* @details It is up to the user to make sure the threads aren't stuck on their work or waiting at a synchronization point.
	*/
	void wait_for_threads_end();


	/**
	* @brief Set the number of different functions we want to use with load_repartition
	* @warning Do not call this function while load_repartition is running on the same object. That could lead to bad things (e.g. segmentation fault)
	*/
	void set_nb_fun(uint16_t n);

	/**
	* @brief Used to prepare for a new round of load_repartition
	* @warning Do not call this function while load_repartition is running on the same object. That could lead to the function array_worker being called more times than expected.
	*/
	void prep_new_work_loop();

	/**
	* @brief Each thread calling load_repartition will be given a part of a work when free.
	* @details Used to share a work load equally (time-wise) between each thread calling this function with the same fun_id.
	* Since multiple thread can call load_repartition at the same time, but not for the same work, fun_id is used to identify to which work the caller wants to participate.
	* If there is still work to do and the calling thread is free, then it call array_work to work on work_subset elements of the array.
	* @param obj Object on which the work shall be done.
	* @param array_worker The work to be done on obj.
	* @param fun_id See details.
	* @param arr_size Size of the array in obj on which the work shall be done. Used to calculate each thread share of the load.
	* @param work_subset See details .
	*/
	template<typename T>
	void load_repartition(T* obj, void (T::*array_worker)(uint32_t, uint32_t), uint16_t fun_id, uint32_t arr_size, uint32_t work_subset);

	/**
	* @brief Mainly used when array_worker needs more than 2 arguments. Then use a bound function to pass array_worker.
	* @see load_repartition
	*/
	void load_repartition(std::function<void(uint32_t, uint32_t)> array_worker, uint16_t fun_id, uint32_t arr_size, uint32_t work_subset);

	/**
	* @brief Calling threads will wait for the n2synchronize-th one to call.
	* @details The first thread to arrive calls unique_work on obj if and only if first_thread_work.
	* The last thread to arrive calls unique_work on obj if and only if last_thread_work.
	* Once the last thread returns from unique_work, they all return (last one included).
	* Because of the extra work for the first thread, the first to arrive can also be the last one that the other threads wait for. This won't cause any trouble.
	* @param n2synchronize Number of thread to synchronize.
	* @param max_wait_sec Maximum number of seconds to wait before returning in case not enough threads call this function.
	* @param unique_work Work to be done on obj for the first thread arriving.
	* @param first_thread_work Whether the first thread should call unique_work.
	* @param last_thread_work  Whether the  last thread should call unique_work.
	*/
	template<typename T>
	void synchronize_internal(uint8_t n2synchronize, uint8_t max_wait_sec, T* obj, void (T::*unique_work)(void), bool first_thread_work, bool last_thread_work);

	/**
	* @brief Call synchronize_internal<ThreadHandler>(n2synchronize, max_wait_sec, nullptr, nullptr, false, false)
	* @see synchronize_internal
	*/
	inline void synchronize(uint8_t n2synchronize, uint8_t max_wait_sec) {
		synchronize_internal<ThreadHandler>(n2synchronize, max_wait_sec, nullptr, nullptr, false, false); // lol for the T substitution
	};

	/**
	* @brief Call synchronize_internal(n2synchronize, max_wait_sec, obj, unique_work, true, false)
	* @see synchronize_internal
	*/
	template<typename T>
	inline void synchronize_first(uint8_t n2synchronize, uint8_t max_wait_sec, T* obj, void (T::*unique_work)(void));
	/**
	* @brief Call synchronize_internal(n2synchronize, max_wait_sec, obj, unique_work, false, true);
	* @see synchronize_internal
	*/
	template<typename T>
	inline void synchronize_last(uint8_t n2synchronize, uint8_t max_wait_sec, T* obj, void (T::*unique_work)(void));
};







template<typename T>
void ThreadHandler::load_repartition(T* obj, void (T::*array_worker)(uint32_t, uint32_t), uint16_t fun_id, uint32_t arr_size, uint32_t work_subset) {
	uint32_t start, end;
	start = reached_work[fun_id].fetch_add(work_subset, std::memory_order_relaxed);
	end = std::min(start + work_subset, arr_size);
	while (start < arr_size) {
		(obj->*array_worker)(start, end);
		start = reached_work[fun_id].fetch_add(work_subset, std::memory_order_relaxed);
		end = std::min(start + work_subset, arr_size);
	}
}


template<typename T>
void ThreadHandler::synchronize_internal(uint8_t n2synchronize, uint8_t max_wait_sec, T* obj, void (T::*unique_work)(), bool first_thread_work, bool last_thread_work) {
	pthread_mutex_lock(&sync_mutex);
	if (first_thread_work && synched_threads.load()==0) { // first thread to arrive
		(obj->*unique_work)();
	}
	if (synched_threads.fetch_add(1) == n2synchronize-1) { // last thread to arrive (can be the first bc the first to arrive has to do some extra work)
		if (last_thread_work) (obj->*unique_work)();
		synched_threads.store(0);
		pthread_cond_broadcast(&sync_condition);
	} else { // other threads
		struct timespec t;
		clock_gettime(CLOCK_REALTIME, &t);
		t.tv_sec += max_wait_sec;
		pthread_cond_timedwait(&sync_condition, &sync_mutex, &t);
	}
	pthread_mutex_unlock(&sync_mutex);
	return;
}

template<typename T>
void ThreadHandler::synchronize_first(uint8_t n2synchronize, uint8_t max_wait_sec, T* obj, void (T::*unique_work)(void)) {
	synchronize_internal(n2synchronize, max_wait_sec, obj, unique_work, true, false);
};


template<typename T>
void ThreadHandler::synchronize_last(uint8_t n2synchronize, uint8_t max_wait_sec, T* obj, void (T::*unique_work)(void)) {
	synchronize_internal(n2synchronize, max_wait_sec, obj, unique_work, false, true);
};