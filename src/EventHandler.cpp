#include "EventHandler.hpp"
#include "Particle_simulator.hpp"

#include <cmath>
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
	
	for (uint8_t k=0; k<MAX_KEYS; k++) {
		pressed_keys[k] = sf::Keyboard::Unknown;
	}
	
	selectedPart.resize(0);
	selectedPartInitPos.resize(0);
}

EventHandler::~EventHandler() {
	clear_selection();
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
			// Keys with immediate effects, or that don't interact with the mouse, aren't added to the pressedkey list
			switch (event.key.code) {
			case sf::Keyboard::Escape :
				window.close();
				break;
			case sf::Keyboard::Space :
				simulator.paused = !simulator.paused;
				break;
			case sf::Keyboard::SemiColon :
				simulator.step = true;
				break;
			case sf::Keyboard::Add :
				simulator.dt *= 2;
				break;
			case sf::Keyboard::Subtract :
				simulator.dt /= 2;
				break;
			case sf::Keyboard::LControl :
				ctrlPressed = true;
				break;
			case sf::Keyboard::Delete :
				if (selectedPart.size()) {
					for (uint32_t p=0; p<selectedPart.size(); p++) {
						simulator.delete_particle(selectedPart[p]);
					}
					clear_selection();
				}
				addPressedKey(event.key.code);
				break;
			default :
				addPressedKey(event.key.code);
				break;
			// case sf::Keyboard::S:
			//	 if (ctrlPressed) sl.savePos();
			//	 break;
			// case sf::Keyboard::C:
			//	 sl.loadPos();
			//	 break;
			}
			break;
		
		case sf::Event::KeyReleased :
			if (event.key.code == sf::Keyboard::LControl) {
				ctrlPressed = false;
			}else {
				remPressedKey(event.key.code);
			}
			break;

		case sf::Event::MouseWheelScrolled:
			mouseWheelDelta = event.mouseWheelScroll.delta;
			coef = mouseWheelDelta > 0 ? 0.8f : 1.25f;
			// centre doit se décaler de coef * vecteur centre -> souris
			mousePos = mouse.getPosition(window);
			center = worldView.getCenter();
			worldView.setCenter(
				center.x + (1-coef) * ((float)mousePos.x - windowSize.x / 2) * viewSize.x / windowSize.x,
				center.y + (1-coef) * ((float)mousePos.y - windowSize.y / 2) * viewSize.y / windowSize.y
			);
			worldView.zoom(coef);
			viewSize = worldView.getSize();

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
					lefttMousePressed = true;
					sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
					uint32_t select = searchParticle(worldPos.x, worldPos.y);

					if (!ctrlPressed && selectedPart.size() != 0) { // empty list of selected particles
						clear_selection();
					}
					if (select != NULLPART) { // hit Particle
						selectedPart.push_back(select);
						selectedPartInitPos.push_back(simulator.particle_array[select].position[0]);
						selectedPartInitPos.push_back(simulator.particle_array[select].position[1]);
						simulator.particle_array[select].select(true);
						initialLeftMousePos.x = worldPos.x;
						initialLeftMousePos.y = worldPos.y;
					}
					else { // no hit
						if (n_key_pressed) { // another key was pressed
							for (uint8_t k=0; k<MAX_KEYS; k++) {
								switch (getPressedKey(k)) {
									case sf::Keyboard::Delete :
									if (simulator.paused) {
										simulator.delete_range(worldPos.x, worldPos.y, simulator.range);
									} else {
										simulator.deletion_order = true;
									}
										break;
									case sf::Keyboard::A :
										simulator.appliedForce = Particle_simulator::userForce::Translation;
										break;
									case sf::Keyboard::Z :
										simulator.appliedForce = Particle_simulator::userForce::Translation_ranged;
										break;
									case sf::Keyboard::E :
										simulator.appliedForce = Particle_simulator::userForce::Rotation;
										break;
									case sf::Keyboard::R :
										simulator.appliedForce = Particle_simulator::userForce::Rotation_ranged;
										break;
									case sf::Keyboard::T :
										simulator.appliedForce = Particle_simulator::userForce::Vortex;
										break;
									case sf::Keyboard::Y :
										simulator.appliedForce = Particle_simulator::userForce::Vortex_ranged;
										break;
								}
							}
							simulator.user_point[0] = worldPos.x;
							simulator.user_point[1] = worldPos.y;
						}
					}
					break;
			
			}
			break;

		// event type
		case sf::Event::MouseButtonReleased:
		if (event.mouseButton.button == sf::Mouse::Right) {
			rightMousePressed = false;
		}
			if (event.mouseButton.button == sf::Mouse::Left) {
				lefttMousePressed = false;
				for (uint8_t k=0; k<MAX_KEYS; k++) {
					switch (getPressedKey(k)) {
						default :
							simulator.deletion_order = false;
							simulator.appliedForce = Particle_simulator::userForce::None;
							break;
					}
				}
			}
			break;

		// event type
		case sf::Event::MouseMoved:
			if (rightMousePressed) {
				worldView.setCenter(
					initialCenterPos.x + ((float)initialRightMousePos.x - event.mouseMove.x) * viewSize.x / windowSize.x,
					initialCenterPos.y + ((float)initialRightMousePos.y - event.mouseMove.y) * viewSize.y / windowSize.y
				);

			}
			else if (lefttMousePressed) {
				sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
				simulator.user_point[0] = worldPos.x;
				simulator.user_point[1] = worldPos.y;
			}
			break;

		case sf::Event::Resized:
			// viewSize = worldView.getSize();
			worldView.setCenter(
				worldView.getCenter().x + (event.size.width -  (float)windowSize.x) * viewSize.x / (2*windowSize.x),
				worldView.getCenter().y + (event.size.height - (float)windowSize.y) * viewSize.y / (2*windowSize.y)
			);
			worldView.setSize(
				viewSize.x * event.size.width  / windowSize.x,
				viewSize.y * event.size.height / windowSize.y
			);
			windowSize = window.getSize();
			viewSize = worldView.getSize();
			break;

		case sf::Event::Closed:
			window.close();
			break;
		}
	}
}


