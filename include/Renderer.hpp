#pragma once

#include "Consometer.hpp"
#include "Particle_simulator.hpp"
#include "VertexArray.hpp"

// Force sfml by setting boolean to 1
# if 0
	#undef OPENGL_INCLUDE_SUCCESS
	#define OPENGL_INCLUDE_SUCCESS 0
#endif

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

/**
* Handles everything relative to displaying (vertices, views, shaders)
*/
class Renderer {
private :
	sf::RenderWindow window;
	sf::Vector2u windowSize; // Window Size in pixels. Needed to know window's size before it has been resized
	sf::View worldView; //< View to display objects relative to the simulation
	sf::View GUIView; //< View to display objects relative to the GUI

	Particle_simulator& particle_sim;

	#if OPENGL_INCLUDE_SUCCESS
		VertexArray particle_vertices;
		VertexArray segment_vertices;
		VertexArray worldGrid_vertices;
		VertexArray zone_vertices;
		VertexArray user_interact_zone;
		// Displaying text is not yet implemented.

		// particle_shader; // also used outside this #if
		sf::Shader default_shader;
		sf::Shader worldGrid_shader;

		float particles_ds; //< Display size of the Particles
		float worldGrid_ds; //< Display size of the World's Cells
	#else
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

		#endif
	sf::Shader particle_shader;

	bool liquid_shader = false;
	sf::Glsl::Vec4 particle_color = sf::Glsl::Vec4(0.25f, 0.88f, 0.88f, 1.f); //< Colour to apply on all Particles should their individual colour be ignored (e.g. liquid shader is used)

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
	bool dp_interaction;
	bool regular_clear; //< Should the screen be cleared before each frame ?
	bool clear; //< Should the screen be cleared before the current frame ?
	// ======== END PARAMETERS ========

	// I don't really consider this a parameter as its more about showing the state of the interaction
	bool interacting = false;
	bool changedView = true; //< Has the view been changed ?

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
	* @see void update_particle_vertices()
	* @see void update_segment_vertices()
	* @see void update_grid_vertices_colour()
	* @see void update_zone_vertices()
	* @see void update_interaction_vertices()
	* @see 
	*/
	void update_display();

	#if !OPENGL_INCLUDE_SUCCESS
	/**
	* @brief Updates the Particles' vertices according to their positions.
	* @warning Reads particle_sim's particles positions while they might be updating in another thread. But this is read-only and never caused any problem.
	* I am keeping this without a mutex lock for performance reasons. 
	*/
	void update_particle_vertices();
	#endif

	/**
	* @brief Updates the Segments' vertices according to their positions.
	* @warning Reads world's segments positions while they might be updating in another thread. But this is read-only and never caused any problem.
	* I am keeping this without a mutex lock for performance reasons. 
	*/
	void update_segment_vertices();

	#if !OPENGL_INCLUDE_SUCCESS
	/**
	* @brief Updates the grid's vertices according to their positions.
	* @details This only needs to be called after initialization or if the grid has moved.
	* @warning Reads world's grid's cells contenance while they might be updating in another thread. But this is read-only and never caused any problem.
	* I am keeping this without a mutex lock for performance reasons. 
	*/
	void init_grid_vertices_pos();
	#endif

	/**
	* @brief Updates the grid's vertices according to their contenance.
	* @details This is what should be called in a loop.
	* @warning Reads world's grid's cells contenance while they might be updating in another thread. But this is read-only and never caused any problem.
	* I am keeping this without a mutex lock for performance reasons. 
	*/
	void update_grid_vertices_colour();

	/**
	* @brief Updates (i.e. gives position and colour (if necessary) of) the world's zone vertices.
	* @details This only needs to be called after initialization or if the zones have changed.
	*/
	void update_zone_vertices();

	/**
	* @brief Updates (i.e. gives position of) the user interaction circle vertices, according to the particle simulator's user point.
	*/
	void update_interaction_vertices();


	#if OPENGL_INCLUDE_SUCCESS
	/**
	* @brief Updates the all vertices that don't change in the life of the program.
	* @details Some objects don't change like the segments or the zones. They are displayed with static vertices recognized with OPGL::DrawUse::STATIC_DRAW.
	* This function calculate their vertices data, and sends it to the GPU to draw later.
	*/
	void update_static_vertices();
	#endif

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
	* @brief Changes the size of the window.
	* @details The view is changed so that objects don't seem to change or move with the window resizing.
	*/
	void resizeWindow(uint32_t x, uint32_t y);
	/**
	* @brief Some things need to be updated when the view is changed. Call updatedView to update those things.
	* @details In the case of OpenGL rendering, the projection matrices and the display size of the points are updated.
	*/
	#if OPENGL_INCLUDE_SUCCESS
	void updatedView();
	#endif
	/**
	* @brief Sets the view so 1) it encompasses the entire world and 2) the projection ratio is equal on both axis (i.e. a square appears square).
	*/
	void setHomeView();

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