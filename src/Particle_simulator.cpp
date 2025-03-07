#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "Particle_simulator.hpp"
#include "Particle.hpp"


Particle_simulator::Particle_simulator() : world(1800, 1000, 2*radii, 2*radii) {
	std::cout << "Particle_simulator::Particle_simulator()" << std::endl;
	for (uint32_t i=0; i<NB_PART; i++) {
		particle_array[i].position[0] = rand()%1800;
		particle_array[i].position[1] = rand()%1000;

		
		particle_array[i].speed[0] = 0;
		particle_array[i].speed[1] = 0;
		particle_array[i].acceleration[0] = 0;
		particle_array[i].acceleration[1] = 0;

		particle_array[i].colour[0] = rand()%255;
		particle_array[i].colour[1] = rand()%255;
		particle_array[i].colour[2] = rand()%255;
		particle_array[i].colour[3] = 255;
	}


	// Wolrd borders 
	seg_array[0].pos[0][0] = 0;
	seg_array[0].pos[0][1] = 0;
	seg_array[0].pos[1][0] = 1800;
	seg_array[0].pos[1][1] = 0;

	seg_array[1].pos[0][0] = seg_array[0].pos[1][0];
	seg_array[1].pos[0][1] = seg_array[0].pos[1][1];
	seg_array[1].pos[1][0] = 1800;
	seg_array[1].pos[1][1] = 1000;

	seg_array[2].pos[0][0] = seg_array[1].pos[1][0];
	seg_array[2].pos[0][1] = seg_array[1].pos[1][1];
	seg_array[2].pos[1][0] = 0;
	seg_array[2].pos[1][1] = 1000;

	seg_array[3].pos[0][0] = seg_array[2].pos[1][0];
	seg_array[3].pos[0][1] = seg_array[2].pos[1][1];
	seg_array[3].pos[1][0] = seg_array[0].pos[0][0];
	seg_array[3].pos[1][1] = seg_array[0].pos[0][1];

	// funny lines
	seg_array[4].pos[0][0] = 100;
	seg_array[4].pos[0][1] = 100;
	seg_array[4].pos[1][0] = 900;
	seg_array[4].pos[1][1] = 500;

	seg_array[5].pos[0][0] = 1700;
	seg_array[5].pos[0][1] = 300;
	seg_array[5].pos[1][0] = 700;
	seg_array[5].pos[1][1] = 800;
	// std::cout << "fin Particle_simulator::Particle_simulator()" << std::endl;
}


Particle_simulator::~Particle_simulator() {
	std::cout << "Particle_simulator::~Particle_simulator()" << std::endl;
	if (threads_created) {
		for (uint8_t i=0; i<MAX_THREAD_NUM; i++) {
			thread_list[i]->join();
			delete(thread_list[i]);
		}
	}
}


void Particle_simulator::start_simulation_threads() {
	// std::cout << "Particle_simulator::start_simulation_threads()" << std::endl;
	sync_count = 0;
	simulate = true;
	for (uint8_t i=0; i<MAX_THREAD_NUM; i++) {
		thread_list[i] = new std::thread(&Particle_simulator::simulation_thread, this, i);
	}
	threads_created = true;
}

void Particle_simulator::stop_simulation_threads() {
	// std::cout << "Particle_simulator::stop_simulation_threads()" << std::endl;
	simulate = false;
	pthread_cond_broadcast(&sync_condition); // Done to make sure no thread is waiting at the simulation synchronisation stop
	for (uint8_t i=0; i<MAX_THREAD_NUM; i++) {
		thread_list[i]->join();
		thread_list[i] = nullptr;
	}
	threads_created = false;
}


