#pragma once

#include <chrono>
#include <string>

/**
* Used to measure time, mostly for performances reasons.
* This class also exist because I fing std::chrono to be quite verbose.
*/
class Consometre {
private :
	std::chrono::nanoseconds sum;
	std::chrono::steady_clock::duration start;

	uint32_t tick;
	uint32_t max_tick;
	std::string check_name;
	
public :
	Consometre();
	/**
	* @brief Sets start to the current time.
	*
	* You can do the following to measure the cumulative time of a specific function in a loop :
	* @code
	* Consometre conso;
	* while (loop) {
	* 	conso.Start();
	* 	foo();
	* 	conso.End();
	* 	bar();
	* }
	* @endcode
	*/
	void Start();

	/**
	* @brief Increases sum by the amount of time passed since last Start().
	*
	* You can do the following to measure the cumulative time of a specific function in a loop :
	* @code
	* Consometre conso;
	* while (loop) {
	* 	conso.Start();
	* 	foo();
	* 	conso.End();
	* 	bar();
	* }
	* @endcode
	*/
	void End();

	/**
	* @brief Sets sum to the amount of time passed since last Start().
	*/
	void Stop();

	/**
	* @brief Resets sum to 0.
	*/
	void setZero();

	/**
	* @brief Used because std::chrono::__seconds can be annoying.
	* @return The number of nanoseconds in sum.
	*/
	long count();
	std::chrono::nanoseconds& getSum();

	/**
	* @brief Used to check to time consumption of a block of code in a loop, averaging on a given number of laps.
	* @param check_name_ name of the check.
	* @param laps number of laps over which performances should be averaged.
	*/
	void start_perf_check(std::string check_name_, uint32_t laps);

	/**
	* @brief Tick to signal a lap has passed.
	* @details This is used to count the time passed in the entire loop. 
	* This function prints the check_name with the time consumption every max_tick call.
	* example :
	* @code
	* Consometre conso;
	* conso.start_perf_check("loop", 1000);
	* while (loop) {
	* 	conso.Tick_general();
	* 	foo();
	* 	bar();
	* }
	* @endcode
	*
	* @warning start_perf_check should have been called at least once before.
	*/
	void Tick_general();

	/**
	* @brief Tick to signal a lap has passed.
	* @return If max_tick has passed : average tick time
	* else : 0
	* @details This is used to count the time passed in a part of a loop. 
	* This function prints the check_name with the time consumption every max_tick call.
	* example :
	* @code
	* Consometre conso;
	* conso.start_perf_check("loop", 1000);
	* while (loop) {
		* 	conso.Start();
		* 	foo(); // Time consumtion of foo() is counted.
		* 	conso.Tick_fine();
	* 	bar(); // Time consumtion of bar() is NOT counted.
	* }
	* @endcode
	*
	* @warning start_perf_check should have been called at least once before.
	*/
	float Tick_fine(bool cout_result);

	/**
	* @brief Returns the average tick time in ms, since the last tick reset.
	* @details This changes in accuracy as more and more tick are passed.
	* @warning Tick_general or Tick_fine should have been called at least once before (though regularly is better). Else this function would serve no purpose.
	*/
	float get_average_perf_now();

	static const uint32_t NB_TICK_SEC = 1000000000; //< Number count() reaches after counting 1 second.
};