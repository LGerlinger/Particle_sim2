#include "Consometer.hpp"
#include <iostream>
// #include <ratio>

Consometre::Consometre() { setZero(); }
void Consometre::Start() { start = std::chrono::high_resolution_clock::now().time_since_epoch(); }
void Consometre::End()   { sum += std::chrono::high_resolution_clock::now().time_since_epoch() - start; }
void Consometre::Stop()  { sum  = std::chrono::high_resolution_clock::now().time_since_epoch() - start; }
void Consometre::setZero() { sum = (std::chrono::duration<long long, std::nano>)0; }
long Consometre::count() {return sum.count();}
std::chrono::nanoseconds& Consometre::getSum() {return sum;}


void Consometre::start_perf_check(std::string check_name_, uint32_t laps) {
	max_tick = laps;
	check_name = check_name_;
	tick = 0;
	setZero();
	Start();
}

void Consometre::Tick_general() {
	if (++tick == max_tick) {
		tick = 0;
		Stop();
		Start();
		float time = (float)count() / max_tick / 1000000; // ms
		std::cout << check_name << " : " << time << " ms" << std::endl;
		setZero();
	}
}


void Consometre::Tick_fine() {
	End();
	if (++tick == max_tick) {
		tick = 0;
		float time = (float)count() / max_tick / 1000000; // ms
		std::cout << check_name << " : " << time << " ms" << std::endl;
		setZero();
	}
}