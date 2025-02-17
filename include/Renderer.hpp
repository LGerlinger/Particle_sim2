#pragma once

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Particle_simulator.hpp"

class Renderer {
private :
	sf::RenderWindow& window;
	sf::View view;

	Particle_simulator& particle_sim;
	sf::VertexArray particle_vertices;
	sf::Texture particle_texture;
	sf::VertexArray segment_vertices;

	sf::Color background = sf::Color::Black;

public :
	Renderer(sf::RenderWindow& window_, Particle_simulator& particle_sim_);
	~Renderer();

	void update_display();
	void update_particle_vertices();
	void update_segment_vertices();
};