#pragma once

#include "Particle_simulator.hpp"
#include "Renderer.hpp"

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>


struct EventOrders {
	bool display;
	bool simulate;
};

class EventHandler {
protected :
	sf::Event event;

	sf::RenderWindow& window;
	sf::View& worldView;
	// sf::View& UIView;
	sf::Mouse mouse;
	sf::Keyboard keyboard;
	static const uint8_t MAX_KEYS = 3;
	uint8_t n_key_pressed = 0;
	sf::Keyboard::Key pressed_keys[MAX_KEYS]; // used to go over all pressed keys. SFML only gives the possibility of asking if a specific key is pressed

	Particle_simulator& simulator;

	std::vector<uint32_t> selectedPart;
	std::vector<float> selectedPartInitPos;

	bool rightMousePressed = false;
	bool lefttMousePressed = false;
	bool ctrlPressed = false;
	sf::Vector2u initialRightMousePos; // relative position to screen
	sf::Vector2f initialLeftMousePos; // world position
	sf::Vector2f initialCenterPos;
	sf::Vector2u windowSize;
	sf::Vector2f viewSize;

	EventOrders orders;


public :
	EventHandler(Renderer& renderer, sf::RenderWindow& window_, Particle_simulator& simulator_);
	~EventHandler();

	void loopOverEvents();
	inline EventOrders& getOrders() { return orders; }
	void addPressedKey(sf::Keyboard::Key key);
	void remPressedKey(sf::Keyboard::Key key);
	sf::Keyboard::Key getPressedKey(uint8_t key_num) {return pressed_keys[key_num];};

	void update_selection_pos();
	void clear_selection();
	uint32_t searchParticle(float x, float y);
};