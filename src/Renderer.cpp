#include <cmath>
#include <iostream>

#include "Renderer.hpp"
#include "Particle_simulator.hpp"
#include "World.hpp"


Renderer::Renderer(Particle_simulator& particle_sim_) : 
	particle_sim(particle_sim_)
{
	std::cout << "Renderer::Renderer()" << std::endl;
	create_window();
	worldView = window.getDefaultView();

	// Initializing Particle VertexArray
	particle_vertices.setPrimitiveType(sf::Quads);
	particle_vertices.resize(4*NB_PART);

	if (particle_texture.loadFromFile("disk128x128.png")) {
		sf::Vector2u texSize = particle_texture.getSize();
		float quad[4][2] = {
			0,									0,
			(float)texSize.x,		0,
			(float)texSize.x,		(float)texSize.y,
		  0,									(float)texSize.y
		};

		for (uint32_t p=0; p<NB_PART; p++) {
			for (uint8_t i=0; i<4; i++) {
				particle_vertices[4*p+i].texCoords.x = quad[i][0];
				particle_vertices[4*p+i].texCoords.y = quad[i][1];
			}
		}
	}
	else {
		std::cout << "\tError loading disk128x128.png" << std::endl;
	}

	// Initializing Segment VertexArray
	segment_vertices.setPrimitiveType(sf::Lines);
	segment_vertices.resize(2*particle_sim.get_active_seg());

	worldGrid_vertices.setPrimitiveType(sf::Quads);
	if (dp_worldGrid) {
		worldGrid_vertices.resize(4* particle_sim.world.getGridSize(0) * particle_sim.world.getGridSize(1));
		update_grid_vertices_init();
	}

	// loading shaders
	if (!sf::Shader::isAvailable()) {
		std::cout << "\tRenderer : Shaders are not available on this computer" << std::endl;
	} else {
		if (liquid_shader) {
			if (particle_shader.loadFromFile("shader/base_shader.vert", "shader/liquid_shader.frag")) {
				// particle_shader.setUniform("texture", particle_texture);
				particle_shader.setUniform("color", particle_color);
			}
		} else {
			if (particle_shader.loadFromFile("shader/base_shader.vert", "shader/base_shader.frag")) {
				// particle_shader.setUniform("texture", particle_texture);
			}
		}
		if (segment_shader.loadFromFile("shader/base_shader.vert", sf::Shader::Vertex)) {
			
		}
	}
}

Renderer::~Renderer() {
	std::cout << "Renderer::~Renderer()" << std::endl;
	particle_vertices.clear();
	segment_vertices.clear();
}


void Renderer::update_display() {
// std::cout << "Renderer::update_display" << std::endl;
	sf::RenderStates state;

	window.setView(worldView);
	window.clear(background);

	if (dp_particles) {
		update_particle_vertices();
		state.texture = &particle_texture;
		state.shader = &particle_shader;
	}
	if (dp_segments) update_segment_vertices();
	if (dp_worldGrid) {
		update_grid_vertices_colour();
		window.draw(worldGrid_vertices);
	}
	if (dp_particles) window.draw(particle_vertices, state);
	if (dp_segments) window.draw(segment_vertices, &segment_shader);
	window.display();
}


void Renderer::update_particle_vertices() {
	// std::cout << "Renderer::draw_particles" << std::endl;
	float pr = liquid_shader ? 8 : 1.05; // padding_ratio
	float quad[4][2] = {
		-particle_sim.radii *pr,	-particle_sim.radii *pr,
		 particle_sim.radii *pr,	-particle_sim.radii *pr,
		 particle_sim.radii *pr,	 particle_sim.radii *pr,
		-particle_sim.radii *pr,	 particle_sim.radii *pr,
	};
	
	for (uint32_t p=0; p<particle_sim.get_active_part(); p++) {
		float norm = sqrt(particle_sim.particle_array[p].speed[0]*particle_sim.particle_array[p].speed[0] + particle_sim.particle_array[p].speed[1]*particle_sim.particle_array[p].speed[1]);
		norm = std::max(0.f, norm-4);
		for (uint8_t i=0; i<4; i++) {
			particle_vertices[4*p+i].position.x = particle_sim.particle_array[p].position[0] + quad[i][0];
			particle_vertices[4*p+i].position.y = particle_sim.particle_array[p].position[1] + quad[i][1];

			if (!liquid_shader) {
				// particle_vertices[4*p+i].color.r = particle_sim.particle_array[p].colour[0];
				// particle_vertices[4*p+i].color.g = particle_sim.particle_array[p].colour[1];
				// particle_vertices[4*p+i].color.b = particle_sim.particle_array[p].colour[2];
				particle_vertices[4*p+i].color.r = 255* std::min(norm/100, 1.f);
				particle_vertices[4*p+i].color.g = 255* std::min(norm/400, 1.f);
				particle_vertices[4*p+i].color.b = 255* std::min(norm/800, 1.f);
			}
			// particle_vertices[4*p+i].color.a = particle_sim.particle_array[p].colour[3];
			particle_vertices[4*p+i].color.a = 255;
		}
	}
	for (uint32_t p=particle_sim.get_active_part(); p<NB_PART; p++) {
		for (uint8_t i=0; i<4; i++) {
			particle_vertices[4*p+i].color.a = 0;
		}
	}
}

