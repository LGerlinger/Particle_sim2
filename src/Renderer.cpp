#include "Renderer.hpp"
#include "SaveLoader.hpp"
#include "Segment.hpp"
#include "World.hpp"


#if OPENGL_INCLUDE_SUCCESS
// CODE USING OpenGL FOR DISPLAYING

// Defining Classes attributes :
std::vector<Attribute> particle_attr = {
	Attribute(OPGL::Type::FLOAT, 2),
	Attribute(OPGL::Type::FLOAT, 2),
};
std::vector<Attribute> cell_attr = {
	Attribute(OPGL::Type::UNSIGNED_INT, 1),
};
std::vector<Attribute> default_attr = { // just 2D position for when nothing else is really needed
	Attribute(OPGL::Type::FLOAT, 2),
};

// VertexArray(const std::vector<Attribute>& attribute_list, OPGL::DrawUse usage_, OPGL::Primitive primitive_, uint64_t number_of_vertices=0, void* vertices_=nullptr, size_t mem_size_vertices = 0)

Renderer::Renderer(Particle_simulator& particle_sim_) : 
	particle_sim(particle_sim_)
{
	std::cout << "Renderer::Renderer()\n\tUsing OpenGL" << std::endl;
	setDefaultParameters();
	create_window();
	setHomeView();
	OPGL::initializeOpenGLfunctions(); // OpenGL function initialization requires a context (window) to have been created. 

	// Setting up vertices
	particle_vertices.set(particle_attr, OPGL::DrawUse::DYNAMIC_DRAW, OPGL::Primitive::POINTS, particle_sim_.get_max_part(), nullptr, sizeof(Particle) * particle_sim_.get_max_part());
	segment_vertices.set(default_attr, OPGL::DrawUse::STATIC_DRAW, OPGL::Primitive::LINES, 8+ 2*particle_sim.world.seg_array.size());
	worldGrid_vertices.set(cell_attr, OPGL::DrawUse::DYNAMIC_DRAW, OPGL::Primitive::POINTS);
	zone_vertices.set(default_attr, OPGL::DrawUse::STATIC_DRAW, OPGL::Primitive::TRIANGLES, 6*particle_sim.world.getNbOfZones());
	user_interact_zone.set(default_attr, OPGL::DrawUse::DYNAMIC_DRAW, OPGL::Primitive::LINE_LOOP, 30);

	if (dp_worldGrid) {
		worldGrid_vertices.setupMemory(particle_sim.world.getGridSize(0) * particle_sim.world.getGridSize(1));
	}

	// Some vertices won't change during the life of the program. So we set and send them to the GPU for later use.
	update_static_vertices();
	
	// loading shaders
	if (!sf::Shader::isAvailable()) {
		std::cout << "\tRenderer : Shaders are not available on this computer" << std::endl;
	} else {
		if (particle_shader.loadFromFile("shader/particle.vert.glsl", "shader/particle.frag.glsl")) {
			particle_shader.setUniform("max_speed", speed_colour_rate);
		}
		if (worldGrid_shader.loadFromFile("shader/cell.vert.glsl", "shader/cell.frag.glsl")) {
			worldGrid_shader.setUniform("coord0", sf::Glsl::Vec2(particle_sim.world.getCellSize(0)/2, particle_sim.world.getCellSize(1)/2));
			worldGrid_shader.setUniform("cellSize", particle_sim.world.getCellSize(0));
			OPGL::sendUnsignedUniform(worldGrid_shader.getNativeHandle(), "cellsPerRows", particle_sim.world.getGridSize(0));
			OPGL::sendUnsignedUniform(worldGrid_shader.getNativeHandle(), "maxPartPerCell", MAX_PART_CELL);
			OPGL::sendUnsignedUniform(worldGrid_shader.getNativeHandle(), "maxSegPerCell", MAX_SEG_CELL);
			worldGrid_shader.setUniform("error_color",      sf::Glsl::Vec4(1.f, 0.f, 0.f, 1.f));
			worldGrid_shader.setUniform("saturation_color", sf::Glsl::Vec4(1.f, 1.f, 1.f, 1.f));
		}
		if (default_shader.loadFromFile("shader/default.vert.glsl", "shader/default.frag.glsl")) {
			// default_shader.setUniform("colour", sf::Glsl::Vec4(0.f, 0.f, 1.f, 1.f));
		}
	}

	// Counting time
	display_time.start_perf_check("Display time", 10000);
}


