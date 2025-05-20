#include "EventHandler.hpp"
#include "Renderer.hpp"
#include "Consometer.hpp"


int main() {
	
	Particle_simulator sim;
	
	Renderer renderer(sim);

	EventHandler eventHandler(renderer, sim);

	sim.start_simulation_threads();

	Consometre conso_this_thread;

	while (renderer.getWindow().isOpen()) {
		conso_this_thread.Start();
		{
			eventHandler.loopOverEvents();
			eventHandler.update_selection_pos();
			renderer.update_display();
		}
		conso_this_thread.End();
		if (!renderer.getVSync()) std::this_thread::sleep_for(std::chrono::nanoseconds(Consometre::NB_TICK_SEC/renderer.FPS_limit) - conso_this_thread.getSum());
		conso_this_thread.setZero();
	}

	sim.stop_simulation_threads();
	return 0;
}