void Renderer::update_segment_vertices() {
	for (uint32_t s=0; s<particle_sim.get_active_seg(); s++) {
		segment_vertices[2*s  ].position.x = particle_sim.seg_array[s].pos[0][0];
		segment_vertices[2*s  ].position.y = particle_sim.seg_array[s].pos[0][1];
		segment_vertices[2*s+1].position.x = particle_sim.seg_array[s].pos[1][0];
		segment_vertices[2*s+1].position.y = particle_sim.seg_array[s].pos[1][1];
	}
}

void Renderer::update_grid_vertices_init() {
	World& world = particle_sim.world;
	uint32_t index = 0;
	for (uint16_t y=0; y<world.getGridSize(1); y++) {
		for (uint16_t x=0; x<world.getGridSize(0); x++) {
			Cell& cell = world.getCell(x, y);

			// position
			worldGrid_vertices[4*index  ].position.x = x *    world.getCellSize(0);
			worldGrid_vertices[4*index  ].position.y = y *    world.getCellSize(1);
			worldGrid_vertices[4*index+1].position.x = (x+1)* world.getCellSize(0);
			worldGrid_vertices[4*index+1].position.y = y *    world.getCellSize(1);
			worldGrid_vertices[4*index+2].position.x = (x+1)* world.getCellSize(0);
			worldGrid_vertices[4*index+2].position.y = (y+1)* world.getCellSize(1);
			worldGrid_vertices[4*index+3].position.x = x *    world.getCellSize(0);
			worldGrid_vertices[4*index+3].position.y = (y+1)* world.getCellSize(1);

			// colour
			worldGrid_vertices[4*index   ].color = sf::Color::Black;
			worldGrid_vertices[4*index +1].color = sf::Color::Black;
			worldGrid_vertices[4*index +2].color = sf::Color::Black;
			worldGrid_vertices[4*index +3].color = sf::Color::Black;
			index++;
		}
	}
}

void Renderer::update_grid_vertices_colour() {
	sf::Color color_tab[MAX_PART_CELL+1] = {sf::Color::Black, sf::Color::Green, sf::Color::Yellow, sf::Color::Red, sf::Color::Magenta};
	sf::Color error_color = sf::Color::White;
	World& world = particle_sim.world;
	uint32_t index = 0;
	for (uint16_t y=0; y<world.getGridSize(1); y++) {
		for (uint16_t x=0; x<world.getGridSize(0); x++) {
			Cell& cell = world.getCell(x, y);
			uint8_t nb_parts = cell.nb_parts.load();
			if (nb_parts < MAX_PART_CELL+1 && cell.nb_segs < MAX_SEG_CELL+1) {
				worldGrid_vertices[4*index   ].color = color_tab[nb_parts];
				worldGrid_vertices[4*index +1].color = cell.nb_segs ? sf::Color::Blue : color_tab[nb_parts];
				worldGrid_vertices[4*index +2].color = color_tab[nb_parts];
				worldGrid_vertices[4*index +3].color = cell.nb_segs ? sf::Color::Blue : color_tab[nb_parts];
			} else {
				// std::cout << "Render detects cell overloading " << x << ", " << y << "  :  parts=" << (short)nb_parts << ",  nb_segs=" << (short)cell.nb_segs << std::endl;
				worldGrid_vertices[4*index   ].color = error_color;
				worldGrid_vertices[4*index +1].color = cell.nb_segs ? sf::Color::Blue : error_color;
				worldGrid_vertices[4*index +2].color = error_color;
				worldGrid_vertices[4*index +3].color = cell.nb_segs ? sf::Color::Blue : error_color;
			}
			index++;
		}
	}
}


void Renderer::create_window() {
	window.create(sf::VideoMode::getDesktopMode(), "Particle sim", fullscreen ? sf::Style::Fullscreen : sf::Style::Default);
	window.setVerticalSyncEnabled(Vsync);
	window.setKeyRepeatEnabled(false);
}

void Renderer::toggleFullScreen() {
	fullscreen = !fullscreen;
	create_window();
}

void Renderer::takeScreenShot() {
	sf::Vector2u windowSize = window.getSize();
	sf::Texture texture;
	texture.create(windowSize.x, windowSize.y);
	texture.update(window);
	sf::Image screenshot = texture.copyToImage();
	screenshot.saveToFile("result_images/screenshot.png");
}


void Renderer::toggle_grid() {
	dp_worldGrid = !dp_worldGrid;

	if (dp_worldGrid) {
		worldGrid_vertices.resize(4* particle_sim.world.getGridSize(0) * particle_sim.world.getGridSize(1));
		update_grid_vertices_init();
	} else {
		worldGrid_vertices.resize(0);
	}
}