Renderer::~Renderer() = default;


void Renderer::setDefaultParameters() {
	fullscreen = false;
	Vsync = false;
	FPS_limit_default = 60;
	FPS_limit = FPS_limit_default;
	
	radius_multiplier = liquid_shader ? 8 : 1;
	colour_momentum = 0.67f;
	speed_colour_rate = 100;

	enable_displaying = true;
	dp_particles = true;
	dp_speed = false;
	dp_segments = true;
	dp_worldGrid = false;
	dp_worldBorder = true;
	dp_worldZones = false;
	dp_FPS = false;
	dp_interaction = true;
	regular_clear = true;
	clear = false;
}


void Renderer::update_display() {
// std::cout << "Renderer::update_display" << std::endl;
	if (particle_sim.isLoading()) {
		if (particle_sim.load_next_positions()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(15));
		}
	}
	
	if (enable_displaying) {
		display_time.Start();

		if (followed) {
			changedView = true;
			worldView.setCenter(sf::Vector2f(followed->position[0], followed->position[1]));
		}

		if (changedView) updatedView();
		if (regular_clear || clear) glClear(GL_COLOR_BUFFER_BIT);

		if (dp_worldGrid && !particle_sim.isLoading()) {
			glPointSize(worldGrid_ds);
			update_grid_vertices_colour();
			worldGrid_vertices.updateAndDraw(worldGrid_shader.getNativeHandle());
		}
		if (dp_worldZones) {
			default_shader.setUniform("color", sf::Glsl::Vec4(0.f, 0.f, 1.f, 0.25f));
			zone_vertices.draw(default_shader.getNativeHandle());
		}
		if (dp_interaction && interacting) {
			update_interaction_vertices();
			default_shader.setUniform("color", sf::Glsl::Vec4(1.f, 1.f, 1.f, 1.f));
			user_interact_zone.updateAndDraw(default_shader.getNativeHandle());
		}
		if (dp_particles) {
			glPointSize(particles_ds);
			particle_vertices.updateAndDraw(particle_shader.getNativeHandle(), 0, particle_sim.get_active_part(), (void*)particle_sim.get_particle_data());
		}
		if (dp_segments || dp_worldBorder) {
			default_shader.setUniform("color", sf::Glsl::Vec4(1.f, 1.f, 1.f, 1.f));
			// glLineWidth(1);
			segment_vertices.draw(default_shader.getNativeHandle(), dp_worldBorder ? 0 : 8, dp_segments ? segment_vertices.nVert() : 8);
		}
		
		// if (dp_FPS) {
		// 	window.setView(GUIView);
		// 	window.draw(FPS_display);
		// }
		// window.setView(worldView);

		window.display();
		display_time.Tick_fine(true);

		// if (dp_FPS && time) {
		// 	std::stringstream oss;
		// 	oss << std::fixed << std::setprecision(3);
		// 	oss << "Display time  : " << time << " ms\n";
		// 	oss << (particle_sim.isLoading() ? "Loading time  : " : "Sim loop time : ") << particle_sim.get_average_loop_time() << " ms\n";
		// 	oss << "Particles     : " << particle_sim.get_active_part() << '\n';
		// 	oss << "time          : " << particle_sim.get_time();
		// 	FPS_display.setString(oss.str());
		// }
	}
}


