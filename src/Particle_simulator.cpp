#include <SFML/Graphics/Color.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "Particle_simulator.hpp"
#include "Particle.hpp"


Particle_simulator::Particle_simulator() : world(1000, 1000, 1000/(4*radii), 1000/(4*radii)) {
	std::cout << "Particle_simulator::Particle_simulator()" << std::endl;
	// std::cout << "point gravity : " << grav_center[0] << ", " << grav_center[1] << std::endl;
	for (uint32_t i=0; i<NB_PART; i++) {
		particle_array[i].position[0] = rand()%1000;
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
	seg_array[0].pos[1][0] = 1000;
	seg_array[0].pos[1][1] = 0;

	seg_array[1].pos[0][0] = 1000;
	seg_array[1].pos[0][1] = 0;
	seg_array[1].pos[1][0] = 1000;
	seg_array[1].pos[1][1] = 1000;

	seg_array[2].pos[0][0] = 1000;
	seg_array[2].pos[0][1] = 1000;
	seg_array[2].pos[1][0] = 0;
	seg_array[2].pos[1][1] = 1000;

	seg_array[3].pos[0][0] = 0;
	seg_array[3].pos[0][1] = 1000;
	seg_array[3].pos[1][0] = 0;
	seg_array[3].pos[1][1] = 0;
}


void Particle_simulator::simu_step() {
	for (uint32_t i=0; i<NB_PART; i++) {
		particle_array[i].acceleration[0] = 0;
		particle_array[i].acceleration[1] = 0;
	}
	point_gravity();
	// gravity();
	fluid_friction();
	// static_friction();
	// collision_pp();
	world.update_grid_particle_contenance(particle_array, nb_active_part);
	collision_pp_grid();
	// collision_pl();
	solver();
}

void Particle_simulator::collision_pp() {
	float vec[2];
	float dist;
	float collision_coef = 0.45f;
	for (uint8_t i=0; i<4; i++) {
		for (uint32_t p1=0; p1<NB_PART; p1++) {
			for (uint32_t p2=p1+1; p2<NB_PART; p2++) {
				vec[0] = particle_array[p2].position[0] + particle_array[p2].speed[0]*dt - particle_array[p1].position[0] - particle_array[p1].speed[0]*dt;
				vec[1] = particle_array[p2].position[1] + particle_array[p2].speed[1]*dt - particle_array[p1].position[1] - particle_array[p1].speed[1]*dt;
				dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
				vec[0] /= dist;
				vec[1] /= dist;
	
				if (dist < 2*radii) {
					// particle_array[p1].acceleration[0] -= vec[0] * (2*radii - dist) /dt/dt *collision_coef;
					// particle_array[p1].acceleration[1] -= vec[1] * (2*radii - dist) /dt/dt *collision_coef;
					// particle_array[p2].acceleration[0] += vec[0] * (2*radii - dist) /dt/dt *collision_coef;
					// particle_array[p2].acceleration[1] += vec[1] * (2*radii - dist) /dt/dt *collision_coef;
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
												// particle_array[p1].acceleration[0] -= vec[0] * (2*radii - dist) /dt/dt *collision_coef;
												// particle_array[p1].acceleration[1] -= vec[1] * (2*radii - dist) /dt/dt *collision_coef;
												// particle_array[p2].acceleration[0] += vec[0] * (2*radii - dist) /dt/dt *collision_coef;
												// particle_array[p2].acceleration[1] += vec[1] * (2*radii - dist) /dt/dt *collision_coef;
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



void Particle_simulator::collision_pl() {
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

		for (uint32_t p=0; p<nb_active_part; p++) {
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



void Particle_simulator::gravity() {
	for (uint32_t i=0; i<NB_PART; i++) {
		particle_array[i].acceleration[1] += 1000;
	}
}

void Particle_simulator::point_gravity() {
	float vec[2];
	float dist;
	float temp;
	for (uint32_t i=0; i<NB_PART; i++) {
		vec[0] = grav_center[0] - particle_array[i].position[0];
		vec[1] = grav_center[1] - particle_array[i].position[1];
		dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
		temp = atan(dist) / dist;
		temp = temp * temp;
		vec[0] *= dist*temp;
		vec[1] *= dist*temp;

		particle_array[i].acceleration[0] += vec[0] * 1000;
		particle_array[i].acceleration[1] += vec[1] * 1000;
	}
}


void Particle_simulator::static_friction() {
	float vec[2];
	float norm;
	for (uint32_t i=0; i<NB_PART; i++) {
		vec[0] = particle_array[i].speed[0];
		vec[1] = particle_array[i].speed[1];
		norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
		particle_array[i].speed[0] = (norm<1) ? 0 : particle_array[i].speed[0];
		particle_array[i].speed[1] = (norm<1) ? 0 : particle_array[i].speed[1];
	}
}


void Particle_simulator::fluid_friction() {
	float vec[2];
	float norm;
	for (uint32_t i=0; i<NB_PART; i++) {
		particle_array[i].speed[0] *= 1-0.2f*dt;
		particle_array[i].speed[1] *= 1-0.2f*dt;
	}
}


void Particle_simulator::solver() {
	for (uint32_t i=0; i<NB_PART; i++) {
		// if (i==0) {
		// 	std::cout << "acceleration : " << particle_array[i].acceleration[0] << "," << particle_array[i].acceleration[1] << std::endl;
		// 	std::cout << "speed : " << particle_array[i].speed[0] << "," << particle_array[i].speed[1] << std::endl;
		// 	std::cout << "position : " << particle_array[i].position[0] << "," << particle_array[i].position[1] << std::endl;
		// 	std::cout << std::endl;
		// }
		particle_array[i].speed[0] += particle_array[i].acceleration[0] * dt;
		particle_array[i].speed[1] += particle_array[i].acceleration[1] * dt;

		particle_array[i].position[0] += particle_array[i].speed[0] * dt;
		particle_array[i].position[1] += particle_array[i].speed[1] * dt;
	}
}