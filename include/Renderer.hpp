#pragma once

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Particle_simulator.hpp"

/**
* Handles everything relative to displaying (vertices, views, shaders)
*/
class Renderer {
private :
	sf::RenderWindow window;
	sf::View worldView; //< View to display objects relative to the simulation
	// sf::View GUIView; //< View to display objects relative to the GUI

	Particle_simulator& particle_sim;
	sf::VertexArray particle_vertices;
	sf::Texture particle_texture;
	sf::VertexArray segment_vertices;
	sf::VertexArray worldGrid_vertices;

	sf::Color background = sf::Color::Black;

	bool liquid_shader = false;
	sf::Shader particle_shader;
	sf::Glsl::Vec4 particle_color = sf::Glsl::Vec4(0.25f, 0.88f, 0.88f, 1.f); //< Colour to apply on all Particles should their individual colour be ignored (e.g. liquid shader is used)
	sf::Shader segment_shader;

	bool fullscreen = false;
	bool Vsync = false;

public :
	/**
	* @brief Constructor. Initializes VertexArrays, loads textures, sets shader uniforms
	*/
	Renderer(Particle_simulator& particle_sim_);
	~Renderer();

	uint16_t FPS_limit = 60;
	void slowFPS(bool slow) {FPS_limit = slow ? 30 : 60;};

	/**
	* @brief Updates then display vertices, using shaders
	* @see update_particle_vertices
	* @see update_segment_vertices
	*/
	void update_display();

	/**
	* @brief Updates the Particles' vertices according to their positions.
	* @warning Reads particle_sim's particles positions while they might be updating in another thread. But this is read-only and never caused any problem.
	* I am keeping this without a mutex lock for performance reasons. 
	*/
	void update_particle_vertices();

	/**
	* @brief Updates the Segments' vertices according to their positions.
	* @warning Reads particle_sim's segments positions while they might be updating in another thread. But this is read-only and never caused any problem.
	* I am keeping this without a mutex lock for performance reasons. 
	*/
	void update_segment_vertices();

	/**
	* @brief Updates the grid's vertices according to their positions.
	* @warning Reads particle_sim.world.grid's cells contenance while they might be updating in another thread. But this is read-only and never caused any problem.
	* I am keeping this without a mutex lock for performance reasons. 
	*/
	void update_grid_vertices_init();
	void update_grid_vertices_colour();

	inline sf::View& getworldView() {return worldView;};
	// inline sf::View& getUIView() {return UIView;};

	bool getVSync() {return Vsync;};
	sf::RenderWindow& getWindow() {return window;};
	/**
	* @brief Create/recreate the window and sets its default parameters.
	*/
	void create_window();
	void toggleFullScreen();

	/**
	* @brief Takes a screenshot and saves it as result_images/screenshot.png .
	*/
	void takeScreenShot();

	bool dp_particles = true;
	bool dp_segments = true;
	bool dp_worldGrid = false;
	void toggle_grid();
};