void EventHandler::addPressedKey(sf::Keyboard::Key key) {
	for (uint8_t k=0; k<MAX_KEYS; k++) {
		if (pressed_keys[k] == sf::Keyboard::Unknown) {
			pressed_keys[k] = key;
			n_key_pressed++;
			break;
		}
	}
}

void EventHandler::remPressedKey(sf::Keyboard::Key key) {
	for (uint8_t k=0; k<MAX_KEYS; k++) {
		if (pressed_keys[k] == key) {
			pressed_keys[k] = sf::Keyboard::Unknown;
			n_key_pressed--;
			break;
		}
	}
}


void EventHandler::update_selection_pos() {
	if (lefttMousePressed && selectedPart.size()) {
		sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
		float speed[2];
		for (uint32_t p=0; p<selectedPart.size(); p++) {
			simulator.particle_array[selectedPart[p]].position[0] = selectedPartInitPos[2*p  ] + worldPos.x - initialLeftMousePos.x;
			simulator.particle_array[selectedPart[p]].position[1] = selectedPartInitPos[2*p+1] + worldPos.y - initialLeftMousePos.y;
			
			speed[0] = simulator.particle_array[selectedPart[p]].position[0] - selectedPartInitPos[2*p  ];
			speed[1] = simulator.particle_array[selectedPart[p]].position[1] - selectedPartInitPos[2*p+1];
	
			simulator.particle_array[selectedPart[p]].speed[0] = speed[0] != 0 ? speed[0] /(10*simulator.dt) : simulator.particle_array[selectedPart[p]].speed[0];
			simulator.particle_array[selectedPart[p]].speed[1] = speed[1] != 0 ? speed[1] /(10*simulator.dt) : simulator.particle_array[selectedPart[p]].speed[1];
	
			if (simulator.paused) {
				simulator.world.change_cell_part(selectedPart[p], selectedPartInitPos[2*p], selectedPartInitPos[2*p+1], simulator.particle_array[selectedPart[p]].position[0], simulator.particle_array[selectedPart[p]].position[1]);
			}
			selectedPartInitPos[2*p  ] = simulator.particle_array[selectedPart[p]].position[0];
			selectedPartInitPos[2*p+1] = simulator.particle_array[selectedPart[p]].position[1];
			
		}
		initialLeftMousePos = worldPos;
	}
}

void EventHandler::clear_selection() {
	for (uint32_t p=0; p<selectedPart.size(); p++) {
		simulator.particle_array[selectedPart[p]].select(false);
	}
	selectedPart.clear();
	selectedPartInitPos.clear();
}

uint32_t EventHandler::searchParticle(float x, float y) {
	uint16_t cell_x, cell_y;
	float vec[2];
	float norm;
	if (simulator.world.getCellCoord_fromPos(x, y, &cell_x, &cell_y)) {
		uint16_t dPos[2];
	
		for (int8_t dy=-1; dy<2; dy++) {
			dPos[1] = cell_y + dy;
			if (dPos[1] < simulator.world.gridSize[1]) {
				for (int8_t dx=-1; dx<2; dx++) {
					dPos[0] = cell_x + dx;
					if (dPos[0] < simulator.world.gridSize[0]) {

						Cell& cell = simulator.world.getCell(dPos[0], dPos[1]);
						for (uint8_t i=0; i<cell.nb_parts; i++) {
							vec[0] = x - simulator.particle_array[cell.parts[i]].position[0];
							vec[1] = y - simulator.particle_array[cell.parts[i]].position[1];
							norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
							if (norm < simulator.radii) {
								return cell.parts[i];
							}
						}

					}
				}
			}
		}
	}

	// no particles found
	return NULLPART;
}