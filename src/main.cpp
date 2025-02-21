#include "EventHandler.hpp"
#include "Renderer.hpp"

#include <SFML/Graphics/View.hpp>
#include <chrono>
#include <cstdint>
#include <iostream>


class Consometre {
private :
	std::chrono::nanoseconds sum;
	std::chrono::steady_clock::duration start;

public :
	Consometre() { setZero(); }
	void Start() { start = std::chrono::high_resolution_clock::now().time_since_epoch(); }
	void End()   { sum += std::chrono::high_resolution_clock::now().time_since_epoch() - start; }
	void setZero() { sum = (std::chrono::duration<long long, std::nano>)0; }
	auto count() {return sum.count();}
	std::chrono::nanoseconds Sum() {return sum;}

	static uint32_t NB_TICK_SEC;
};
uint32_t Consometre::NB_TICK_SEC = 1000000000;

int main() {
	
	Particle_simulator sim;

	sf::RenderWindow window(sf::VideoMode(1800, 1000), "Particle sim");
	Renderer renderer(sim, window);

	EventHandler eventHandler(renderer, window, sim);
	EventOrders eventOrders = eventHandler.getOrders();

	sim.start_simulation_threads();

	Consometre conso_tot;
	Consometre conso_simu;
	Consometre conso_render;
	uint32_t nb_frames = 0;

	uint32_t i=0;
	while (window.isOpen()) {
		conso_tot.Start();
		
		eventHandler.loopOverEvents();
		renderer.update_display();
		std::this_thread::sleep_for(std::chrono::microseconds(1000000/renderer.FPS_limit));
		
		// nb_frames++;
		// if (conso_tot.count() > Consometre::NB_TICK_SEC) { // 1s passed
		// 	std::cout << "FPS : " << nb_frames << std::endl;
		// 	std::cout << "Conso simu : " << (float)conso_simu.count() / nb_frames / 1000000 << "ms (" << (float)conso_simu.count() / conso_tot.count() *100 << " %)\t\tConso render : " << (float)conso_render.count() / nb_frames / 1000000 << " ms (" << (float)conso_render.count() / conso_tot.count() *100 << " %)" << std::endl;
		// 	conso_tot.setZero();
		// 	conso_render.setZero();
		// 	conso_simu.setZero();
		// 	nb_frames = 0;
		// }
		conso_tot.End();
	}

	sim.stop_simulation_threads();
	return 0;
}