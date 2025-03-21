#include "EventHandler.hpp"
#include "Renderer.hpp"
#include "Consometer.hpp"

#include <SFML/Graphics/View.hpp>
#include <SFML/Window.hpp>
#include <cstdint>
#include <iostream>



int main() {
	
	Particle_simulator sim;
	
	Renderer renderer(sim);

	EventHandler eventHandler(renderer, sim);

	sim.start_simulation_threads();

	Consometre conso_this_thread;
	Consometre clock;
	uint32_t nb_frames = 0;

	uint32_t i=0;
	while (renderer.getWindow().isOpen()) {
		clock.Start();
		conso_this_thread.Start();
		{
			eventHandler.loopOverEvents();
			eventHandler.update_selection_pos();
			renderer.update_display();
		}
		conso_this_thread.End();
		if (!renderer.getVSync()) std::this_thread::sleep_for(std::chrono::nanoseconds(Consometre::NB_TICK_SEC/renderer.FPS_limit) - conso_this_thread.getSum());
		conso_this_thread.setZero();
		
		nb_frames++;
		if (clock.count() > Consometre::NB_TICK_SEC) { // 1s passed
			std::cout << "FPS : " << nb_frames << std::endl;
			clock.setZero();
			nb_frames = 0;
		}
		clock.End();
	}

	sim.stop_simulation_threads();
	return 0;
}