void Particle_simulator::simulation_thread(uint8_t th_id) {
	// std::cout << "Particle_simulator::simulation_thread()" << std::endl;
	uint32_t nppt; // number of particles per thread
	uint32_t p_start, p_end;
	conso.start_perf_check("simulateur", 1000);
	while (simulate) {
		// Synchronisation
		// std::cout << "lock thread id : " << (short)th_id << "  count=" << (short)sync_count << std::endl;
		pthread_mutex_lock(&sync_mutex);
		sync_count++;
		if (sync_count == MAX_THREAD_NUM) { // last thread to reache synchronisation stop
			sync_count = 0;
			conso.Tick_fine();
			while (simulate && paused && !step) {
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
			step = false;
			conso.Start();

			// This part is executed by only one thread while others wait
			{
				time[0] += dt;
	
				if (deletion_order) {
					delete_range(user_point[0], user_point[1], range);
				}
				if (0.01f * radii < time[0] - time[1]) {
					time[1] = time[0];
					uint32_t pa = create_particle(world.size[0]/2-200, 100);
					uint32_t pb = create_particle(world.size[0]/2+200, 100);

					if (pa != NULLPART) {
						particle_array[pa].speed[0] =  800 * cos(-2*time[0]);
						particle_array[pa].speed[1] =  800 * sin(-2*time[0]);
					}
					if (pb != NULLPART) {
						particle_array[pb].speed[0] = -800 * cos(-2*time[0]);
						particle_array[pb].speed[1] =  800 * sin(-2*time[0]);
					}
				}

				world.update_grid_particle_contenance(particle_array, nb_active_part);
			}

			pthread_cond_broadcast(&sync_condition);

		} else { // not the last thread so wait
			int ernum = pthread_cond_wait(&sync_condition, &sync_mutex);

		}
		pthread_mutex_unlock(&sync_mutex);


		// Simulation -- applying forces and collisions
		nppt = nb_active_part/MAX_THREAD_NUM; // number of particles per thread +- 1
		p_start = nppt *th_id       + std::min(th_id, (uint8_t)(nb_active_part%MAX_THREAD_NUM));
		p_end =   nppt *(th_id+1)   + std::min(th_id, (uint8_t)(nb_active_part%MAX_THREAD_NUM)) + (th_id < (uint8_t)(nb_active_part%MAX_THREAD_NUM));

		for (uint32_t p=p_start; p<p_end; p++) {
			particle_array[p].acceleration[0] = 0;
			particle_array[p].acceleration[1] = 0;
		}

		switch (appliedForce) {
			case userForce::None:
				break;
			case userForce::Translation:
				attraction(p_start, p_end);
				break;
			case userForce::Translation_ranged:
				attraction_ranged(p_start, p_end);
				break;
			case userForce::Rotation:
				rotation(p_start, p_end);
				break;
			case userForce::Rotation_ranged:
				rotation_ranged(p_start, p_end);
				break;
			case userForce::Vortex:
				vortex(p_start, p_end);
				break;
			case userForce::Vortex_ranged:
				vortex_ranged(p_start, p_end);
				break;
		}

		// point_gravity(p_start, p_end);
		gravity(p_start, p_end);
		// fluid_friction(p_start, p_end);
		// static_friction(p_start, p_end);

		// collision_pp(p_start, p_end);
		collision_pp_grid_threaded(p_start, p_end);
		// collision_pp_glue(p_start, p_end);
		collision_pl(p_start, p_end);
		world_borders(p_start, p_end);

		// Simulation -- displacement
		solver(p_start, p_end);
	}
}


void Particle_simulator::simu_step() {
	// std::cout << "Particle_simulator::simu_step()" << std::endl;
	// particle_mutex.lock();
	for (uint32_t i=0; i<nb_active_part; i++) {
		particle_array[i].acceleration[0] = 0;
		particle_array[i].acceleration[1] = 0;
	}

	switch (appliedForce) {
		case userForce::Translation:
			attraction(0, nb_active_part);
			break;
		case userForce::Rotation:
			rotation(0, nb_active_part);
			break;
		case userForce::Vortex:
			vortex(0, nb_active_part);
			break;
		case userForce::None:
			break;
	}

	// gravity(0, nb_active_part);
	point_gravity(0, nb_active_part);

	world.update_grid_particle_contenance(particle_array, nb_active_part);
	// collision_pp(0, nb_active_part);
	collision_pp_grid_threaded(0, nb_active_part);
	collision_pl(0, nb_active_part);

	// collision_pp_grid();
	solver(0, nb_active_part);
}


void Particle_simulator::solver(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::solver()" << std::endl;
	for (uint32_t p=p_start; p<p_end; p++) {
		particle_array[p].speed[0] += particle_array[p].acceleration[0] * dt;
		particle_array[p].speed[1] += particle_array[p].acceleration[1] * dt;
	
		particle_array[p].position[0] += particle_array[p].speed[0] * dt;
		particle_array[p].position[1] += particle_array[p].speed[1] * dt;
	}
}


void Particle_simulator::collision_pp(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::collision_pp()" << std::endl;
	float vec[2];
	float dist;
	float collision_coef = 0.45f;
	for (uint8_t i=0; i<2; i++) {
		for (uint32_t p1=p_start; p1<p_end; p1++) {

			for (uint32_t p2=p1+1; p2<nb_active_part; p2++) {
				vec[0] = particle_array[p2].position[0] + particle_array[p2].speed[0]*dt - particle_array[p1].position[0] - particle_array[p1].speed[0]*dt;
				vec[1] = particle_array[p2].position[1] + particle_array[p2].speed[1]*dt - particle_array[p1].position[1] - particle_array[p1].speed[1]*dt;
				dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
				vec[0] /= dist;
				vec[1] /= dist;
	
				if (dist < 2*radii) {
					particle_array[p1].speed[0] -= vec[0] * (2*radii - dist) /dt *collision_coef;
					particle_array[p1].speed[1] -= vec[1] * (2*radii - dist) /dt *collision_coef;
					particle_array[p2].speed[0] += vec[0] * (2*radii - dist) /dt *collision_coef;
					particle_array[p2].speed[1] += vec[1] * (2*radii - dist) /dt *collision_coef;
				}
			}

		}
	}
}


void Particle_simulator::collision_pp_grid() {
	// std::cout << "Particle_simulator::collision_pp_grid()" << std::endl;
	// std::cout << "========== Particle_simulator::collision_pp_grid ==========" << std::endl;
	uint16_t dPos[2];
	float vec[2];
	float dist;
	float collision_coef = 0.45f;
	uint32_t part1, part2;
	for (uint8_t i=0; i<2; i++) {
		for (uint16_t y=0; y<world.gridSize[1]; y++) {
			for (uint16_t x=0; x<world.gridSize[0]; x++) {
				// std::cout << "cell1 size :" << (short)world.getCell(x, y).nb_parts << std::endl;
				for (uint16_t p1=0; p1<world.getCell(x, y).nb_parts; p1++) {
					part1 = world.getCell(x, y).parts[p1];
					// std::cout << "[" << x << ", " << y << "] : " << part1 << std::endl;
	
					for (int8_t dy=0; dy<2; dy++) {
						dPos[1] = y + dy;
						if (dPos[1] < world.gridSize[1]) {
							for (int8_t dx=-dy; dx<2; dx++) {
								dPos[0] = x + dx;
								if (dPos[0] < world.gridSize[0]) {
									
									// std::cout << "\tcell2 [" << dPos[0] << ", " << dPos[1] << "], size :" << (short)world.getCell(dPos[0], dPos[1]).nb_parts << std::endl;
									for (uint16_t p2=0; p2<world.getCell(dPos[0], dPos[1]).nb_parts; p2++) {
										part2 = world.getCell(dPos[0], dPos[1]).parts[p2];
										// std::cout << "\t\t" << part2 << std::endl;
										if (part1 != part2) {
		
											vec[0] = particle_array[part2].position[0] + particle_array[part2].speed[0]*dt - particle_array[part1].position[0] - particle_array[part1].speed[0]*dt;
											vec[1] = particle_array[part2].position[1] + particle_array[part2].speed[1]*dt - particle_array[part1].position[1] - particle_array[part1].speed[1]*dt;
											dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
											vec[0] /= dist;
											vec[1] /= dist;
								
											if (dist < 2*radii) {
												particle_array[part1].speed[0] -= vec[0] * (2*radii - dist) /dt *collision_coef;
												particle_array[part1].speed[1] -= vec[1] * (2*radii - dist) /dt *collision_coef;
												particle_array[part2].speed[0] += vec[0] * (2*radii - dist) /dt *collision_coef;
												particle_array[part2].speed[1] += vec[1] * (2*radii - dist) /dt *collision_coef;
											}
										}
									}
	
								}
							}
						}
					}
					
				}
			}
		}
	}
}


void Particle_simulator::collision_pp_grid_threaded(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::collision_pp_grid_threaded()" << std::endl;
	// std::cout << "========== Particle_simulator::collision_pp_grid_threaded ==========" << std::endl;
	uint16_t dPos[2];
	float vec[2];
	float dist;
	float collision_coef = 0.45f;
	uint32_t part2;
	uint16_t x, y;
	for (uint8_t i=0; i<2; i++) {
		for (uint32_t p1=p_start; p1<p_end; p1++) {

			x = particle_array[p1].position[0] / world.cellSize[0];
			y = particle_array[p1].position[1] / world.cellSize[1];
			// if (p1 < 10) std::cout << "p1 = " << p1 << ",  cell1 [" << x << ", " << y << "] size :" << (short)world.getCell(x, y).nb_parts << std::endl;
	
	
			for (int8_t dy=0; dy<2; dy++) {
				dPos[1] = y + dy;
				if (dPos[1] < world.gridSize[1]) {
					for (int8_t dx=-dy; dx<2; dx++) {
						dPos[0] = x + dx;
						if (dPos[0] < world.gridSize[0]) {
							
							// std::cout << "\tcell2 [" << dPos[0] << ", " << dPos[1] << "], size :" << (short)world.getCell(dPos[0], dPos[1]).nb_parts << std::endl;
							for (uint16_t p2=0; p2<world.getCell(dPos[0], dPos[1]).nb_parts; p2++) {
								part2 = world.getCell(dPos[0], dPos[1]).parts[p2];
								// std::cout << "\t\t" << part2 << std::endl;
								if (p1 != part2) {
	
									vec[0] = particle_array[part2].position[0] + particle_array[part2].speed[0]*dt - particle_array[p1].position[0] - particle_array[p1].speed[0]*dt;
									vec[1] = particle_array[part2].position[1] + particle_array[part2].speed[1]*dt - particle_array[p1].position[1] - particle_array[p1].speed[1]*dt;
									dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
									vec[0] /= dist;
									vec[1] /= dist;
						
									if (dist < 2*radii) {
										particle_array[p1].speed[0] -= vec[0] * (2*radii - dist) /dt *collision_coef;
										particle_array[p1].speed[1] -= vec[1] * (2*radii - dist) /dt *collision_coef;
										particle_array[part2].speed[0] += vec[0] * (2*radii - dist) /dt *collision_coef;
										particle_array[part2].speed[1] += vec[1] * (2*radii - dist) /dt *collision_coef;
									}
								}
							}
	
						}
					}
				}
			}

		}
					
	}
}



void Particle_simulator::collision_pl(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::collision_pl()" << std::endl;
	float vec[2];
	float AB[2], AC[2];
	float AB_dim[2];
	float normABsq;
	float temp;
	float min[2], max[2];
	float part_next_pos[2];
	for (uint32_t s=0; s<nb_active_seg; s++) {
		AB[0] = seg_array[s].pos[1][0] - seg_array[s].pos[0][0];
		AB[1] = seg_array[s].pos[1][1] - seg_array[s].pos[0][1];
		normABsq = AB[0]*AB[0] + AB[1]*AB[1];
		AB_dim[0] = AB[0]/normABsq;
		AB_dim[1] = AB[1]/normABsq;

		min[0] = std::min(seg_array[s].pos[0][0], seg_array[s].pos[1][0]);
		max[0] = std::max(seg_array[s].pos[0][0], seg_array[s].pos[1][0]);
		min[1] = std::min(seg_array[s].pos[0][1], seg_array[s].pos[1][1]);
		max[1] = std::max(seg_array[s].pos[0][1], seg_array[s].pos[1][1]);

		for (uint32_t p=p_start; p<p_end; p++) {
			part_next_pos[0] = particle_array[p].position[0] + particle_array[p].speed[0]*dt;
			part_next_pos[1] = particle_array[p].position[1] + particle_array[p].speed[1]*dt;

			AC[0] = part_next_pos[0] - seg_array[s].pos[0][0];
			AC[1] = part_next_pos[1] - seg_array[s].pos[0][1];
			
			// Calculating AP = (AC.AB)/||AB||² * AB
			temp = AC[0]*AB_dim[0] + AC[1]*AB_dim[1];
			vec[0] = temp * AB[0];
			vec[1] = temp * AB[1];

			// Calculating OP : OP = OA + AP
			vec[0] = vec[0] + seg_array[s].pos[0][0];
			vec[1] = vec[1] + seg_array[s].pos[0][1];

			if (min[0] <= vec[0] && vec[0] <= max[0] && min[1] <= vec[1] && vec[1] <= max[1]) { // P is in [AB]
				// Calculating CP : CP = CO + OP = OP - OC
				vec[0] = vec[0] - part_next_pos[0];
				vec[1] = vec[1] - part_next_pos[1];
				temp = sqrt(vec[0]*vec[0] + vec[1]*vec[1]); // ||CP||

				if (temp < radii) { // Collision
					vec[0] /= temp;
					vec[1] /= temp;
					particle_array[p].speed[0] -= vec[0] * (radii - temp) /dt;
					particle_array[p].speed[1] -= vec[1] * (radii - temp) /dt;
				}
			}
			else { // P is not in [AB] but the particle might still be near the edges A or B
				for (uint8_t i=0; i<2; i++) {
					vec[0] = seg_array[s].pos[i][0] - part_next_pos[0];
					vec[1] = seg_array[s].pos[i][1] - part_next_pos[1];
					temp = sqrt(vec[0]*vec[0] + vec[1]*vec[1]); // ||CX||

					if (temp < radii) { // Collision
						vec[0] /= temp;
						vec[1] /= temp;
						particle_array[p].speed[0] -= vec[0] * (radii - temp) /dt;
						particle_array[p].speed[1] -= vec[1] * (radii - temp) /dt;
					}
				}
			}
		}

	}
}


inline float dot(float vec1[2], float vec2[2]) {
	return vec1[0]*vec2[0] + vec1[1]*vec2[1];
}

void Particle_simulator::collision_pp_glue(uint32_t p_start, uint32_t p_end) {
	uint16_t dPos[2];
	float vec[2];
	float speed_vec[2];
	float dist;
	float collision_coef = 0.45f;
	// float dist_offset = 1.2f;
	float force_coef = 10.f;
	uint32_t part2;
	uint16_t x, y;
	float res;
	for (uint8_t i=0; i<2; i++) {
		for (uint32_t p1=p_start; p1<p_end; p1++) {

			x = particle_array[p1].position[0] / world.cellSize[0];
			y = particle_array[p1].position[1] / world.cellSize[1];
			// if (p1 < 10) std::cout << "p1 = " << p1 << ",  cell1 [" << x << ", " << y << "] size :" << (short)world.getCell(x, y).nb_parts << std::endl;


			for (int8_t dy=0; dy<2; dy++) {
				dPos[1] = y + dy;
				if (dPos[1] < world.gridSize[1]) {
					for (int8_t dx=-dy; dx<2; dx++) {
						dPos[0] = x + dx;
						if (dPos[0] < world.gridSize[0]) {
							
							// std::cout << "\tcell2 [" << dPos[0] << ", " << dPos[1] << "], size :" << (short)world.getCell(dPos[0], dPos[1]).nb_parts << std::endl;
							for (uint16_t p2=0; p2<world.getCell(dPos[0], dPos[1]).nb_parts; p2++) {
								part2 = world.getCell(dPos[0], dPos[1]).parts[p2];
								// std::cout << "\t\t" << part2 << std::endl;
								if (p1 != part2) {
	
									vec[0] = particle_array[part2].position[0] + particle_array[part2].speed[0]*dt - particle_array[p1].position[0] - particle_array[p1].speed[0]*dt;
									vec[1] = particle_array[part2].position[1] + particle_array[part2].speed[1]*dt - particle_array[p1].position[1] - particle_array[p1].speed[1]*dt;
									dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
									vec[0] /= dist;
									vec[1] /= dist;

									if (dist < 2*radii) {
										particle_array[p1].speed[0] -= vec[0] * (2*radii - dist) /dt *collision_coef;
										particle_array[p1].speed[1] -= vec[1] * (2*radii - dist) /dt *collision_coef;
										particle_array[part2].speed[0] += vec[0] * (2*radii - dist) /dt *collision_coef;
										particle_array[part2].speed[1] += vec[1] * (2*radii - dist) /dt *collision_coef;
									}
									else if (dist < 2*radii*1.2f) {
										dist /= 2*radii;

										// Speed at which 2 is going relatively to 1
										speed_vec[0] = particle_array[part2].speed[0] - particle_array[p1].speed[0];
										speed_vec[1] = particle_array[part2].speed[1] - particle_array[p1].speed[1];

										res = force_coef * std::abs(dot(speed_vec, vec));

										particle_array[p1].acceleration[0] += vec[0] * res;
										particle_array[p1].acceleration[1] += vec[1] * res;
										particle_array[part2].acceleration[0] -= vec[0] * res;
										particle_array[part2].acceleration[1] -= vec[1] * res;
									}
								}
							}
	
						}
					}
				}
			}

		}
					
	}
}


void Particle_simulator::world_borders(uint32_t p_start, uint32_t p_end) {
	float vec[2];
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		for (uint8_t i=0; i<2; i++) {
			vec[i] = particle_array[p1].position[i] + particle_array[p1].speed[i]*dt;
			if (vec[i] < radii) {
				particle_array[p1].acceleration[i] += (radii - vec[i]) /dt /dt;
			} else if (world.size[i] - radii < vec[i]) {
				particle_array[p1].acceleration[i] -= (vec[i] - (world.size[i] - radii)) /dt /dt;
			}
		}
	}
}


