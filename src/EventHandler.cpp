#include "EventHandler.hpp"
#include "Particle_simulator.hpp"
#include <iostream>


EventHandler::EventHandler(Renderer& renderer, sf::RenderWindow& window_, Particle_simulator& simulator_) :
	window(window_), worldView(renderer.getworldView()), simulator(simulator_)
{
	windowSize = window.getSize();
	window.setKeyRepeatEnabled(false);

	worldView = renderer.getworldView();
	viewSize = worldView.getSize();

	orders.display = true;
	orders.simulate = true;
}


void EventHandler::loopOverEvents() {
	float coef;
	sf::Vector2i mousePos;
	sf::Vector2f center;
	float mouseWheelDelta;
	while (window.pollEvent(event))
	{
		switch (event.type) {
		// event type
		case sf::Event::KeyPressed:
			switch (event.key.code) {
			case sf::Keyboard::Key::Escape :
				window.close();
				break;
			case sf::Keyboard::Key::Space :
				simulator.paused = !simulator.paused;
				break;
			case sf::Keyboard::Key::SemiColon :
				simulator.step = true;
				break;
			case sf::Keyboard::Key::Add :
				simulator.dt *= 2;
				break;
			case sf::Keyboard::Key::Subtract :
				simulator.dt /= 2;
				break;
			// case sf::Keyboard::Key::S:
			//	 if (ctrlPressed) sl.savePos();
			//	 break;
			// case sf::Keyboard::Key::C:
			//	 sl.loadPos();
			//	 break;
			}
			break;

		case sf::Event::MouseWheelScrolled:
			mouseWheelDelta = event.mouseWheelScroll.delta;
			coef = mouseWheelDelta > 0 ? 0.8f : 1.25f;
			worldView.zoom(coef);
			// centre doit se d�caler de coef * vecteur centre -> souris
			mousePos = mouse.getPosition(window);
			center = worldView.getCenter();
			windowSize = window.getSize();
			viewSize = worldView.getSize();
			worldView.setCenter(
				center.x + (1-coef) * ((float)mousePos.x - windowSize.x / 2) * viewSize.x / windowSize.x,
				center.y + (1-coef) * ((float)mousePos.y - windowSize.y / 2) * viewSize.y / windowSize.y
			);

			break;

		// event type
		case sf::Event::MouseButtonPressed :
			switch (event.mouseButton.button) {
				case sf::Mouse::Right :
					rightMousePressed = true;
					initialRightMousePos.x = event.mouseButton.x;
					initialRightMousePos.y = event.mouseButton.y;
					initialCenterPos = worldView.getCenter();
					break;
				
				case sf::Mouse::Left :
					sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
					// sf::Vector2f worldPos = window.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y));
					sf::Vector2f worldPos = window.mapPixelToCoords(pixelPos);
					simulator.appliedForce = Particle_simulator::userForce::Translation;
					simulator.user_point[0] = worldPos.x;
					simulator.user_point[1] = worldPos.y;

					// windowSize = window.getSize();
					// Cat* tempCat = searchCat(
					// 	worldView.getCenter().x + viewSize.x * ((float)event.mouseButton.x / windowSize.x - 0.5f),
					// 	worldView.getCenter().y + viewSize.y * ((float)event.mouseButton.y / windowSize.y - 0.5f)
					// );
					// if (tempCat) {
					// 	catHit = true;
					// 	if (!ctrlPressed) {
					// 		for (Cat* cat : selectedCat) {
					// 			cat->simUnlocked = true;
					// 			cat->glow(false);
					// 		}
					// 		selectedCat.clear();
					// 		selectedCatInitPos.clear();
					// 	}
					// 	selectedCat.push_back(tempCat);
					// 	selectedCatInitPos.push_back(tempCat->position[0]);
					// 	selectedCatInitPos.push_back(tempCat->position[1]);
					// 	tempCat->simUnlocked = false;
					// 	tempCat->glow();
					// }
					// else if (!ctrlPressed) {
					// 	catHit = false;
					// 	for (Cat* cat : selectedCat) {
					// 		cat->simUnlocked = true;
					// 		cat->glow(false);
					// 	}
					// 	selectedCat.clear();
					// 	selectedCatInitPos.clear();
					// }
					break;
			
			}
			break;

		// event type
		case sf::Event::MouseButtonReleased:
		if (event.mouseButton.button == sf::Mouse::Right) {
			rightMousePressed = false;
		}
			if (event.mouseButton.button == sf::Mouse::Left) {
				simulator.appliedForce = Particle_simulator::userForce::None;
				// if (leftMousePressed) {
				// 	leftMousePressed = false;

				// 	for (uint16_t i = 0; i < selectedCat.size(); i++) {
				// 		selectedCatInitPos[2*i   ] = selectedCat[i]->position[0];
				// 		selectedCatInitPos[2*i +1] = selectedCat[i]->position[1];
				// 	}
				// 	//if (!selectedCat.empty()) {
				// 		//selectedCat.back()->simUnlocked = true;
				// 		//selectedCat.pop_back();
				// 	//}
				// }
			}
			break;

		// event type
		case sf::Event::MouseMoved:
			windowSize = window.getSize();
			viewSize = worldView.getSize();
			// if (mouse.isButtonPressed(sf::Mouse::Right)) {
			if (rightMousePressed) {
				worldView.setCenter(
					initialCenterPos.x + ((float)initialRightMousePos.x - event.mouseMove.x) * viewSize.x / windowSize.x,
					initialCenterPos.y + ((float)initialRightMousePos.y - event.mouseMove.y) * viewSize.y / windowSize.y
				);

			}
			else if (mouse.isButtonPressed(sf::Mouse::Left)) {
				sf::Vector2i pixelPos = sf::Mouse::getPosition(window);
				sf::Vector2f worldPos = window.mapPixelToCoords(pixelPos);
				simulator.user_point[0] = worldPos.x;
				simulator.user_point[1] = worldPos.y;
				// uint16_t i = 0;
				// for (Cat* cat : selectedCat) {
				// 	float posX = cat->position[0];
				// 	float posY = cat->position[1];
				// 	cat->changePosition(
				// 		selectedCatInitPos[2*i] + worldView.getCenter().x + viewSize.x * ((float)event.mouseMove.x / windowSize.x - 0.5f) - initialLeftMousePos.x,
				// 		selectedCatInitPos[2*i+1] + worldView.getCenter().y + viewSize.y * ((float)event.mouseMove.y / windowSize.y - 0.5f) - initialLeftMousePos.y
				// 	);

				// 	//selectedCat->speed[0] = (selectedCat->position[0] - posX) * 10;
				// 	//selectedCat->speed[1] = (selectedCat->position[1] - posY) * 10;
				// 	cat->speed[0] = 0;
				// 	cat->speed[1] = 0;

				// 	cat->updateDisplay();
				// 	cat->moveRelations();
				// 	i++;
				// }
			}
			break;
		case sf::Event::Closed:
			window.close();
			break;
		}
	}
}

// Cat* EventHandler::searchCat(float x, float y) {
// 	for (Cat* cat : cats) {
// 		if (cat->position[0] <= x &&
// 			cat->position[0] + cat->size[0] >= x &&
// 			cat->position[1] <= y &&
// 			cat->position[1] + cat->size[1] >= y)
// 		{ // cursor if over the borders of the cat
// 			leftMousePressed = true;
// 			initialLeftMousePos.x = x;
// 			initialLeftMousePos.y = y;
// 			//selectedCatInitPos[0] = cat->position[0];
// 			//selectedCatInitPos[1] = cat->position[1];

// 			cat->simUnlocked = false;

// 			return cat;
// 		}
// 	}
// 	return nullptr;
// }