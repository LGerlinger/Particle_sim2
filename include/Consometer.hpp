#pragma once

#include <chrono>
#include <string>

class Consometre {
private :
	std::chrono::nanoseconds sum;
	std::chrono::steady_clock::duration start;

	uint32_t tick;
	uint32_t max_tick;
	std::string check_name;
	
public :
	Consometre();
	void Start();
	void End();
	void Stop();
	void setZero();
	long count();
	std::chrono::nanoseconds Sum();

	void start_perf_check(std::string check_name_, uint32_t laps);
	void Tick_general();
	void Tick_fine();

	static uint32_t NB_TICK_SEC;
};