void Particle_simulator::gravity(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::gravity()" << std::endl;
	for (uint32_t p=p_start; p<p_end; p++) {
		particle_array[p].acceleration[1] += 1000;
	}
}

void Particle_simulator::point_gravity(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::point_gravity()" << std::endl;
	float vec[2];
	float dist;
	float temp;

	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = grav_center[0] - particle_array[p].position[0];
		vec[1] = grav_center[1] - particle_array[p].position[1];
		dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
		temp = atan(dist) / dist;
		temp = temp * temp;
		vec[0] *= dist*temp;
		vec[1] *= dist*temp;
	
		particle_array[p].acceleration[0] += vec[0] * 1000;
		particle_array[p].acceleration[1] += vec[1] * 1000;
	}
}


void Particle_simulator::static_friction(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::static_friction()" << std::endl;
	float vec[2];
	float norm;

	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = particle_array[p].speed[0];
		vec[1] = particle_array[p].speed[1];
		norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
		particle_array[p].speed[0] = (norm<1) ? 0 : particle_array[p].speed[0];
		particle_array[p].speed[1] = (norm<1) ? 0 : particle_array[p].speed[1];
	}
}


void Particle_simulator::fluid_friction(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::fluid_friction()" << std::endl;
	for (uint32_t p=p_start; p<p_end; p++) {
		particle_array[p].speed[0] *= 1-0.1f*dt;
		particle_array[p].speed[1] *= 1-0.1f*dt;
	}
}



