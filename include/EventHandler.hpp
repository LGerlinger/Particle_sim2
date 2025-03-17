#pragma once

#include "Particle_simulator.hpp"
#include "Renderer.hpp"

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

/**
* EventHandler allows the user to interact with the window and the simulator
* It handles all events for 1 window and 1 simulator.
*/
class EventHandler {
private :
	sf::Event event;

	Renderer& renderer;
	sf::RenderWindow& window;
	sf::View& worldView;
	// sf::View& GUIView;
	sf::Mouse mouse;
	sf::Keyboard keyboard;
	static const uint8_t MAX_KEYS = 3; //< Max number of keys pressed at once that the EventHandler will register.
	uint8_t n_key_pressed = 0; //< Number of keys pressed at any time. n_key_pressed <= MAX_KEYS
	sf::Keyboard::Key pressed_keys[MAX_KEYS]; //< Used to go over all pressed keys. SFML only gives the possibility of asking if a specific key is pressed.

	Particle_simulator& simulator; //< The simulator with which the handler will interact

	std::vector<uint32_t> selectedPart; //< List of Particles selected by the user
	std::vector<float> selectedPartInitPos; //< List of selected Particles' position before moving them

	bool rightMousePressed = false;
	bool lefttMousePressed = false;
	bool ctrlPressed = false;
	sf::Vector2u initialRightMousePos; //< relative position to screen
	sf::Vector2f initialLeftMousePos; //< world position
	sf::Vector2f initialCenterPos; //< relative to world position. Used for worldview moving.
	sf::Vector2u windowSize; //< Used so I don't have to call window.getSize() at each view change or window resizing
	sf::Vector2f viewSize; //< Used so I don't have to call view.getSize() at each view change or window resizing


public :
	EventHandler(Renderer& renderer, Particle_simulator& simulator_);
	~EventHandler();

	/**
	* @brief Handle all events in event queue (so all since last call).
	*/
	void loopOverEvents();

	/**
	* @brief Adds a key to pressed_keys. Does nothing if the pressed_keys is already full.
	* @see MAX_KEYS
	*/
	void addPressedKey(sf::Keyboard::Key key);
	/**
	* @brief Removes a key to pressed_keys. Does nothing if key isn't in pressed_keys.
	* @details The default "no key" is sf::Keyboard::Unknown
	* @see MAX_KEYS
	*/
	void remPressedKey(sf::Keyboard::Key key);
	inline sf::Keyboard::Key getPressedKey(uint8_t key_num) {return pressed_keys[key_num];};

	/**
	* @brief Moves the position of all selected Particles to follow the mouse (if left mouse is pressed).
	*/
	void update_selection_pos();

	/**
	* @brief Remove all Particle from selection.
	* @details Particles display in their colour whether or not they are selected. So clear_selection will call Particle::select(bool selection) to reflect that.
	*/
	void clear_selection();

	/**
	* @brief Returns the number of the first Particle covering the point [x, y]. Returns NULLPART if none is found.
	* @details This function uses the simulator's world grid because I like it that way. So it cannot find Particles outside the world borders.  
	*/
	uint32_t searchParticle(float x, float y);
};