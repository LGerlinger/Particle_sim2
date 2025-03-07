#include "Consometer.hpp"
#include <iostream>
#include <ratio>

uint32_t Consometre::NB_TICK_SEC = 1000000000;

Consometre::Consometre() { setZero(); }
void Consometre::Start() { start = std::chrono::high_resolution_clock::now().time_since_epoch(); }
void Consometre::End()   { sum += std::chrono::high_resolution_clock::now().time_since_epoch() - start; }
void Consometre::Stop()  { sum  = std::chrono::high_resolution_clock::now().time_since_epoch() - start; }
void Consometre::setZero() { sum = (std::chrono::duration<long long, std::nano>)0; }
long Consometre::count() {return sum.count();}
std::chrono::nanoseconds Consometre::Sum() {return sum;}


void Consometre::start_perf_check(std::string check_name_, uint32_t laps) {
	max_tick = laps;
	check_name = check_name_;
	tick = 0;
	Start();
}

void Consometre::Tick_general() {
	if (++tick == max_tick) {
		tick = 0;
		Stop();
		Start();
		float time = (float)count() / max_tick / 1000; // us
		setZero();
		std::cout << check_name << " : " << time << " us" << std::endl;
	}
}


void Consometre::Tick_fine() {
	End();
	if (++tick == max_tick) {
		tick = 0;
		float time = (float)count() / max_tick / 1000; // us
		setZero();
		std::cout << check_name << " : " << time << " us" << std::endl;
	}
}