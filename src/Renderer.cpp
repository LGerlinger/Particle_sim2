#include "Renderer.hpp"
#include "Segment.hpp"
#include "World.hpp"

#include <cmath>
#include <iostream>

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
	segment_vertices.resize(2*particle_sim.world.seg_array.size());

	worldGrid_vertices.setPrimitiveType(sf::Quads);
	if (dp_worldGrid) {
		worldGrid_vertices.resize(4* particle_sim.world.getGridSize(0) * particle_sim.world.getGridSize(1));
		update_grid_vertices_init();
	}

	world_vertices.setPosition(0, 0);
	world_vertices.setSize(sf::Vector2f(particle_sim_.world.getSize(0), particle_sim_.world.getSize(1)));
	world_vertices.setOutlineThickness(2);
	world_vertices.setOutlineColor(sf::Color::White);
	world_vertices.setFillColor(sf::Color::Transparent);

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
	window.draw(world_vertices);
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
	uint8_t r, g, b;
	float color_momentum = 0.3f; // how much of the new colour is taken in
	
	for (uint32_t p=0; p<particle_sim.get_active_part(); p++) {
		float norm = sqrt(particle_sim.particle_array[p].speed[0]*particle_sim.particle_array[p].speed[0] + particle_sim.particle_array[p].speed[1]*particle_sim.particle_array[p].speed[1]);
		norm = std::max(0.f, norm-4);
		norm /= 100;
		r = particle_vertices[4*p].color.r;
		r = r*(1-color_momentum) + color_momentum*255* std::min(norm, 1.f);
		g = particle_vertices[4*p].color.g;
		g = g*(1-color_momentum) + color_momentum*255* std::min(norm/4, 1.f);
		b = particle_vertices[4*p].color.b;
		b = b*(1-color_momentum) + color_momentum*255* std::min(norm/8, 1.f);
		for (uint8_t i=0; i<4; i++) {
			particle_vertices[4*p+i].position.x = particle_sim.particle_array[p].position[0] + quad[i][0];
			particle_vertices[4*p+i].position.y = particle_sim.particle_array[p].position[1] + quad[i][1];
			
			if (!liquid_shader) {
				particle_vertices[4*p+i].color.r = r;
				particle_vertices[4*p+i].color.g = g;
				particle_vertices[4*p+i].color.b = b;
			}
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
	std::vector<Segment>& seg_array = particle_sim.world.seg_array;
	segment_vertices.resize(2*particle_sim.world.seg_array.size());
	for (uint32_t s=0; s<seg_array.size(); s++) {
		segment_vertices[2*s  ].position.x = seg_array[s].pos[0][0];
		segment_vertices[2*s  ].position.y = seg_array[s].pos[0][1];
		segment_vertices[2*s+1].position.x = seg_array[s].pos[1][0];
		segment_vertices[2*s+1].position.y = seg_array[s].pos[1][1];
	}
}

void Renderer::update_grid_vertices_init() {
	World& world = particle_sim.world;
	uint32_t index = 0;
	for (uint16_t y=0; y<world.getGridSize(1); y++) {
		for (uint16_t x=0; x<world.getGridSize(0); x++) {
			// position
			worldGrid_vertices[index   ].position.x = x *    world.getCellSize(0);
			worldGrid_vertices[index   ].position.y = y *    world.getCellSize(1);
			worldGrid_vertices[index +1].position.x = (x+1)* world.getCellSize(0);
			worldGrid_vertices[index +1].position.y = y *    world.getCellSize(1);
			worldGrid_vertices[index +2].position.x = (x+1)* world.getCellSize(0);
			worldGrid_vertices[index +2].position.y = (y+1)* world.getCellSize(1);
			worldGrid_vertices[index +3].position.x = x *    world.getCellSize(0);
			worldGrid_vertices[index +3].position.y = (y+1)* world.getCellSize(1);

			// colour
			worldGrid_vertices[index   ].color = sf::Color::Black;
			worldGrid_vertices[index +1].color = sf::Color::Black;
			worldGrid_vertices[index +2].color = sf::Color::Black;
			worldGrid_vertices[index +3].color = sf::Color::Black;
			index += 4;
		}
	}
}

void Renderer::update_grid_vertices_colour() {
	World& world = particle_sim.world;
	if (world.gsm_trylock()) {
		sf::Color error_color = sf::Color::White;
		uint32_t index = 0;
		sf::Color color;
		for (uint16_t y=0; y<world.getGridSize(1); y++) {
			for (uint16_t x=0; x<world.getGridSize(0); x++) {
				uint8_t nb_parts = world.getCell(x, y).nb_parts.load();
				uint8_t nb_segs = world.sig() ? world.getCell_seg(x, y).nb_segs : 0;
				// std::cout << x << ", " << y << " : segment in grid=" << b2s(world.segments_in_grid) << " donc nb_segs=" << (short)nb_segs << "   nb_parts=" << (short)nb_parts << std::endl;
	
				if (nb_parts < MAX_PART_CELL+1 && nb_segs < MAX_SEG_CELL+1) {
					color.r = 0;
					color.g = 255* ((float)nb_parts / MAX_PART_CELL);
					color.b = 255* ((float)nb_segs / MAX_SEG_CELL);
					// std::cout << "\tcb=" << (short)color.b << std::endl;
					color.a = 255;
	
					// worldGrid_vertices[index   ].color = color;
					worldGrid_vertices[index +1].color = color;
					// worldGrid_vertices[index +2].color = color;
					worldGrid_vertices[index +3].color = color;
				} else {
					// std::cout << "\tRender detects cell overloading " << x << ", " << y << "  :  parts=" << (short)nb_parts << ",  nb_segs=" << (short)nb_segs << std::endl;
					// worldGrid_vertices[index   ].color = error_color;
					worldGrid_vertices[index +1].color = error_color;
					// worldGrid_vertices[index +2].color = error_color;
					worldGrid_vertices[index +3].color = error_color;
				}
				index += 4;
			}
		}
		
		if (!world.sig()) {
			uint8_t i=0;
			for (auto& seg : world.seg_array) {
				for (auto& coord : seg.cells) {
					index = 4*(coord[1]*world.getGridSize(0) + coord[0]);
					for (uint8_t i=1; i<4; i+=2) {
						worldGrid_vertices[index +i].color.b += 255./MAX_SEG_CELL; // The "Add Particle to Vertices" has already put this value to 0 before iterating through the segments
					}
				}
			}
		}

		world.gsm_unlock();
	}
}


void Renderer::create_window() {
	window.create(sf::VideoMode::getDesktopMode(), "Particle sim", fullscreen ? sf::Style::Fullscreen : sf::Style::Default);
	window.setVerticalSyncEnabled(Vsync);
	window.setKeyRepeatEnabled(false);

	sf::Image iconImage;
	iconImage.loadFromFile("icon.png");
	window.setIcon(iconImage.getSize().x, iconImage.getSize().y, iconImage.getPixelsPtr());
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