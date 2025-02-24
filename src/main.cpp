#include "EventHandler.hpp"
#include "Renderer.hpp"
#include "Consometer.hpp"

#include <SFML/Graphics/View.hpp>
#include <cstdint>
#include <iostream>



int main() {
	
	Particle_simulator sim;

	sf::RenderWindow window(sf::VideoMode(1800, 1000), "Particle sim");
	Renderer renderer(sim, window);

	EventHandler eventHandler(renderer, window, sim);
	EventOrders eventOrders = eventHandler.getOrders();

	sim.start_simulation_threads();

	Consometre conso_this_thread;
	Consometre clock;
	uint32_t nb_frames = 0;

	uint32_t i=0;
	while (window.isOpen()) {
		clock.Start();
		conso_this_thread.Start();
		{
			eventHandler.loopOverEvents();
			renderer.update_display();
		}
		conso_this_thread.End();
		std::this_thread::sleep_for(std::chrono::nanoseconds(Consometre::NB_TICK_SEC/renderer.FPS_limit) - conso_this_thread.Sum());
		conso_this_thread.setZero();
		
		nb_frames++;
		if (clock.count() > Consometre::NB_TICK_SEC) { // 1s passed
			// 	std::cout << "Conso simu : " << (float)conso_simu.count() / nb_frames / 1000000 << "ms (" << (float)conso_simu.count() / conso_tot.count() *100 << " %)\t\tConso render : " << (float)conso_render.count() / nb_frames / 1000000 << " ms (" << (float)conso_render.count() / conso_tot.count() *100 << " %)" << std::endl;
			std::cout << "FPS : " << nb_frames << std::endl;
			clock.setZero();
			nb_frames = 0;
		}
		clock.End();
	}

	sim.stop_simulation_threads();
	return 0;
}