void Particle_simulator::attraction(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::attraction()" << std::endl;
	float vec[2], norm;
	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = user_point[0] - particle_array[p].position[0];
		vec[1] = user_point[1] - particle_array[p].position[1];
		norm = translation_force / sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
		
		particle_array[p].acceleration[0] += vec[0] * norm;
		particle_array[p].acceleration[1] += vec[1] * norm;
	}
}

void Particle_simulator::attraction_ranged(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::attraction_ranged()" << std::endl;
	float vec[2], norm;
	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = user_point[0] - particle_array[p].position[0];
		vec[1] = user_point[1] - particle_array[p].position[1];
		norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);

		if (norm < range) {
			particle_array[p].acceleration[0] += vec[0] * translation_force / norm;
			particle_array[p].acceleration[1] += vec[1] * translation_force / norm;
		}
	}
}

void Particle_simulator::rotation(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::rotation()" << std::endl;
	float vec[2], norm;
	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = -user_point[1] + particle_array[p].position[1];
		vec[1] =  user_point[0] - particle_array[p].position[0];
		norm = rotation_force / sqrt(vec[0]*vec[0] + vec[1]*vec[1]);

		particle_array[p].acceleration[0] += vec[0] * norm;
		particle_array[p].acceleration[1] += vec[1] * norm;
	}
}

