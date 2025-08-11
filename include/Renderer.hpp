#pragma once

#include "Consometer.hpp"
#include "Particle_simulator.hpp"

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

/**
* Handles everything relative to displaying (vertices, views, shaders)
*/
class Renderer {
private :
	sf::RenderWindow window;
	sf::View worldView; //< View to display objects relative to the simulation
	sf::View GUIView; //< View to display objects relative to the GUI

	Particle_simulator& particle_sim;
	sf::VertexArray particle_vertices;
	sf::Texture particle_texture;
	sf::VertexArray segment_vertices;
	sf::VertexArray worldGrid_vertices;
	sf::VertexArray zone_vertices;
	sf::RectangleShape world_vertices; //< The rectangle of the world
	sf::CircleShape user_interact_zone;

	sf::Font font;
	sf::Text FPS_display;
	sf::Color background = sf::Color::Black;

	bool liquid_shader = false;
	sf::Shader particle_shader;
	sf::Glsl::Vec4 particle_color = sf::Glsl::Vec4(0.25f, 0.88f, 0.88f, 1.f); //< Colour to apply on all Particles should their individual colour be ignored (e.g. liquid shader is used)
	sf::Shader segment_shader;

	Consometre display_time;

	// ======== PARAMETERS ========
private :
	bool fullscreen;
	bool Vsync;
	uint16_t FPS_limit_default;

public :
	void setDefaultParameters();
	float radius_multiplier; //< For Particles : multiplier on the display of Particles' radii compared to their actual radii.
	float colour_momentum; //< For Particles : how much of the previous colour is kept.
	float speed_colour_rate; //< For Particles: how much speed is necessary to reach the same colour/brightness

	bool enable_displaying; //< To use when you want to make the simulation run while you are away.
	bool dp_particles;
	bool dp_speed; //< Whether Particles should be colored based on their speed.
	bool dp_segments;
	bool dp_worldGrid;
	bool dp_worldBorder;
	bool dp_worldZones;
	bool dp_FPS;
	// ======== END PARAMETERS ========

	// I don't really consider this a parameter as its more about showing the state of the interaction
	bool interacting = false;

public :
	/**
	* @brief Constructor. Initializes VertexArrays, loads textures, sets shader uniforms
	*/
	Renderer(Particle_simulator& particle_sim_);
	~Renderer();

	uint16_t FPS_limit;
	void slowFPS(bool slow) {FPS_limit = slow ? 30 : FPS_limit_default;};

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
	* @brief Updates the grid's vertices according to their positions. You should only need to call this once.
	* @warning Reads particle_sim.world.grid's cells contenance while they might be updating in another thread. But this is read-only and never caused any problem.
	* I am keeping this without a mutex lock for performance reasons. 
	*/
	void update_grid_vertices_init();
	/**
	* @brief Updates the grid's vertices according to their contenance. This is what should be called in a loop.
	* @warning Reads particle_sim.world.grid's cells contenance while they might be updating in another thread. But this is read-only and never caused any problem.
	* I am keeping this without a mutex lock for performance reasons. 
	*/
	void update_grid_vertices_colour();

	/**
	* @brief Updates (i.e. gives position and colour of) the world's zone vertices.
	*/
	void update_zone_vertices();

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
	* @brief Changes @see dp_speed so that if the Particle_simulatior is loading Particle positions but not speed, Renderer won't try to display Particle speed.
	*/
	void should_use_speed();

	/**
	* @brief Takes a screenshot and saves it as result_images/screenshot.png .
	*/
	void takeScreenShot();

	Particle* followed = nullptr; //< Particle that is being followed (i.e. that the camera stays centered around).
	
	/**
	* @brief Toggles the displaying of the grid.
	* @details To start displaying the World's grid, Renderer fills the vertex array @see worldGrid_vertices.
	* When displaying of the grid stops, the memory can be freed.
	* @warning In large worlds with lots of Cells in their grid, displaying the grid can slow down the display loop a lot or even crash the program if too much memory is asked from the GPU.
	*/
	void toggle_grid();
};