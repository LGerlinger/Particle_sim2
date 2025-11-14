#include "EventHandler.hpp"

#include <SFML/Window/Keyboard.hpp>
#include <cmath>

inline void inverse(bool& b) {b = !b;}; 


EventHandler::EventHandler(Renderer& renderer_, Particle_simulator& simulator_) :
	renderer(renderer_), window(renderer_.getWindow()), worldView(renderer_.getworldView()), simulator(simulator_)
{
	windowSize = window.getSize();
	window.setKeyRepeatEnabled(false);

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
			inverse(simulator.paused);
				break;
			case sf::Keyboard::SemiColon :
				simulator.step = true;
				break;
			case sf::Keyboard::Comma :
				simulator.quickstep = true;
				break;
			case sf::Keyboard::Add :
				simulator.params.dt *= 2;
				break;
			case sf::Keyboard::Equal :
				simulator.params.dt *= 2;
				break;
			case sf::Keyboard::Subtract :
				simulator.params.dt /= 2;
				break;
			case sf::Keyboard::Hyphen :
				simulator.params.dt /= 2;
				break;
			case sf::Keyboard::LControl :
				ctrlPressed = true;
				break;
			case sf::Keyboard::F :
				renderer.toggleFullScreen();
				break;
			case sf::Keyboard::S :
				renderer.takeScreenShot();
				break;
			case sf::Keyboard::H :
				if (keyboard.isKeyPressed(sf::Keyboard::LControl)) simulator.reinitialize_order = true;
				else {
					renderer.setHomeView();
				}
				break;
			
			case sf::Keyboard::I :
				inverse(renderer.dp_interaction);
				break;
			case sf::Keyboard::J :
				inverse(renderer.dp_worldBorder);
				break;
			case sf::Keyboard::K :
				inverse(renderer.dp_FPS);
				break;
			case sf::Keyboard::L :
				inverse(renderer.dp_segments);
				break;
			case sf::Keyboard::M :
				renderer.toggle_grid();
				break;
			case sf::Keyboard::O :
				inverse(renderer.dp_worldZones);
				break;
			case sf::Keyboard::P :
				inverse(renderer.dp_particles);
				break;
			case sf::Keyboard::W :
				inverse(renderer.enable_displaying);
				break;
			case sf::Keyboard::C :
				if (keyboard.isKeyPressed(sf::Keyboard::LControl)) 
					inverse(renderer.regular_clear);
				else {
					renderer.clear = true;
				}
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
			}
			break;
		
		case sf::Event::KeyReleased :
			switch (event.key.code) {
				case sf::Keyboard::C :
					renderer.clear = false;
					break;
				case sf::Keyboard::LControl :
					ctrlPressed = false;
					break;
				case sf::Keyboard::Comma :
					simulator.quickstep = false;
					break;
				default :
					remPressedKey(event.key.code);
					break;
			}
			break;

		case sf::Event::MouseWheelScrolled:
			renderer.changedView = true;
			mouseWheelDelta = event.mouseWheelScroll.delta;
			coef = mouseWheelDelta > 0 ? 0.8f : 1.25f;
			// centre doit se décaler de coef * vecteur centre -> souris
			mousePos = mouse.getPosition(window);
			center = worldView.getCenter();
			worldView.setCenter(
				center.x + (1-coef) * (mousePos.x - (float)windowSize.x / 2) * worldView.getSize().x / windowSize.x,
				center.y + (1-coef) * (mousePos.y - (float)windowSize.y / 2) * worldView.getSize().y / windowSize.y
			);
			worldView.zoom(coef);

			break;

		// event type
		case sf::Event::MouseButtonPressed :
			switch (event.mouseButton.button) {
				case sf::Mouse::Right :
					rightMousePressed = true;
					initialRightMousePos.x = event.mouseButton.x;
					initialRightMousePos.y = event.mouseButton.y;
					initialCenterPos = worldView.getCenter();
					renderer.followed = nullptr;
					break;
				
				case sf::Mouse::Left :
					leftMousePressed = true;
					sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window), worldView);
					uint32_t select = searchParticle(worldPos.x, worldPos.y);

					if (!ctrlPressed) { // empty list of selected particles
						clear_selection();
					}
					if (select != NULLPART) { // hit Particle
						if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) {
							renderer.followed = &simulator[select];
						}
						else {
							selectedPart.push_back(select);
							selectedPartInitPos.push_back(simulator[select].position[0]);
							selectedPartInitPos.push_back(simulator[select].position[1]);
							// simulator[select].select(true);
							initialLeftMousePos.x = worldPos.x;
							initialLeftMousePos.y = worldPos.y;
						}
					}
					else { // no hit
						if (n_key_pressed) { // another key was pressed
							for (uint8_t k=0; k<MAX_KEYS; k++) {
								switch (getPressedKey(k)) {
									case sf::Keyboard::Delete :
										simulator.params.range = 200;
										if (simulator.paused) {
											simulator.delete_range(worldPos.x, worldPos.y, simulator.params.range);
										} else {
											simulator.deletion_order = true;
										}
										break;
									case sf::Keyboard::A :
										simulator.params.range = INFINITY;
										simulator.appliedForce = Particle_simulator::userForce::Translation;
										break;
									case sf::Keyboard::Z :
										simulator.params.range = 200;
										simulator.appliedForce = Particle_simulator::userForce::Translation;
										break;
									case sf::Keyboard::E :
										simulator.params.range = INFINITY;
										simulator.appliedForce = Particle_simulator::userForce::Rotation;
										break;
									case sf::Keyboard::R :
										simulator.params.range = 200;
										simulator.appliedForce = Particle_simulator::userForce::Rotation;
										break;
									case sf::Keyboard::T :
										simulator.params.range = INFINITY;
										simulator.appliedForce = Particle_simulator::userForce::Vortex;
										break;
									case sf::Keyboard::Y :
										simulator.params.range = 200;
										simulator.appliedForce = Particle_simulator::userForce::Vortex;
										break;
								}
							}
							simulator.user_point[0] = worldPos.x;
							simulator.user_point[1] = worldPos.y;
							renderer.interacting = true;
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
				leftMousePressed = false;
				simulator.deletion_order = false;
				simulator.appliedForce = Particle_simulator::userForce::None;
				renderer.interacting = false;
				// for (uint8_t k=0; k<MAX_KEYS; k++) {
				// 	switch (getPressedKey(k)) {
				// 		default :
				// 			break;
				// 	}
				// }
			}
			break;

		// event type
		case sf::Event::MouseMoved:
			if (rightMousePressed) {
				renderer.changedView = true;
				worldView.setCenter(
					initialCenterPos.x + ((float)initialRightMousePos.x - event.mouseMove.x) * worldView.getSize().x / windowSize.x,
					initialCenterPos.y + ((float)initialRightMousePos.y - event.mouseMove.y) * worldView.getSize().y / windowSize.y
				);

			}
			else if (leftMousePressed) {
				sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
				simulator.user_point[0] = worldPos.x;
				simulator.user_point[1] = worldPos.y;
			}
			break;

		case sf::Event::Resized:
			renderer.resizeWindow(event.size.width, event.size.height);
			windowSize = window.getSize();
			break;

		case sf::Event::LostFocus:
			renderer.slowFPS(true);
			emptyPressedKey();
			break;
			case sf::Event::GainedFocus:
			renderer.slowFPS(false);
			emptyPressedKey();
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

void EventHandler::emptyPressedKey() {
	for (uint8_t k=0; k<MAX_KEYS; k++) {
		pressed_keys[k] = sf::Keyboard::Unknown;
	}
	n_key_pressed = 0;
}

void EventHandler::update_selection_pos() {
	if (leftMousePressed && selectedPart.size()) {
		sf::Vector2f worldPos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
		float speed[2];
		for (uint32_t p=0; p<selectedPart.size(); p++) {
			simulator[selectedPart[p]].position[0] = selectedPartInitPos[2*p  ] + worldPos.x - initialLeftMousePos.x;
			simulator[selectedPart[p]].position[1] = selectedPartInitPos[2*p+1] + worldPos.y - initialLeftMousePos.y;
			
			speed[0] = simulator[selectedPart[p]].position[0] - selectedPartInitPos[2*p  ];
			speed[1] = simulator[selectedPart[p]].position[1] - selectedPartInitPos[2*p+1];
	
			simulator[selectedPart[p]].speed[0] = speed[0] != 0 ? speed[0] /(10*simulator.params.dt) : simulator[selectedPart[p]].speed[0];
			simulator[selectedPart[p]].speed[1] = speed[1] != 0 ? speed[1] /(10*simulator.params.dt) : simulator[selectedPart[p]].speed[1];
	
			if (simulator.paused) {
				simulator.world.change_cell_part(selectedPart[p], selectedPartInitPos[2*p], selectedPartInitPos[2*p+1], simulator[selectedPart[p]].position[0], simulator[selectedPart[p]].position[1]);
			}
			selectedPartInitPos[2*p  ] = simulator[selectedPart[p]].position[0];
			selectedPartInitPos[2*p+1] = simulator[selectedPart[p]].position[1];
			
		}
		initialLeftMousePos = worldPos;
	}
}

void EventHandler::clear_selection() {
	selectedPart.clear();
	selectedPartInitPos.clear();
}

uint32_t EventHandler::searchParticle(float x, float y) {
	uint16_t cell_x, cell_y;
	float vec[2];
	float norm;
	// std::cout << "search particle."
	if (simulator.world.getCellCoord_fromPos(x, y, &cell_x, &cell_y) && !simulator.isLoading()) { // point (x, y) is in a cell. So we search the Particles around that Cell
		uint16_t dPos[2];
	
		for (int8_t dy=-1; dy<2; dy++) {
			dPos[1] = cell_y + dy;
			if (dPos[1] < simulator.world.getGridSize(1)) {
				for (int8_t dx=-1; dx<2; dx++) {
					dPos[0] = cell_x + dx;
					if (dPos[0] < simulator.world.getGridSize(0)) {

						Cell& cell = simulator.world.getCell(dPos[0], dPos[1]);
						for (uint8_t i=0; i<cell.nb_parts; i++) {
							vec[0] = x - simulator[cell.parts[i]].position[0];
							vec[1] = y - simulator[cell.parts[i]].position[1];
							norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
							if (norm < simulator.params.radii) {
								return cell.parts[i];
							}
						}

					}
				}
			}
		}
	} else { // Not in a Cell or Cells aren't being filled, so we have to search though every Particle (might take long)
		for (uint32_t p=0; p<simulator.get_active_part(); p++) {
			vec[0] = x - simulator[p].position[0];
			vec[1] = y - simulator[p].position[1];
			norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
			if (norm < simulator.params.radii) {
				return p;
			}
		}
	}

	// no particles found
	return NULLPART;
}