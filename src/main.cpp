#include "Renderer.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>


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
	sf::RenderWindow window(sf::VideoMode(1000, 1000), "Particle sim");
	sf::Event event;
	// sf::RenderWindow window(sf::VideoMode(1000, 500), "Particle sim", sf::Style::Fullscreen);
	
	Particle_simulator sim;

	Renderer renderer(window, sim);
	renderer.update_display();

	Consometre conso_tot;
	Consometre conso_simu;
	Consometre conso_render;
	uint32_t nb_frames = 0;

	uint32_t i=0;
	while (window.isOpen()) {
		while (window.pollEvent(event)) {}

		conso_tot.Start();
		// std::this_thread::sleep_for(std::chrono::milliseconds(10));
		
		conso_render.Start();
		renderer.update_display();
		conso_render.End();

		conso_simu.Start();
		sim.simu_step();
		conso_simu.End();

		conso_tot.End();
		nb_frames++;
		if (conso_tot.count() > Consometre::NB_TICK_SEC) {
			std::cout << "FPS : " << nb_frames << std::endl;
			std::cout << "Conso simu : " << (float)conso_simu.count() / nb_frames / 1000000 << "ms (" << (float)conso_simu.count() / conso_tot.count() *100 << " %)\t\tConso render : " << (float)conso_render.count() / nb_frames / 1000000 << " ms (" << (float)conso_render.count() / conso_tot.count() *100 << " %)" << std::endl;
			conso_tot.setZero();
			conso_render.setZero();
			conso_simu.setZero();
			nb_frames = 0;
		}


	}
}