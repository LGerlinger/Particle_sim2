#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <pthread.h>

#include "Particle_simulator.hpp"
#include "Consometer.hpp"
#include "Particle.hpp"
#include "World.hpp"


Particle_simulator::Particle_simulator() : world(1800, 1000, 2*radii, 2*radii) {
	std::cout << "Particle_simulator::Particle_simulator()" << std::endl;
	uint16_t x=0,y=0;
	for (uint32_t i=0; i<NB_PART; i++) {
		// particle_array[i].position[0] = x* world.getCellSize(0);
		// particle_array[i].position[1] = y* world.getCellSize(1);
		// if (++x==world.getGridSize(0)) {
		// 	x = 0;
		// 	y++;
		// }

		particle_array[i].position[0] = rand()%(uint32_t)(world.getSize(0) - 2*radii) + 2*radii;
		particle_array[i].position[1] = rand()%(uint32_t)(world.getSize(1) - 2*radii) + 2*radii;
		
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
	seg_array[0].pos[1][0] = world.getSize(0)-0.1;
	seg_array[0].pos[1][1] = 0;

	seg_array[1].pos[0][0] = seg_array[0].pos[1][0];
	seg_array[1].pos[0][1] = seg_array[0].pos[1][1];
	seg_array[1].pos[1][0] = world.getSize(0)-0.1;
	seg_array[1].pos[1][1] = world.getSize(1)-0.1;

	seg_array[2].pos[0][0] = seg_array[1].pos[1][0];
	seg_array[2].pos[0][1] = seg_array[1].pos[1][1];
	seg_array[2].pos[1][0] = 0;
	seg_array[2].pos[1][1] = world.getSize(1)-0.1;

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

	world.update_grid_segment_contenance(seg_array, nb_active_seg);
	// std::cout << "fin Particle_simulator::Particle_simulator()" << std::endl;
}


Particle_simulator::~Particle_simulator() {
	std::cout << "Particle_simulator::~Particle_simulator()" << std::endl;
	simulate = false;
	if (threads_created) {
		stop_simulation_threads();
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
		delete(thread_list[i]);
		thread_list[i] = nullptr;
	}
	threads_created = false;
}


void Particle_simulator::simulation_thread(uint8_t th_id) {
	// std::cout << "Particle_simulator::simulation_thread()" << std::endl;
	uint32_t nppt; // number of particles per thread
	uint32_t p_start, p_end; // indices of the [first, last[ Particle of the thread.
	if (!th_id) {
		conso.start_perf_check("simulateur", 10000);
		conso2.start_perf_check("update world grid", 10000);
	}
	while (simulate) {
		// Synchronisation
		pthread_mutex_lock(&sync_mutex);
		sync_count++;
		if (sync_count == MAX_THREAD_NUM) { // last thread to reach synchronisation stop
			sync_count = 0;

			// conso2.Start();
			// world.update_grid_particle_contenance(particle_array, nb_active_part, dt);
			// conso2.Tick_fine();

			conso.Tick_fine();
			while (simulate && paused && !step && !quickstep) {
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
			step = false;
			if (quickstep) std::this_thread::sleep_for(std::chrono::milliseconds(50));
			conso.Start();

			// This part is executed by only one thread while others wait
			{
				time[0] += dt;
	
				if (deletion_order) {
					delete_range(user_point[0], user_point[1], range);
				}
				if (0.01f * radii < time[0] - time[1]) {
					time[1] = time[0];
					uint32_t pa = create_particle(world.getSize(0)/2-200, 100);
					uint32_t pb = create_particle(world.getSize(0)/2+200, 100);

					if (pa != NULLPART) {
						particle_array[pa].speed[0] =  800 * cos(-2*time[0]);
						particle_array[pa].speed[1] =  800 * sin(-2*time[0]);
					}
					if (pb != NULLPART) {
						particle_array[pb].speed[0] = -800 * cos(-2*time[0]);
						particle_array[pb].speed[1] =  800 * sin(-2*time[0]);
					}
				}
				conso2.Start();
				world.update_grid_particle_contenance(particle_array, nb_active_part, dt);
				conso2.Tick_fine();
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
		// vibrate(p_start, p_end);

		// updating speed
		for (uint32_t p=p_start; p<p_end; p++) {
			particle_array[p].speed[0] += particle_array[p].acceleration[0] * dt;
			particle_array[p].speed[1] += particle_array[p].acceleration[1] * dt;
		}

		// collision_pp(p_start, p_end);
		collision_pp_grid_threaded(p_start, p_end);
		// collision_pp_glue(p_start, p_end);

		collision_pl_grid(p_start, p_end);
		world_borders(p_start, p_end);

		// Simulation -- displacement
		// solver(p_start, p_end);

		// updating position
		for (uint32_t p=p_start; p<p_end; p++) {
			particle_array[p].position[0] += particle_array[p].speed[0] * dt;
			particle_array[p].position[1] += particle_array[p].speed[1] * dt;
		}
	}
}


void Particle_simulator::simu_step() {
	// std::cout << "Particle_simulator::simu_step()" << std::endl;
	for (uint32_t i=0; i<nb_active_part; i++) {
		particle_array[i].acceleration[0] = 0;
		particle_array[i].acceleration[1] = 0;
	}

	switch (appliedForce) {
		case userForce::Translation:
			attraction(0, nb_active_part);
			break;
		case userForce::Translation_ranged:
			attraction_ranged(0, nb_active_part);
			break;
		case userForce::Rotation:
			rotation(0, nb_active_part);
			break;
		case userForce::Rotation_ranged:
			rotation_ranged(0, nb_active_part);
			break;
		case userForce::Vortex:
			vortex(0, nb_active_part);
			break;
		case userForce::Vortex_ranged:
			vortex_ranged(0, nb_active_part);
			break;
		case userForce::None:
			break;
	}

	// gravity(0, nb_active_part);
	point_gravity(0, nb_active_part);

	world.update_grid_particle_contenance(particle_array, nb_active_part, dt);
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
		for (uint16_t y=0; y<world.getGridSize(1); y++) {
			for (uint16_t x=0; x<world.getGridSize(0); x++) {
				for (uint16_t p1=0; p1<world.getCell(x, y).nb_parts; p1++) {
					part1 = world.getCell(x, y).parts[p1];
	
					for (int8_t dy=0; dy<2; dy++) {
						dPos[1] = y + dy;
						if (dPos[1] < world.getGridSize(1)) {
							for (int8_t dx=-dy; dx<2; dx++) {
								dPos[0] = x + dx;
								if (dPos[0] < world.getGridSize(0)) {
									
									for (uint16_t p2=0; p2<world.getCell(dPos[0], dPos[1]).nb_parts; p2++) {
										part2 = world.getCell(dPos[0], dPos[1]).parts[p2];
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
	uint16_t dPos[2];
	float vec[2];
	float dist;
	float collision_coef = 0.45f;
	float temp_coef;
	uint32_t part2;
	uint16_t x, y;
	int8_t min_x, max_x, min_y, max_y;
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		
		// std::cout << "p_start=" << p_start << "    p1=" << p1 << "   p_end=" << p_end << std::endl;
		x = particle_array[p1].position[0] / world.getCellSize(0);
		y = particle_array[p1].position[1] / world.getCellSize(1);
		min_x = x==0 ? 0 : -1;
		min_y = y==0 ? 0 : -1;
		max_x = x>world.getGridSize(0)-2 ? (x == world.getGridSize(0)-1 ? 1 : -1) : 2;
		max_y = y>world.getGridSize(1)-2 ? (y == world.getGridSize(1)-1 ? 1 : -1) : 2;
		for (int8_t dy=min_y; dy<max_y; dy++) {
			dPos[1] = y + dy;
			for (int8_t dx=min_x; dx<max_x; dx++) {
				dPos[0] = x + dx;
				
				for (uint16_t p2=0; p2<world.getCell(dPos[0], dPos[1]).nb_parts; p2++) {
					part2 = world.getCell(dPos[0], dPos[1]).parts[p2];
					if (p1 != part2) {

						vec[0] = particle_array[part2].position[0] + particle_array[part2].speed[0]*dt - particle_array[p1].position[0] - particle_array[p1].speed[0]*dt;
						vec[1] = particle_array[part2].position[1] + particle_array[part2].speed[1]*dt - particle_array[p1].position[1] - particle_array[p1].speed[1]*dt;
						dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
						temp_coef = (2*radii - dist) * collision_coef /(dt * dist);
						vec[0] *= temp_coef;
						vec[1] *= temp_coef;
			
						if (dist < 2*radii) {
							particle_array[p1].speed[0] -= vec[0];
							particle_array[p1].speed[1] -= vec[1];
							particle_array[part2].speed[0] += vec[0];
							particle_array[part2].speed[1] += vec[1];
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

void Particle_simulator::collision_pl_grid(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::collision_pl_grid()" << std::endl;
	float vec[2];
	float AB[2], AC[2];
	float AB_dim[2];
	float normABsq;
	float temp;
	float min[2], max[2];
	float part_next_pos[2];
	uint32_t list_of_seg_met[9]; // 9 is below what could be found but generally that won't ever get close to that.
	uint8_t met_seg;
	uint16_t x, y;
	for (uint32_t p=p_start; p<p_end; p++) {
		met_seg = 0;

		x = particle_array[p].position[0] / world.getCellSize(0);
		y = particle_array[p].position[1] / world.getCellSize(1);

		// iterating over all 9 cells around the Particle
		if (x < world.getGridSize(0) && y < world.getGridSize(1)) {
			Cell& cell = world.getCell(x, y);
			for (uint8_t seg=0; seg<cell.nb_segs; seg++) {
				uint32_t s = cell.segs[seg];
				
				// checking if we aren't checking the same segment multiple times
				bool never_met = true;
				for (uint8_t i=0; i<met_seg && never_met; i++) {
					never_met = list_of_seg_met[i] != s;
				}
				if (never_met) {
					list_of_seg_met[met_seg] = s;
					met_seg = met_seg+1 % 9;
				} else break;

				AB[0] = seg_array[s].pos[1][0] - seg_array[s].pos[0][0];
				AB[1] = seg_array[s].pos[1][1] - seg_array[s].pos[0][1];
				normABsq = AB[0]*AB[0] + AB[1]*AB[1];
				AB_dim[0] = AB[0]/normABsq;
				AB_dim[1] = AB[1]/normABsq;
				
				min[0] = std::min(seg_array[s].pos[0][0], seg_array[s].pos[1][0]);
				max[0] = std::max(seg_array[s].pos[0][0], seg_array[s].pos[1][0]);
				min[1] = std::min(seg_array[s].pos[0][1], seg_array[s].pos[1][1]);
				max[1] = std::max(seg_array[s].pos[0][1], seg_array[s].pos[1][1]);

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
						if (temp==0) break;
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
							if (temp==0) break;
							particle_array[p].speed[0] -= vec[0] * (radii - temp) /dt;
							particle_array[p].speed[1] -= vec[1] * (radii - temp) /dt;
						}
					}
				}
			}
		}
	}
}



void Particle_simulator::collision_pp_glue(uint32_t p_start, uint32_t p_end) {
	uint16_t dPos[2];
	float vec[2];
	float dist;
	float collision_coef = 0.45f;
	float force_coef = .2f;
	float temp_coef;
	uint32_t part2;
	uint16_t x, y;
	int8_t min_x, max_x, min_y, max_y;
	uint8_t cs = 2; // check_size : how far (in cells) the particle while check for other particles
	for (uint32_t p1=p_start; p1<p_end; p1++) {

		x = particle_array[p1].position[0] / world.getCellSize(0);
		y = particle_array[p1].position[1] / world.getCellSize(1);
		min_x = x<cs ? -x : -cs;
		max_x = x>world.getGridSize(0)-1 -cs ? (x >= world.getGridSize(0) ? -cs : world.getGridSize(0) -x) : cs;
		min_y = y<cs ? -y : -cs;
		max_y = y>world.getGridSize(1)-1 -cs ? (y >= world.getGridSize(1) ? -cs : world.getGridSize(1) -y) : cs;
		for (int8_t dy=min_y; dy<max_y; dy++) {
			dPos[1] = y + dy;
			for (int8_t dx=min_x; dx<max_x; dx++) {
				dPos[0] = x + dx;
				
				for (uint16_t p2=0; p2<world.getCell(dPos[0], dPos[1]).nb_parts; p2++) {
					part2 = world.getCell(dPos[0], dPos[1]).parts[p2];
					if (p1 != part2) {

						vec[0] = particle_array[part2].position[0] + particle_array[part2].speed[0]*dt - particle_array[p1].position[0] - particle_array[p1].speed[0]*dt;
						vec[1] = particle_array[part2].position[1] + particle_array[part2].speed[1]*dt - particle_array[p1].position[1] - particle_array[p1].speed[1]*dt;
						dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
						temp_coef = (2*radii - dist) * collision_coef /(dt * dist);
						temp_coef = dist < 2*radii ? temp_coef*collision_coef : temp_coef * 0.001f;
						vec[0] *= temp_coef;
						vec[1] *= temp_coef;
			
						particle_array[p1].speed[0] -= vec[0];
						particle_array[p1].speed[1] -= vec[1];
						particle_array[part2].speed[0] += vec[0];
						particle_array[part2].speed[1] += vec[1];
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
				particle_array[p1].speed[i] += (radii - vec[i]) /dt;
			} else if (world.getSize(i) - radii < vec[i]) {
				particle_array[p1].speed[i] -= (vec[i] - (world.getSize(i) - radii)) /dt;
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

void Particle_simulator::vibrate(uint32_t p_start, uint32_t p_end) {
	int32_t max = 20000;
	for (uint32_t p=p_start; p<p_end; p++) {
		particle_array[p].acceleration[0] += sin(60*(time[0] +p)) * max;
		particle_array[p].acceleration[1] += (p%2 ? 1 : -1)*cos(60*(time[0] +p)) * max;
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

	swap = particle_array[nb_active_part-1]; // Their order doesn't matter
	particle_array[nb_active_part-1] = particle_array[p];
	particle_array[p] = swap;

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