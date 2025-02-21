#pragma once

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Particle_simulator.hpp"


class Renderer {
private :
	sf::RenderWindow& window;
	sf::View worldView;
	// sf::View UIView;

	Particle_simulator& particle_sim;
	sf::VertexArray particle_vertices;
	sf::Texture particle_texture;
	sf::VertexArray segment_vertices;

	sf::Color background = sf::Color::Black;

public :
	Renderer(Particle_simulator& particle_sim_, sf::RenderWindow& window_);
	~Renderer();

	uint16_t FPS_limit = 60;

	void update_display();
	void update_particle_vertices();
	void update_segment_vertices();

	inline sf::View& getworldView() {return worldView;};
	// inline sf::View& getUIView() {return UIView;};
};