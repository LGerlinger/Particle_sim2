#include "Renderer.hpp"
#include "SaveLoader.hpp"
#include "Segment.hpp"
#include "World.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

Renderer::Renderer(Particle_simulator& particle_sim_) : 
	particle_sim(particle_sim_)
{
	std::cout << "Renderer::Renderer()" << std::endl;
	setDefaultParameters();
	create_window();
	worldView = window.getDefaultView();

	// Initializing Particle VertexArray
	particle_vertices.setPrimitiveType(sf::Quads);
	particle_vertices.resize(4*particle_sim_.get_max_part());

	if (particle_texture.loadFromFile("disk128x128.png")) {
		sf::Vector2u texSize = particle_texture.getSize();
		float quad[4][2] = {
			0,									0,
			(float)texSize.x,		0,
			(float)texSize.x,		(float)texSize.y,
		  0,									(float)texSize.y
		};

		for (uint32_t p=0; p<particle_sim_.get_max_part(); p++) {
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

	zone_vertices.setPrimitiveType(sf::Quads);
	zone_vertices.resize(4*particle_sim.world.getNbOfZones());
	update_zone_vertices();

	world_vertices.setPosition(0, 0);
	world_vertices.setSize(sf::Vector2f(particle_sim_.world.getSize(0), particle_sim_.world.getSize(1)));
	world_vertices.setOutlineThickness(2);
	world_vertices.setOutlineColor(sf::Color::White);
	world_vertices.setFillColor(sf::Color::Transparent);

	user_interact_zone.setRadius(particle_sim_.params.range);
	user_interact_zone.setOutlineThickness(0.75);
	user_interact_zone.setOutlineColor(sf::Color::White);
	user_interact_zone.setFillColor(sf::Color::Transparent);

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

	if (!font.loadFromFile("VCR_OSD_MONO_1.001.ttf")) std::cout << "error loading VCR_OSD_MONO_1.001.ttf" << std::endl;
	else {
		FPS_display.setFont(font);
		FPS_display.setString("=^w^=");
		FPS_display.setCharacterSize(12);
		// FPS_display.setLineSpacing(2);
		FPS_display.setFillColor(sf::Color::White);
		FPS_display.setPosition(7.f, 10.f);
	}

	// Counting time
	display_time.start_perf_check("Display time", 30);
}

Renderer::~Renderer() {
	std::cout << "Renderer::~Renderer()" << std::endl;
	particle_vertices.clear();
	segment_vertices.clear();
}


void Renderer::setDefaultParameters() {
	fullscreen = false;
	Vsync = false;
	FPS_limit_default = 60;
	FPS_limit = FPS_limit_default;
	
	radius_multiplier = liquid_shader ? 8 : 1;
	colour_momentum = 0.67f;
	speed_colour_rate = 50;

	enable_displaying = true;
	dp_particles = true;
	dp_speed = false;
	dp_segments = true;
	dp_worldGrid = false;
	dp_worldBorder = true;
	dp_worldZones = false;
	dp_FPS = true;
}


void Renderer::update_display() {
// std::cout << "Renderer::update_display" << std::endl;
	display_time.Start();
	if (particle_sim.isDonePosLoading()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(15));
	}
	if (particle_sim.isLoading()) particle_sim.load_next_positions();

	if (enable_displaying) {
		sf::RenderStates state;

		window.clear(background);
		
		if (followed) {
			worldView.setCenter(sf::Vector2f(followed->position[0], followed->position[1]));
		}

		if (dp_worldBorder) window.draw(world_vertices);
		if (dp_worldGrid && !particle_sim.isLoading()) {
			update_grid_vertices_colour();
			window.draw(worldGrid_vertices);
		}
		if (dp_worldZones) {
			window.draw(zone_vertices);
		}
		if (interacting) {
			user_interact_zone.setPosition(sf::Vector2f(particle_sim.user_point[0] - particle_sim.params.range, particle_sim.user_point[1] - particle_sim.params.range));
			window.draw(user_interact_zone);
		}
		if (dp_particles) {
			update_particle_vertices();
			state.texture = &particle_texture;
			state.shader = &particle_shader;
			window.draw(particle_vertices, state);
		}
		if (dp_segments) {
			update_segment_vertices();
			window.draw(segment_vertices, &segment_shader);
		}
		
		if (dp_FPS) {
			window.setView(GUIView);
			window.draw(FPS_display);
		}
		window.setView(worldView);

		window.display();
		float time = display_time.Tick_fine(false);

		if (dp_FPS && time) {
			std::stringstream oss;
			oss << std::fixed << std::setprecision(3);
			oss << "Display time  : " << time << " ms\n";
			oss << (particle_sim.isLoading() ? "Loading time  : " : "Sim loop time : ") << particle_sim.get_average_loop_time() << " ms\n";
			oss << "Particles     : " << particle_sim.get_active_part() << '\n';
			oss << "time          : " << particle_sim.get_time();
			FPS_display.setString(oss.str());
		}
	}
}


void Renderer::update_particle_vertices() {
	float size = particle_sim.params.radii *radius_multiplier;
	float quad[4][2] = {
		-size,	-size,
		 size,	-size,
		 size,	 size,
		-size,	 size,
	};
	uint8_t r, g, b;

	float viewRectangle[2][2] = {
		worldView.getCenter().x - worldView.getSize().x/2 - particle_sim.params.radii,
		worldView.getCenter().y - worldView.getSize().y/2 - particle_sim.params.radii,
		worldView.getCenter().x + worldView.getSize().x/2 + particle_sim.params.radii,
		worldView.getCenter().y + worldView.getSize().y/2 + particle_sim.params.radii,
	};
	
	for (uint32_t p=0; p<particle_sim.get_active_part(); p++) {
		if (viewRectangle[0][0] < particle_sim[p].position[0] && particle_sim[p].position[0] < viewRectangle[1][0] &&
		    viewRectangle[0][1] < particle_sim[p].position[1] && particle_sim[p].position[1] < viewRectangle[1][1])
		{
			if (dp_speed) {
				// r = std::min((0 > particle_sim[p].speed[1]) * std::abs(particle_sim[p].speed[1])/1000 * 255, 255.f);
				// g = std::min((0 < particle_sim[p].speed[1]) * particle_sim[p].speed[1]/1000 * 255, 255.f);
				// b = 0;
				float norm = sqrt(particle_sim[p].speed[0]*particle_sim[p].speed[0] + particle_sim[p].speed[1]*particle_sim[p].speed[1]);
				norm = std::max(20.f, norm);
				norm /= speed_colour_rate;
				r = particle_vertices[4*p].color.r*colour_momentum + (1-colour_momentum)*255* std::min(norm, 1.f);
				g = particle_vertices[4*p].color.g*colour_momentum + (1-colour_momentum)*255* std::min(norm/4, 1.f);
				b = particle_vertices[4*p].color.b*colour_momentum + (1-colour_momentum)*255* std::min(norm/8, 1.f);
			} else {
				r = 255;
				g = 255;
				b = 255;
			}
	
			for (uint8_t i=0; i<4; i++) {
				particle_vertices[4*p+i].position.x = particle_sim[p].position[0] + quad[i][0];
				particle_vertices[4*p+i].position.y = particle_sim[p].position[1] + quad[i][1];
				
				if (!liquid_shader) {
					particle_vertices[4*p+i].color.r = r;
					particle_vertices[4*p+i].color.g = g;
					particle_vertices[4*p+i].color.b = b;
				}
				particle_vertices[4*p+i].color.a = 255;
			}
		} else {
			for (uint8_t i=0; i<4; i++) {
				particle_vertices[4*p+i].color.a = 0;
			}
		}
	}
	for (uint32_t p=particle_sim.get_active_part(); p<particle_sim.get_max_part(); p++) {
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

sf::Color saturation_color = sf::Color::White; // I had enough of redeclaring this each time. Although I don't know if this would have been optimised by the compiler.
sf::Color error_color = sf::Color::Red; // I had enough of redeclaring this each time. Although I don't know if this would have been optimised by the compiler.

void Renderer::update_grid_vertices_colour() {
	World& world = particle_sim.world;
	uint16_t viewRectangle[2][2] = { // visible cells rectangle
		(uint16_t)std::max(0,                     (int32_t)((worldView.getCenter().x - worldView.getSize().x/2) / world.getCellSize(0)) -1 ),
		(uint16_t)std::max(0,                     (int32_t)((worldView.getCenter().y - worldView.getSize().y/2) / world.getCellSize(1)) -1 ),
		          std::min(world.getGridSize(0), (uint16_t)((worldView.getCenter().x + worldView.getSize().x/2) / world.getCellSize(0) +1) ),
		          std::min(world.getGridSize(1), (uint16_t)((worldView.getCenter().y + worldView.getSize().y/2) / world.getCellSize(1) +1) )
	};
	// if (viewRectangle[1][0] < viewRectangle[0][0] || viewRectangle[1][1] < viewRectangle[0][1]) return; // return if we know every cell is outside the view
	if (world.gsm_trylock()) {
		uint32_t index = 0;
		sf::Color color;
		for (uint16_t y=viewRectangle[0][1]; y<viewRectangle[1][1]; y++) {
			for (uint16_t x=viewRectangle[0][0]; x<viewRectangle[1][0]; x++) {
				uint8_t nb_parts = world.getCell(x, y).nb_parts.load();
				uint8_t nb_segs = world.sig() ? world.getCell_seg(x, y).nb_segs : 0;
				index = 4*((uint32_t)y*(uint32_t)world.getGridSize(0) + (uint32_t)x);
	
				if (nb_parts < MAX_PART_CELL && nb_segs < MAX_SEG_CELL) {
					color.r = 0;
					color.g = 255* ((float)nb_parts / MAX_PART_CELL);
					color.b = 255* ((float)nb_segs / MAX_SEG_CELL);
					color.a = 255;
	
					// worldGrid_vertices[index   ].color = color;
					worldGrid_vertices[index +1].color = color;
					// worldGrid_vertices[index +2].color = color;
					worldGrid_vertices[index +3].color = color;
				}
				else if (nb_parts > MAX_PART_CELL || nb_segs > MAX_SEG_CELL) {
					worldGrid_vertices[index   ].color = error_color;
					worldGrid_vertices[index +1].color = error_color;
					worldGrid_vertices[index +2].color = error_color;
					worldGrid_vertices[index +3].color = error_color;
				} else {
					// worldGrid_vertices[index   ].color = saturation_color;
					worldGrid_vertices[index +1].color = saturation_color;
					// worldGrid_vertices[index +2].color = saturation_color;
					worldGrid_vertices[index +3].color = saturation_color;
				}
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

void Renderer::update_zone_vertices() {
	uint32_t Vind=0;
	for (uint16_t i=0; i < particle_sim.world.getNbOfZones(); i++) {
		Zone& zone = particle_sim.world.getZone(i);
		Vind = 4*i;
		zone_vertices[Vind   ].position.x = zone.pos(0);
		zone_vertices[Vind   ].position.y = zone.pos(1);

		zone_vertices[Vind +1].position.x = zone.endPos(0);
		zone_vertices[Vind +1].position.y = zone.pos(1);

		zone_vertices[Vind +2].position.x = zone.endPos(0);
		zone_vertices[Vind +2].position.y = zone.endPos(1);

		zone_vertices[Vind +3].position.x = zone.pos(0);
		zone_vertices[Vind +3].position.y = zone.endPos(1);

		for (uint8_t v=0; v<4; v++) {
			zone_vertices[Vind +v].color = sf::Color::Black;
			zone_vertices[Vind +v].color.a = 64;
			zone_vertices[Vind +v].color.r = 0;
			zone_vertices[Vind +v].color.g = 0;
			zone_vertices[Vind +v].color.b = 255;
		}
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

void Renderer::should_use_speed() {
	dp_speed = !particle_sim.HasNoSpeed();
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
	if (!particle_sim.isLoading()) {
		
		dp_worldGrid = !dp_worldGrid;
		if (dp_worldGrid) {
			worldGrid_vertices.resize(4* particle_sim.world.getGridSize(0) * particle_sim.world.getGridSize(1));
			update_grid_vertices_init();
		} else {
			worldGrid_vertices.resize(0);
		}
		
	}
}