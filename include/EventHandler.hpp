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

	Particle_simulator& simulator;

	// std::vector<Cat*>& cats;

	// std::vector<Cat*> selectedCat;
	// std::vector<float> selectedCatInitPos;
	//float selectedCatInitPos[2];

	bool catHit = false;
	bool rightMousePressed = false;
	sf::Vector2u initialRightMousePos; // relative position to screen
	sf::Vector2f initialLeftMousePos; // absolute position
	sf::Vector2f initialCenterPos;
	sf::Vector2u windowSize;
	sf::Vector2f viewSize;

	EventOrders orders;


public :
	EventHandler(Renderer& renderer, sf::RenderWindow& window_, Particle_simulator& simulator_);

	void loopOverEvents();
	inline EventOrders& getOrders() { return orders; }

	// Cat* searchCat(float x, float y);
};