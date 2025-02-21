#include <iostream>

#include "Renderer.hpp"


Renderer::Renderer(Particle_simulator& particle_sim_, sf::RenderWindow& window_) : 
	particle_sim(particle_sim_), window(window_)
{
	std::cout << "Renderer::Renderer()" << std::endl;
	worldView = window.getDefaultView();

	// Initializing Particle VertexArray
	particle_vertices.setPrimitiveType(sf::Quads);
	particle_vertices.resize(4*particle_sim.nb_active_part);

	if (particle_texture.loadFromFile("disk128x128.png")) {
		sf::Vector2u texSize = particle_texture.getSize();
		float quad[4][2] = {
			0,									0,
			(float)texSize.x,		0,
			(float)texSize.x,		(float)texSize.y,
		  0,									(float)texSize.y
		};

		for (uint32_t p=0; p<particle_sim.nb_active_part; p++) {
			for (uint8_t i=0; i<4; i++) {
				particle_vertices[4*p+i].texCoords.x = quad[i][0];
				particle_vertices[4*p+i].texCoords.y = quad[i][1];
			}
		}
	}
	else {
		std::cout << "Error loading disk128x128.png" << std::endl;
	}

	// Initializing Segment VertexArray
	segment_vertices.setPrimitiveType(sf::Lines);
	segment_vertices.resize(2*particle_sim.nb_active_seg);
}

Renderer::~Renderer() {
	std::cout << "Renderer::~Renderer()" << std::endl;
	particle_vertices.clear();
	segment_vertices.clear();
}


void Renderer::update_display() {
// std::cout << "Renderer::update_display" << std::endl;
	window.setView(worldView);
	window.clear(background);
	// particle_sim.particle_mutex.lock();
	update_particle_vertices();
	update_segment_vertices();
	// particle_sim.particle_mutex.unlock();
	window.draw(particle_vertices, &particle_texture);
	window.draw(segment_vertices);
	window.display();
}


void Renderer::update_particle_vertices() {
	// std::cout << "Renderer::draw_particles" << std::endl;
	// float r = particle_sim.radii * 2 * sqrt(3);
	// float triangle[3][2] = {
	// 	 0,										 -(float)(particle_sim.radii * 2 / sqrt(3)),
	// 	 particle_sim.radii,		(float)(particle_sim.radii / sqrt(3)),
	// 	-particle_sim.radii,		(float)(particle_sim.radii / sqrt(3))
	// };
	float pr = 1; // padding_ratio
	float quad[4][2] = {
		-particle_sim.radii *pr,	-particle_sim.radii *pr,
		 particle_sim.radii *pr,	-particle_sim.radii *pr,
		 particle_sim.radii *pr,	 particle_sim.radii *pr,
		-particle_sim.radii *pr,	 particle_sim.radii *pr,
	};
	
	for (uint32_t p=0; p<particle_sim.nb_active_part; p++) {
		for (uint8_t i=0; i<4; i++) {
			particle_vertices[4*p+i].position.x = particle_sim.particle_array[p].position[0] + quad[i][0];
			particle_vertices[4*p+i].position.y = particle_sim.particle_array[p].position[1] + quad[i][1];

			particle_vertices[4*p+i].color.r = particle_sim.particle_array[p].colour[0];
			particle_vertices[4*p+i].color.g = particle_sim.particle_array[p].colour[1];
			particle_vertices[4*p+i].color.b = particle_sim.particle_array[p].colour[2];
			particle_vertices[4*p+i].color.a = particle_sim.particle_array[p].colour[3];

			// particle_vertices[4*p+i].color.a = 150;
		}
	}
}

void Renderer::update_segment_vertices() {
	for (uint32_t s=0; s<particle_sim.nb_active_seg; s++) {
		segment_vertices[2*s  ].position.x = particle_sim.seg_array[s].pos[0][0];
		segment_vertices[2*s  ].position.y = particle_sim.seg_array[s].pos[0][1];
		segment_vertices[2*s+1].position.x = particle_sim.seg_array[s].pos[1][0];
		segment_vertices[2*s+1].position.y = particle_sim.seg_array[s].pos[1][1];
	}
}