void Particle_simulator::rotation_ranged(uint32_t p_start, uint32_t p_end) {
	float vec[2], norm;
	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = -user_point[1] + particle_array[p].position[1];
		vec[1] =  user_point[0] - particle_array[p].position[0];
		norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);

		if (norm < range) {
			particle_array[p].acceleration[0] += vec[0] * rotation_force / norm;
			particle_array[p].acceleration[1] += vec[1] * rotation_force / norm;
		}
	}
}

void Particle_simulator::vortex(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::vortex()" << std::endl;
	float vec[2], norm;
	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = user_point[0] - particle_array[p].position[0];
		vec[1] = user_point[1] - particle_array[p].position[1];
		float norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);

		particle_array[p].acceleration[0] += (vec[0]*translation_force - vec[1]*rotation_force) / norm;
		particle_array[p].acceleration[1] += (vec[1]*translation_force + vec[1]*rotation_force) / norm;
	}
}

void Particle_simulator::vortex_ranged(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::vortex_ranged()" << std::endl;
	float vec[2], norm;
	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = user_point[0] - particle_array[p].position[0];
		vec[1] = user_point[1] - particle_array[p].position[1];
		float norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);

		if (norm < range) {
			particle_array[p].acceleration[0] += (vec[0]*translation_force - vec[1]*rotation_force) / norm;
			particle_array[p].acceleration[1] += (vec[1]*translation_force + vec[1]*rotation_force) / norm;
		}
	}
}


