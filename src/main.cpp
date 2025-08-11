#include "EventHandler.hpp"
#include "Renderer.hpp"
#include "Particle_simulator.hpp"
#include "World.hpp"
#include "SaveLoader.hpp"

int main() {
	SaveLoader* saveLoader = new SaveLoader;
	PSparam* sim_param = new PSparam;
	WorldParam* world_param = new WorldParam;
	SLinfo* load_sim = new SLinfo;
	*load_sim = saveLoader->setParameters(*world_param, *sim_param);

	World world(*world_param);
	if (load_sim->isLoadWorld()) saveLoader->loadWorldSegNZones(world, load_sim->worldName());
	else {
		world.add_segment(100, 100, 900, 500);
		world.add_segment(1700, 300, 700, 800);
	}
	if (load_sim->isSaveWorld()) saveLoader->saveWorld(world, 10, FileHandler::KB, load_sim->worldName() + "-bin", 1);
	world.will_use_nParticles(sim_param->max_part);

	Particle_simulator sim(world, *sim_param, *load_sim);

	Renderer renderer(sim);
	renderer.should_use_speed();

	EventHandler eventHandler(renderer, sim);

	if (!load_sim->isLoadPos()) sim.start_simulation_threads();

	// We're done with loading and saving. These objects have done their job.
	delete saveLoader;
	delete sim_param;
	delete world_param;
	delete load_sim;

	Consometre conso_this_thread;

	while (renderer.getWindow().isOpen()) {
		conso_this_thread.Start();
		{
			eventHandler.loopOverEvents();
			eventHandler.update_selection_pos(); // We call this function here because calling it only when the mouse is moved lets the particles be moved around when the mouse doesn't move.
			renderer.update_display();
		}
		conso_this_thread.End();
		if (!renderer.getVSync()) std::this_thread::sleep_for(std::chrono::nanoseconds(Consometre::NB_TICK_SEC/renderer.FPS_limit) - conso_this_thread.getSum());
		conso_this_thread.setZero();
	}

	sim.stop_simulation_threads();
	return 0;
}