void Renderer::update_segment_vertices() {
	std::vector<Segment>& seg_array = particle_sim.world.seg_array;

	// Seting up world border
	segment_vertices.get<sf::Glsl::Vec2>(0) = sf::Glsl::Vec2(0, 0);
	segment_vertices.get<sf::Glsl::Vec2>(1) = sf::Glsl::Vec2(particle_sim.world.getSize(0), 0);
	segment_vertices.get<sf::Glsl::Vec2>(2) = sf::Glsl::Vec2(particle_sim.world.getSize(0), 0);
	segment_vertices.get<sf::Glsl::Vec2>(3) = sf::Glsl::Vec2(particle_sim.world.getSize(0), particle_sim.world.getSize(1));
	segment_vertices.get<sf::Glsl::Vec2>(4) = sf::Glsl::Vec2(particle_sim.world.getSize(0), particle_sim.world.getSize(1));
	segment_vertices.get<sf::Glsl::Vec2>(5) = sf::Glsl::Vec2(0, particle_sim.world.getSize(1));
	segment_vertices.get<sf::Glsl::Vec2>(6) = sf::Glsl::Vec2(0, particle_sim.world.getSize(1));
	segment_vertices.get<sf::Glsl::Vec2>(7) = sf::Glsl::Vec2(0, 0);

	// Setting up segments
	for (uint32_t s=0; s<seg_array.size(); s++) {
		sf::Glsl::Vec2* ref = (sf::Glsl::Vec2*)segment_vertices[2*s +8];
		ref->x = seg_array[s].pos[0][0];
		ref->y = seg_array[s].pos[0][1];
		ref = (sf::Glsl::Vec2*)segment_vertices[2*s +9];
		ref->x = seg_array[s].pos[1][0];
		ref->y = seg_array[s].pos[1][1];
	}
}