void Particle_simulator::delete_particle(uint32_t p) {
	// std::cout << "Particle_simulator::delete_particle()" << std::endl;
	Particle swap;

	// std::cout << "\n\nDeletion =======" << std::endl;
	// std::cout << "nb_active_parts = " << nb_active_part << std::endl;
	// std::cout << p << " :" << std::endl;
	// particle_array[p].print();
	// std::cout << nb_active_part-1 << " :" << std::endl;
	// particle_array[nb_active_part-1].print();

	swap = particle_array[nb_active_part-1]; // Their order doesn't matter
	particle_array[nb_active_part-1] = particle_array[p];
	particle_array[p] = swap;

	// std::cout << "\nAprès suppression" << std::endl;
	// std::cout << p << " :" << std::endl;
	// particle_array[p].print();
	// std::cout << nb_active_part-1 << " :" << std::endl;
	// particle_array[nb_active_part-1].print();

	nb_active_part--;
}

void Particle_simulator::delete_range(float x, float y, float range_) {
	// std::cout << "Particle_simulator::delete_range()" << std::endl;
	float vec[2], norm;
	for (uint32_t p=0; p<nb_active_part; p++) {
		vec[0] = x - particle_array[p].position[0];
		vec[1] = y - particle_array[p].position[1];
		norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);

		if (norm < range) {
			delete_particle(p);
			p--;
		}
	}
}


uint32_t Particle_simulator::create_particle(float x, float y) {
	// std::cout << "Particle_simulator::create_particle(), nb_actif=" << nb_active_part << std::endl;
	if (nb_active_part < NB_PART) {
		particle_array[nb_active_part].position[0] = x;
		particle_array[nb_active_part].position[1] = y;

		particle_array[nb_active_part].speed[0] = 0;
		particle_array[nb_active_part].speed[1] = 0;
		particle_array[nb_active_part].acceleration[0] = 0;
		particle_array[nb_active_part].acceleration[1] = 0;

		return nb_active_part++;

	} else {
		return NULLPART;
	}
}