void Renderer::update_grid_vertices_colour() {
	World& world = particle_sim.world;
	uint16_t viewRectangle[2][2] = { // visible cells rectangle
		(uint16_t)std::max(0,                     (int32_t)((worldView.getCenter().x - worldView.getSize().x/2) / world.getCellSize(0)) -1 ),
		(uint16_t)std::max(0,                     (int32_t)((worldView.getCenter().y - worldView.getSize().y/2) / world.getCellSize(1)) -1 ),
		          std::min(world.getGridSize(0), (uint16_t)((worldView.getCenter().x + worldView.getSize().x/2) / world.getCellSize(0) +1) ),
		          std::min(world.getGridSize(1), (uint16_t)((worldView.getCenter().y + worldView.getSize().y/2) / world.getCellSize(1) +1) )
	};
	if (world.gsm_trylock()) {
		uint32_t index = 0;
		uint8_t* data;
		for (uint16_t y=viewRectangle[0][1]; y<viewRectangle[1][1]; y++) {
			for (uint16_t x=viewRectangle[0][0]; x<viewRectangle[1][0]; x++) {
				uint8_t nb_parts = world.getCell(x, y).nb_parts.load();
				uint8_t nb_segs = world.sig() ? world.getCell_seg(x, y).nb_segs : 0;
				index = (uint32_t)y*(uint32_t)world.getGridSize(0) + (uint32_t)x;

				data = (uint8_t*)worldGrid_vertices[index];
				data[0] = nb_parts;
				data[1] = nb_segs;
			}
		}
		
		if (!world.sig()) {
			uint8_t i=0;
			for (auto& seg : world.seg_array) {
				for (auto& coord : seg.cells) {
					index = (uint32_t)coord[1]*world.getGridSize(0) + coord[0];

					data = (uint8_t*)worldGrid_vertices[index];
					data[1] = MAX_SEG_CELL <= data[1] ? MAX_SEG_CELL : data[1]+1;
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
		Vind = 6*i;
		zone_vertices.get<sf::Glsl::Vec2>(Vind   ) = sf::Glsl::Vec2(zone.pos(0),    zone.pos(1));
		zone_vertices.get<sf::Glsl::Vec2>(Vind +1) = sf::Glsl::Vec2(zone.endPos(0), zone.pos(1));
		zone_vertices.get<sf::Glsl::Vec2>(Vind +2) = sf::Glsl::Vec2(zone.pos(0),    zone.endPos(1));
		zone_vertices.get<sf::Glsl::Vec2>(Vind +3) = sf::Glsl::Vec2(zone.endPos(0), zone.endPos(1));
		zone_vertices.get<sf::Glsl::Vec2>(Vind +4) = sf::Glsl::Vec2(zone.pos(0),    zone.endPos(1));
		zone_vertices.get<sf::Glsl::Vec2>(Vind +5) = sf::Glsl::Vec2(zone.endPos(0), zone.pos(1));
	}
}


void Renderer::update_interaction_vertices() {
	float theta = 0;
	uint8_t nb_points = user_interact_zone.nVert();
	for (uint8_t i=0; i < nb_points; i++) {
		user_interact_zone.get<sf::Glsl::Vec2>(i) = sf::Glsl::Vec2(
			particle_sim.user_point[0] + std::cos(theta)*particle_sim.params.range,
			particle_sim.user_point[1] + std::sin(theta)*particle_sim.params.range
		);
		theta += 2*M_PIf / nb_points;
	}
}

void Renderer::update_static_vertices() {
	update_zone_vertices(); // Setting up data CPU side
	zone_vertices.updateVertices(); // Sending data to GPU
	update_segment_vertices();
	segment_vertices.updateVertices();
}


void Renderer::updatedView() {
	changedView = false;
	window.setView(worldView);
	const sf::Glsl::Mat4& mat = sf::Glsl::Mat4(worldView.getTransform());
	particle_shader.setUniform("modelViewMat", mat);
	worldGrid_shader.setUniform("modelViewMat", mat);
	default_shader.setUniform("modelViewMat", mat);

	particles_ds = std::max(radius_multiplier * particle_sim.params.radii *2       *  window.getSize().x / worldView.getSize().x, 0.5f);
	worldGrid_ds = std::ceil(particle_sim.world.getCellSize(0) *  window.getSize().x / worldView.getSize().x);
}


void Renderer::toggle_grid() {
	if (!particle_sim.isLoading()) {
		
		dp_worldGrid = !dp_worldGrid;
		if (dp_worldGrid) {
			worldGrid_vertices.setupMemory(particle_sim.world.getGridSize(0) * particle_sim.world.getGridSize(1));
		} else {
			worldGrid_vertices.freeMemory();
		}
		
	}
}











#else // OPENGL_INCLUDE_SUCCESS
// CODE USING SFML FOR DISPLAYING

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

Renderer::Renderer(Particle_simulator& particle_sim_) : 
	particle_sim(particle_sim_)
{
	std::cout << "Renderer::Renderer()\n\tUsing SFML" << std::endl;
	setDefaultParameters();
	create_window();
	setHomeView();

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
		init_grid_vertices_pos();
	}

	zone_vertices.setPrimitiveType(sf::Quads);
	zone_vertices.resize(4*particle_sim.world.getNbOfZones());
	update_zone_vertices();

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
			if (particle_shader.loadFromFile("shader/particle-SFML.vert.glsl", "shader/liquid_shader.frag.glsl")) {
				// particle_shader.setUniform("texture", particle_texture);
				particle_shader.setUniform("color", particle_color);
			}
		} else {
			if (particle_shader.loadFromFile("shader/particle-SFML.vert.glsl", "shader/particle-SFML.frag.glsl")) {
				// particle_shader.setUniform("texture", particle_texture);
			}
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
	worldGrid_vertices.clear();
	zone_vertices.clear();
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
	dp_speed = true;
	dp_segments = true;
	dp_worldGrid = false;
	dp_worldBorder = true;
	dp_worldZones = false;
	dp_FPS = true;
	dp_interaction = true;
	regular_clear = true;
	clear = false;
}


void Renderer::update_display() {
// std::cout << "Renderer::update_display" << std::endl;
	display_time.Start();
	if (particle_sim.isLoading()) {
		if (particle_sim.load_next_positions()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(15));
		}
	}
	
	if (enable_displaying) {
		sf::RenderStates state;
		
		if (followed) {
			worldView.setCenter(sf::Vector2f(followed->position[0], followed->position[1]));
			changedView = true;
		}

		if (regular_clear || clear) {
			window.clear(background);
		}

		if (dp_worldBorder) window.draw(world_vertices);
		if (dp_worldGrid && !particle_sim.isLoading()) {
			update_grid_vertices_colour();
			window.draw(worldGrid_vertices);
		}
		if (dp_worldZones) {
			window.draw(zone_vertices);
		}
		if (dp_interaction && interacting) {
			update_interaction_vertices();
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
			window.draw(segment_vertices);
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

			// g=0;
			// b=0;
			// r = std::min(particle_sim.getPressure(p) / 20000, 1.f)*225;
			// // r = particle_sim.getPressure(p) == 0 ? particle_vertices[4*p].color.r : std::min(particle_sim.getPressure(p) / 20000, 1.f)*225 + 30;
			// r = std::min(particle_vertices[4*p].color.r*colour_momentum + r, 255.f);



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
		}
		else {
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

void Renderer::init_grid_vertices_pos() {
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

void Renderer::update_interaction_vertices() {
	user_interact_zone.setPosition(sf::Vector2f(particle_sim.user_point[0] - particle_sim.params.range, particle_sim.user_point[1] - particle_sim.params.range));
}


void Renderer::toggle_grid() {
	if (!particle_sim.isLoading()) {
		
		dp_worldGrid = !dp_worldGrid;
		if (dp_worldGrid) {
			worldGrid_vertices.resize(4* particle_sim.world.getGridSize(0) * particle_sim.world.getGridSize(1));
			init_grid_vertices_pos();
		} else {
			worldGrid_vertices.resize(0);
		}
		
	}
}

#endif // OPENGL_INCLUDE_SUCCESS





void Renderer::create_window() {
	#if OPENGL_INCLUDE_SUCCESS
		// If we use OpenGL we need to use core OpenGL which in turn forces us to define the version.
		sf::ContextSettings settings;
		settings.majorVersion = 4;
		settings.minorVersion = 6;
		settings.attributeFlags = sf::ContextSettings::Core;
		window.setActive(false);
		window.create(sf::VideoMode::getDesktopMode(), "Particle sim", fullscreen ? sf::Style::Fullscreen : sf::Style::Default, settings);
		window.setActive(true); // We will only use my opengl functions to display so it is useless to set and unset active during display.

		// Setting up some OpenGL parameters
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		// glEnable(GL_LINE_SMOOTH);
		glLineWidth(1);
	#else
		window.create(sf::VideoMode::getDesktopMode(), "Particle sim", fullscreen ? sf::Style::Fullscreen : sf::Style::Default);
	#endif
	window.setVerticalSyncEnabled(Vsync);
	window.setKeyRepeatEnabled(false);
	windowSize = window.getSize();

	sf::Image iconImage;
	iconImage.loadFromFile("icon.png");
	window.setIcon(iconImage.getSize().x, iconImage.getSize().y, iconImage.getPixelsPtr());
}

void Renderer::toggleFullScreen() {
	fullscreen = !fullscreen;
	create_window();
	#if OPENGL_INCLUDE_SUCCESS
		particle_vertices.reset(particle_attr);
		segment_vertices.reset(default_attr);
		worldGrid_vertices.reset(cell_attr);
		zone_vertices.reset(default_attr);
		user_interact_zone.reset(default_attr);
		update_static_vertices();
	#endif
}




void Renderer::resizeWindow(uint32_t x, uint32_t y) {
	changedView = true;
	worldView.setCenter(
		worldView.getCenter().x + (x - (float)windowSize.x) * worldView.getSize().x / (2*windowSize.x),
		worldView.getCenter().y + (y - (float)windowSize.y) * worldView.getSize().y / (2*windowSize.y)
	);
	worldView.setSize(
		worldView.getSize().x * x / windowSize.x,
		worldView.getSize().y * y / windowSize.y
	);
	windowSize = sf::Vector2u(x, y);
	#if OPENGL_INCLUDE_SUCCESS
		glViewport(0, 0, x, y);
	#endif
}

void Renderer::setHomeView() {
	changedView = true;
	float worldSize[2] = {
		particle_sim.world.getSize(0),
		particle_sim.world.getSize(1)
	};

	worldView.setCenter(
		worldSize[0] /2,
		worldSize[1] /2
	);
	
	// Calculating the view size considering world size and window proportions
	if (window.getSize().x / worldSize[0] < window.getSize().y / worldSize[1]) {
		worldView.setSize(
			worldSize[0],
			worldSize[0] * window.getSize().y / window.getSize().x
		);
	} else {
		worldView.setSize(
			worldSize[1] * window.getSize().x / window.getSize().y,
			worldSize[1]
		);
	}

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